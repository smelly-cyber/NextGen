#include "Tweak.h"

#include "TweakBackupStore.h"

#include <QObject>
#include <QSettings>

namespace {

QString describe(const QVariant &value)
{
    if (!value.isValid())
        return QObject::tr("(not set)");
    return value.toString();
}

} // namespace

Tweak::Tweak(QString id, QString title, QString description, Scope scope)
    : m_id(std::move(id))
    , m_title(std::move(title))
    , m_description(std::move(description))
    , m_scope(scope)
{
}

Tweak::~Tweak() = default;

// ---------------------------------------------------------- RegistryTweak --

RegistryTweak::RegistryTweak(QString id, QString title, QString description, Scope scope,
                             QVector<RegistryEntry> entries)
    : Tweak(std::move(id), std::move(title), std::move(description), scope)
    , m_entries(std::move(entries))
{
}

QStringList RegistryTweak::plannedChanges() const
{
    QStringList lines;
    for (const RegistryEntry &entry : m_entries) {
        const QSettings settings(entry.path, QSettings::NativeFormat);
        const QVariant current = settings.value(entry.valueName);

        lines << QObject::tr("%1\\%2:  %3  →  %4")
                     .arg(entry.path, entry.valueName, describe(current),
                          describe(entry.optimised));
    }
    return lines;
}

bool RegistryTweak::isApplied() const
{
    for (const RegistryEntry &entry : m_entries) {
        const QSettings settings(entry.path, QSettings::NativeFormat);
        const QVariant current = settings.value(entry.valueName);
        if (!current.isValid())
            return false;
        // Compare as strings so REG_DWORD vs REG_SZ storage does not matter.
        if (current.toString() != entry.optimised.toString())
            return false;
    }
    return true;
}

bool RegistryTweak::apply(TweakBackupStore &store, QString *error)
{
    for (const RegistryEntry &entry : m_entries) {
        QSettings settings(entry.path, QSettings::NativeFormat);

        // Some values Windows locks down even from an elevated administrator -
        // a handful of Explorer\Advanced values on Windows 11, for instance,
        // return "access denied" no matter what. Those are skipped quietly: a
        // value we are not allowed to touch is not a failure of the app, so it
        // is not reported. A key that simply is not writable (no elevation) is
        // treated the same way here - the confirmation dialog already offers to
        // relaunch elevated before a machine-level tweak ever runs.
        if (!settings.isWritable())
            continue;

        // Read the current value first (this is the "way back"), but only commit
        // that backup once the write has actually succeeded below - so a locked
        // value we could not change never leaves a phantom entry to revert.
        const QString slot = entry.path + QLatin1Char('\\') + entry.valueName;
        const QVariant current = settings.value(entry.valueName);

        settings.setValue(entry.valueName, entry.optimised);
        settings.sync();

        if (settings.status() == QSettings::AccessError)
            continue; // Locked by Windows - skip quietly, do not report.
        if (settings.status() != QSettings::NoError) {
            if (error)
                *error = QObject::tr("Failed to write %1\\%2.").arg(entry.path, entry.valueName);
            return false;
        }

        if (!store.hasBackup(id(), slot))
            store.record(id(), slot, current.isValid() ? current : entry.windowsDefault,
                         current.isValid());
    }
    return true;
}

bool RegistryTweak::revert(TweakBackupStore &store, QString *error)
{
    for (const RegistryEntry &entry : m_entries) {
        const QString slot = entry.path + QLatin1Char('\\') + entry.valueName;
        if (!store.hasBackup(id(), slot))
            continue;

        QSettings settings(entry.path, QSettings::NativeFormat);
        if (!settings.isWritable()) {
            if (error) {
                *error = QObject::tr("No permission to restore %1. Administrator rights are "
                                     "required.")
                             .arg(entry.path);
            }
            return false;
        }

        if (store.previousValueExisted(id(), slot))
            settings.setValue(entry.valueName, store.previousValue(id(), slot));
        else
            settings.remove(entry.valueName);

        settings.sync();
    }
    return true;
}

// --------------------------------------------------------- CompositeTweak --

CompositeTweak::CompositeTweak(QString id, QString title, QString description, Scope scope,
                               QVector<Tweak *> parts)
    : Tweak(std::move(id), std::move(title), std::move(description), scope)
    , m_parts(std::move(parts))
{
    // A group needs a reboot if any single part of it does.
    for (const Tweak *part : m_parts) {
        if (part->requiresReboot()) {
            setRequiresReboot(true);
            break;
        }
    }
}

CompositeTweak::~CompositeTweak()
{
    qDeleteAll(m_parts);
}

QStringList CompositeTweak::plannedChanges() const
{
    QStringList lines;
    for (const Tweak *part : m_parts)
        lines << part->plannedChanges();
    return lines;
}

bool CompositeTweak::isApplied() const
{
    for (const Tweak *part : m_parts) {
        if (!part->isApplied())
            return false;
    }
    return !m_parts.isEmpty();
}

bool CompositeTweak::apply(TweakBackupStore &store, QString *error)
{
    for (Tweak *part : m_parts) {
        if (!part->apply(store, error))
            return false;
    }
    return true;
}

bool CompositeTweak::revert(TweakBackupStore &store, QString *error)
{
    // Every part is attempted even if one fails, so a single stubborn setting
    // cannot leave the rest of the group applied.
    bool allOk = true;
    QString firstError;
    for (Tweak *part : m_parts) {
        QString partError;
        if (!part->revert(store, &partError)) {
            allOk = false;
            if (firstError.isEmpty())
                firstError = partError;
        }
    }
    if (!allOk && error)
        *error = firstError;
    return allOk;
}
