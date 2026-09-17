#include "RestorePointService.h"

#include "ElevationHelper.h"

#include <QObject>
#include <QProcess>
#include <QSettings>

namespace RestorePointService {

namespace {

/// Windows rate-limits restore points to one per 24h by default; this policy
/// value is what controls it. Reading it lets us explain a silent no-op.
const QLatin1String kSystemRestoreKey(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore");

} // namespace

bool isAvailable()
{
    const QSettings settings(kSystemRestoreKey, QSettings::NativeFormat);
    // DisableSR = 1 means System Protection is switched off entirely.
    return settings.value(QStringLiteral("DisableSR"), 0).toInt() == 0;
}

bool create(const QString &description, QString *error)
{
    if (!ElevationHelper::isElevated()) {
        if (error)
            *error = QObject::tr("A restore point needs administrator rights.");
        return false;
    }

    if (!isAvailable()) {
        if (error) {
            *error = QObject::tr("System Protection is turned off, so no restore point could be "
                                 "created. You can enable it in System Properties > System "
                                 "Protection.");
        }
        return false;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral("powershell"),
                  {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                   QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                   QStringLiteral("-Command"),
                   QStringLiteral("Checkpoint-Computer -Description '%1' "
                                  "-RestorePointType MODIFY_SETTINGS")
                       .arg(QString(description).replace(QLatin1Char('\''), QLatin1Char(' ')))});

    if (!process.waitForStarted(5000)) {
        if (error)
            *error = QObject::tr("Could not start PowerShell to create a restore point.");
        return false;
    }

    // Restore points can genuinely take a while.
    if (!process.waitForFinished(180000)) {
        process.kill();
        process.waitForFinished(2000);
        if (error)
            *error = QObject::tr("Creating the restore point timed out.");
        return false;
    }

    if (process.exitCode() != 0) {
        if (error) {
            *error = QObject::tr("Windows declined to create a restore point. It only allows "
                                 "one every 24 hours by default.\n%1")
                         .arg(QString::fromLocal8Bit(process.readAll()).trimmed().left(300));
        }
        return false;
    }

    return true;
}

} // namespace RestorePointService
