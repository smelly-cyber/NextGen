#include "SectionPage.h"

#include "ConfirmChangesDialog.h"
#include "MessageDialog.h"

#include <QThread>
#include <QProcess>
#include <QTimer>
#include "ModernButton.h"
#include "ElevationHelper.h"
#include "OptionRow.h"
#include "Theme.h"
#include "Tweak.h"
#include "TweakBackupStore.h"
#include "TweakEngine.h"

#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>

SectionPage::SectionPage(QWidget *parent)
    : QWidget(parent)
{
    m_statusColor = Theme::colors().link;
    m_idleColor = m_statusColor;
}

void SectionPage::setStatus(const QString &text, const QColor &color)
{
    m_statusText = text;
    m_statusColor = color;

    // Remember the resting status so it can be restored after a run.
    if (!m_running) {
        m_idleStatus = text;
        m_idleColor = color;
    }

    emit statusChanged(m_statusText, m_statusColor);
}

void SectionPage::bindOption(OptionRow *row, const QString &tweakId)
{
    if (!row || tweakId.isEmpty())
        return;
    m_boundOptions.insert(row, tweakId);
    m_optionOrder.append(row);
}

QStringList SectionPage::selectedTweakIds() const
{
    QStringList ids;
    // Keep the order the rows appear in, so the preview reads top to bottom.
    for (OptionRow *row : m_optionOrder) {
        if (row->isOptionEnabled() && row->isEnabled())
            ids << m_boundOptions.value(row);
    }
    return ids;
}

