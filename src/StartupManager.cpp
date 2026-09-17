#include "StartupManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QString>

namespace {
/// Windows per-user "run at sign-in" key. Writing here needs no elevation.
const QLatin1String kRunKey(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
/// The value name under that key; must match on read, write and delete.
const QLatin1String kValueName("NextGen Tweaks");

/// Native, quoted path to this executable so a path with spaces still launches.
QString quotedExecutablePath()
{
    return QLatin1Char('"')
           + QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
           + QLatin1Char('"');
}
} // namespace

bool StartupManager::isEnabled()
{
#ifdef Q_OS_WIN
    const QSettings run(kRunKey, QSettings::NativeFormat);
    const QString stored = run.value(kValueName).toString().trimmed();
    if (stored.isEmpty())
        return false;

    // Consider it "on" only when the entry still points at *this* build, so a
    // moved or reinstalled app reports honestly instead of showing a stale tick.
    const QString expected = quotedExecutablePath();
    return stored.compare(expected, Qt::CaseInsensitive) == 0;
#else
    return false;
#endif
}

bool StartupManager::setEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (enabled)
        run.setValue(kValueName, quotedExecutablePath());
    else
        run.remove(kValueName);
    run.sync();
    return run.status() == QSettings::NoError;
#else
    Q_UNUSED(enabled)
    return false;
#endif
}
