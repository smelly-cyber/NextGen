#include "TweakEngine.h"

#include "ElevationHelper.h"
#include "RestorePointService.h"
#include "Tweak.h"
#include "TweakBackupStore.h"
#include "TweakCatalogue.h"

#include <QMetaObject>
#include <QThread>
#include <QTimer>

TweakEngine::TweakEngine(QObject *parent)
    : QObject(parent)
{
    m_tweaks = TweakCatalogue::createAll();
}

TweakEngine::~TweakEngine()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
    qDeleteAll(m_tweaks);
}

TweakEngine *TweakEngine::instance()
{
    static TweakEngine engine;
    return &engine;
}

Tweak *TweakEngine::tweak(const QString &id) const
{
    for (Tweak *t : m_tweaks) {
        if (t->id() == id)
            return t;
    }
    return nullptr;
}

TweakStatus TweakEngine::status(const QString &id) const
{
    TweakStatus result;
    Tweak *t = tweak(id);
    if (!t)
        return result;

    result.id = t->id();
    result.title = t->title();
    result.description = t->description();
    result.requiresAdmin = t->requiresAdmin();
    result.requiresReboot = t->requiresReboot();
    result.recorded = TweakBackupStore::instance()->isRecorded(id);
    result.applied = t->isApplied();
    return result;
}

QStringList TweakEngine::previewFor(const QStringList &ids) const
{
    QStringList lines;
    for (const QString &id : ids) {
        Tweak *t = tweak(id);
        if (!t)
            continue;

        lines << QStringLiteral("• %1").arg(t->title());
        for (const QString &change : t->plannedChanges())
            lines << QStringLiteral("      %1").arg(change);
    }
    return lines;
}

bool TweakEngine::requiresAdmin(const QStringList &ids) const
{
    for (const QString &id : ids) {
        Tweak *t = tweak(id);
        if (t && t->requiresAdmin())
            return true;
    }
    return false;
}

bool TweakEngine::requiresReboot(const QStringList &ids) const
{
    for (const QString &id : ids) {
        Tweak *t = tweak(id);
        if (t && t->requiresReboot())
            return true;
    }
    return false;
}

QStringList TweakEngine::revertableIds() const
{
    QStringList ids;
    const QStringList recorded = TweakBackupStore::instance()->appliedTweaks();
    for (Tweak *t : m_tweaks) {
        if (recorded.contains(t->id()))
            ids << t->id();
    }
    return ids;
}

void TweakEngine::cancel()
{
    m_cancelRequested = true;
}

void TweakEngine::applyTweaks(const QStringList &ids)
{
    runJob(ids, false);
}

void TweakEngine::revertTweaks(const QStringList &ids)
{
    runJob(ids, true);
}

void TweakEngine::runJob(const QStringList &ids, bool reverting)
{
    if (m_busy || ids.isEmpty())
        return;

    m_busy = true;
    m_cancelRequested = false;
    emit busyChanged(true);

    // The tweaks themselves are owned by this object on the main thread, but
    // they only touch the registry / child processes, which is thread safe for
    // our purposes. The worker never touches a widget.
    m_thread = QThread::create([this, ids, reverting] {
        TweakBackupStore &store = *TweakBackupStore::instance();

        int failures = 0;
        int completed = 0;
        const int total = ids.size() + 1; // +1 for the restore point step

        // --- Safety net first -------------------------------------------------
        if (!reverting && requiresAdmin(ids) && ElevationHelper::isElevated()) {
            QMetaObject::invokeMethod(this, [this] {
                emit progress(2, tr("Creating a system restore point..."));
            }, Qt::QueuedConnection);

            QString restoreError;
            if (!RestorePointService::create(tr("Before Nextgen Tweaks optimisation"),
                                             &restoreError)) {
                const QString message = restoreError;
                QMetaObject::invokeMethod(this, [this, message] {
                    emit tweakFinished(QStringLiteral("restorePoint"), false, message);
                }, Qt::QueuedConnection);
            }
        }
        ++completed;

        // --- Then the tweaks themselves ---------------------------------------
        for (const QString &id : ids) {
            if (m_cancelRequested)
                break;

            Tweak *t = tweak(id);
            if (!t) {
                ++completed;
                continue;
            }

            const QString title = t->title();
            const int percent = int(100.0 * completed / total);
            QMetaObject::invokeMethod(this, [this, percent, title, reverting] {
                emit progress(percent, reverting ? tr("Restoring %1...").arg(title)
                                                 : tr("Applying %1...").arg(title));
            }, Qt::QueuedConnection);

            QString error;
            const bool ok = reverting ? t->revert(store, &error) : t->apply(store, &error);
            if (ok && reverting)
                store.clearTweak(id);
            if (!ok)
                ++failures;

            QMetaObject::invokeMethod(this, [this, id, ok, error] {
                emit tweakFinished(id, ok, error);
            }, Qt::QueuedConnection);

            ++completed;
        }

        const bool cancelled = m_cancelRequested;
        const int failed = failures;
        const int applied = ids.size() - failures;

        QMetaObject::invokeMethod(this, [this, cancelled, failed, applied, reverting] {
            QString summary;
            if (cancelled)
                summary = tr("Cancelled after %n change(s).", nullptr, applied);
            else if (failed == 0)
                summary = reverting ? tr("Restored %n setting(s).", nullptr, applied)
                                    : tr("Applied %n change(s).", nullptr, applied);
            else
                summary = tr("%1 applied, %2 skipped.").arg(applied).arg(failed);

            emit progress(100, summary);
            m_busy = false;
            emit busyChanged(false);
            emit finished(failed == 0 && !cancelled, summary);
        }, Qt::QueuedConnection);
    });

    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QThread::destroyed, this, [this] { m_thread = nullptr; });
    m_thread->start();
}