void SectionPage::syncOptionsFromMachine()
{
    TweakEngine *engine = TweakEngine::instance();

    // Reading whether each tweak is already applied means reading the registry -
    // now hundreds of values across a page's options. Doing that on the UI
    // thread is what made switching to a tab freeze for a moment. So the rows
    // are made usable immediately, and the "is it already on?" check runs on a
    // background thread and ticks the switches on when it finishes.
    QList<QPair<OptionRow *, QString>> toCheck;
    for (OptionRow *row : m_optionOrder) {
        const QString id = m_boundOptions.value(row);
        if (!engine->tweak(id))
            continue;
        // Every option stays freely selectable straight away.
        row->setEnabled(true);
        toCheck.append({row, id});
    }
    applyAvailability();

    if (toCheck.isEmpty())
        return;

    // Guard so a rapid re-entry (page shown again before the last check ended)
    // does not pile up worker threads.
    if (m_syncing)
        return;
    m_syncing = true;

    auto *worker = QThread::create([this, engine, toCheck] {
        for (const auto &entry : toCheck) {
            Tweak *tweak = engine->tweak(entry.second);
            const bool applied = tweak && tweak->isApplied();
            OptionRow *row = entry.first;
            // Apply the result back on the UI thread.
            QMetaObject::invokeMethod(
                this,
                [row, applied] {
                    if (applied)
                        row->setOptionEnabled(true);
                },
                Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, [this] { m_syncing = false; }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void SectionPage::showResultDialog(bool allSucceeded)
{
    const QString title = m_runActionTitle.isEmpty() ? tr("Optimisation") : m_runActionTitle;

    if (allSucceeded && m_runErrors.isEmpty()) {
        MessageDialog::information(
            window(), title, tr("Changes Applied Successfully"),
            m_runIsRevert
                ? tr("Your settings have been restored to how they were before.")
                : tr("All selected optimisations were applied. Every change was recorded "
                     "and can be reverted at any time from Settings."),
            MessageDialog::Tone::Success);
    } else {
        // Something failed - name what, and show why.
        MessageDialog::information(
            window(), title, tr("Unsuccessful"),
            (m_runErrors.isEmpty()
                 ? tr("The changes could not be applied.")
                 : tr("Some changes could not be applied:") + QStringLiteral("<br><br>")
                       + m_runErrors.join(QStringLiteral("<br><br>"))),
            MessageDialog::Tone::Danger);
    }

    // A separate prompt, once the result has been acknowledged, if any applied
    // setting only takes effect after a restart.
    maybePromptReboot();
}

void SectionPage::maybePromptReboot()
{
    if (!m_runNeedsReboot)
        return;
    m_runNeedsReboot = false;

    MessageDialog dialog(window(), tr("Restart Required"));
    dialog.setTone(MessageDialog::Tone::Warning);
    dialog.setHeading(tr("Some of these changes require a reboot"));
    dialog.setBody(tr("A few settings only take effect after your PC restarts. You can keep "
                      "applying optimisations in the other tabs and restart when you are done."));
    dialog.addButton(tr("Reboot Later"), ModernButton::Variant::Secondary, 0);
    dialog.addButton(tr("Reboot Now"), ModernButton::Variant::Primary, 1, true);
    if (dialog.run() == 1)
        rebootNow();
}

void SectionPage::rebootNow()
{
    // The app runs elevated, so shutdown.exe can restart immediately. /t 0 = now,
    // /r = reboot. Detached so it keeps going as this process is torn down.
    QProcess::startDetached(QStringLiteral("shutdown"),
                            {QStringLiteral("/r"), QStringLiteral("/t"), QStringLiteral("0")});
}

void SectionPage::applyAvailability()
{
    TweakEngine *engine = TweakEngine::instance();

    for (OptionRow *row : m_optionOrder) {
        const Tweak *tweak = engine->tweak(m_boundOptions.value(row));
        if (!tweak || tweak->isAvailable())
            continue;
        // The one thing that does grey a toggle out: hardware it needs is not
        // in this PC (an NVIDIA-only tweak on an AMD or Intel machine). It is
        // also forced off so a run can never select it.
        row->setOptionEnabled(false);
        row->setEnabled(false);
        row->setToolTip(tweak->unavailableReason());
    }
}

void SectionPage::pageShown()
{
    connectEngine();
    syncOptionsFromMachine();
}

void SectionPage::connectEngine()
{
    if (m_engineConnected)
        return;
    m_engineConnected = true;

    TweakEngine *engine = TweakEngine::instance();

    connect(engine, &TweakEngine::busyChanged, this, &SectionPage::handleBusyChanged);

    connect(engine, &TweakEngine::progress, this, [this](int percent, const QString &step) {
        if (!m_running)
            return;
        Q_UNUSED(percent)
        setStatus(step, Theme::colors().link);
    });

    connect(engine, &TweakEngine::tweakFinished, this,
            [this, engine](const QString &id, bool ok, const QString &message) {
                if (!m_running || ok)
                    return;
                // Remember each failure (tweak name + reason) for the result
                // dialog, and mirror the first line to the status strip.
                const QString name = engine->tweak(id) ? engine->tweak(id)->title() : id;
                const QString reason = message.isEmpty() ? tr("Failed") : message;
                m_runErrors << QStringLiteral("<b>%1</b><br>%2")
                                   .arg(name.toHtmlEscaped(),
                                        reason.section(QLatin1Char('\n'), 0, 0).toHtmlEscaped());
                setStatus(reason.section(QLatin1Char('\n'), 0, 0), Theme::colors().danger);
            });

    connect(engine, &TweakEngine::finished, this,
            [this](bool allSucceeded, const QString &summary) {
                if (!m_awaitingResult)
                    return; // A run this page did not start.
                m_awaitingResult = false;
                setStatus(summary, allSucceeded ? Theme::colors().success
                                                : Theme::colors().danger);
                syncOptionsFromMachine();
                // Deferred so the modal dialog opens after the engine's own
                // busyChanged() has finished re-enabling the controls.
                QTimer::singleShot(0, this, [this, allSucceeded] { showResultDialog(allSucceeded); });
            });
}

void SectionPage::handleBusyChanged(bool busy)
{
    if (!m_running && busy)
        return; // Another page started the run.

    m_running = busy;
    runStateChanged(busy);
    // runStateChanged() re-enables every row when a run ends.
    if (!busy)
        applyAvailability();

    if (!busy) {
        // Leave the summary on screen; the next page visit resets it.
        m_running = false;
    }
}

void SectionPage::runSelectedTweaks(const QString &actionTitle, const QString &subtitle)
{
    TweakEngine *engine = TweakEngine::instance();
    if (engine->isBusy())
        return;

    const QStringList ids = selectedTweakIds();
    if (ids.isEmpty()) {
        setStatus(tr("Nothing selected - switch on at least one option."),
                  Theme::colors().danger);
        return;
    }

    // The confirmation lists the optimisations by name, not the raw registry
    // paths - the user wants a clean "these are the changes", not a log.
    QStringList summary;
    for (const QString &id : ids) {
        if (Tweak *tweak = engine->tweak(id))
            summary << QStringLiteral("•  %1").arg(tweak->title());
    }
    const bool needsAdmin = engine->requiresAdmin(ids);
    const bool elevated = ElevationHelper::isElevated();

    ConfirmChangesDialog dialog(actionTitle, subtitle, summary, window());
    dialog.setNeedsAdmin(needsAdmin, elevated);
    dialog.setNeedsReboot(engine->requiresReboot(ids));
    dialog.setDestructive(ids.contains(QLatin1String("optimise.cleanTemp")));

    if (dialog.exec() != QDialog::Accepted)
        return;

    if (dialog.elevationRequested()) {
        emit elevationRequested();
        return;
    }

    m_runErrors.clear();
    m_runActionTitle = actionTitle;
    m_runIsRevert = false;
    m_runNeedsReboot = engine->requiresReboot(ids);
    m_awaitingResult = true;
    m_running = true;
    runStateChanged(true);
    engine->applyTweaks(ids);
}

void SectionPage::revertAllTweaks()
{
    TweakEngine *engine = TweakEngine::instance();
    if (engine->isBusy())
        return;

    const QStringList ids = engine->revertableIds();
    if (ids.isEmpty()) {
        setStatus(tr("Nothing to revert - no changes have been applied."),
                  Theme::colors().textSecondary);
        return;
    }

    QStringList preview;
    for (const QString &id : ids) {
        Tweak *tweak = engine->tweak(id);
        if (!tweak)
            continue;
        preview << QStringLiteral("• %1").arg(tweak->title());
        preview << QStringLiteral("      %1")
                       .arg(tr("applied %1")
                                .arg(TweakBackupStore::instance()->appliedAt(id)));
    }

    ConfirmChangesDialog dialog(
        tr("Revert All Changes"),
        tr("Every setting Nextgen Tweaks changed will be written back to the value it had "
           "before. Deleted temporary files cannot be brought back."),
        preview, window());
    dialog.setNeedsAdmin(engine->requiresAdmin(ids), ElevationHelper::isElevated());
    dialog.setConfirmText(tr("Revert Everything"));

    if (dialog.exec() != QDialog::Accepted)
        return;

    if (dialog.elevationRequested()) {
        emit elevationRequested();
        return;
    }

    m_runErrors.clear();
    m_runActionTitle = tr("Revert Changes");
    m_runIsRevert = true;
    m_runNeedsReboot = false;
    m_awaitingResult = true;
    m_running = true;
    runStateChanged(true);
    engine->revertTweaks(ids);
}

void SectionPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF body(rect());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient base(body.topLeft(), body.bottomRight());
    base.setColorAt(0.0, c.formPanelTop);
    base.setColorAt(1.0, c.formPanelBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(body);

    QRadialGradient bloom(QPointF(body.width() * 0.88, body.height() * 0.04),
                          qMax(body.width(), body.height()) * 0.75);
    bloom.setColorAt(0.0, Theme::alpha(c.primary, 22));
    bloom.setColorAt(1.0, Qt::transparent);
    painter.setBrush(bloom);
    painter.drawRect(body);
}
