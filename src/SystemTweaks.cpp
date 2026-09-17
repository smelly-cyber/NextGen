#include "SystemTweaks.h"

#include "TweakBackupStore.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <psapi.h>
#  include <tlhelp32.h>
#endif

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSettings>
#include <QStorageInfo>

namespace {

/// Runs a console tool and returns its combined output. Used for powercfg, sc
/// and defrag, all of which ship with Windows.
QString runTool(const QString &program, const QStringList &arguments, int timeoutMs = 20000,
                int *exitCode = nullptr)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);

    if (!process.waitForStarted(5000)) {
        if (exitCode)
            *exitCode = -1;
        return QString();
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        if (exitCode)
            *exitCode = -1;
        return QString();
    }

    if (exitCode)
        *exitCode = process.exitCode();
    return QString::fromLocal8Bit(process.readAll());
}

} // namespace

// ------------------------------------------------------------- PowerPlan ---

PowerPlanTweak::PowerPlanTweak(QString id, QString title, QString description,
                               QString targetGuid)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::CurrentUser)
    , m_targetGuid(std::move(targetGuid))
{
}

QString PowerPlanTweak::activeSchemeGuid()
{
    const QString output = runTool(QStringLiteral("powercfg"), {QStringLiteral("/getactivescheme")});
    static const QRegularExpression re(
        QStringLiteral("([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{12})"));
    const QRegularExpressionMatch match = re.match(output);
    return match.hasMatch() ? match.captured(1).toLower() : QString();
}

QString PowerPlanTweak::schemeName(const QString &guid)
{
    if (guid.isEmpty())
        return QObject::tr("unknown");

    const QString output = runTool(QStringLiteral("powercfg"), {QStringLiteral("/list")});
    for (const QString &line : output.split(QLatin1Char('\n'))) {
        if (!line.contains(guid, Qt::CaseInsensitive))
            continue;
        const int open = line.indexOf(QLatin1Char('('));
        const int close = line.lastIndexOf(QLatin1Char(')'));
        if (open >= 0 && close > open)
            return line.mid(open + 1, close - open - 1).trimmed();
    }
    return guid;
}

QStringList PowerPlanTweak::plannedChanges() const
{
    const QString current = activeSchemeGuid();
    return {QObject::tr("Active power plan:  %1  →  %2")
                .arg(schemeName(current), schemeName(m_targetGuid))};
}

bool PowerPlanTweak::isApplied() const
{
    return activeSchemeGuid().compare(m_targetGuid, Qt::CaseInsensitive) == 0;
}

bool PowerPlanTweak::apply(TweakBackupStore &store, QString *error)
{
    const QString current = activeSchemeGuid();
    if (current.isEmpty()) {
        if (error)
            *error = QObject::tr("Could not read the active power plan.");
        return false;
    }

    if (!store.hasBackup(id(), QStringLiteral("scheme")))
        store.record(id(), QStringLiteral("scheme"), current, true);

    int code = 0;
    const QString output = runTool(QStringLiteral("powercfg"),
                                   {QStringLiteral("/setactive"), m_targetGuid}, 20000, &code);
    if (code != 0) {
        if (error) {
            *error = QObject::tr("powercfg could not activate that plan. It may not exist on "
                                 "this edition of Windows.\n%1")
                         .arg(output.trimmed());
        }
        return false;
    }
    return true;
}

bool PowerPlanTweak::revert(TweakBackupStore &store, QString *error)
{
    if (!store.hasBackup(id(), QStringLiteral("scheme")))
        return true;

    const QString previous = store.previousValue(id(), QStringLiteral("scheme")).toString();
    int code = 0;
    runTool(QStringLiteral("powercfg"), {QStringLiteral("/setactive"), previous}, 20000, &code);

    if (code != 0) {
        if (error)
            *error = QObject::tr("Could not restore the previous power plan.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------ TempCleaner --

TempCleanTweak::TempCleanTweak(QString id, QString title, QString description)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::CurrentUser)
{
}

QStringList TempCleanTweak::targetDirectories()
{
    QStringList candidates;

    // Per-user temp and browser-agnostic caches. No admin needed for these.
    candidates << QDir::tempPath();
    const QString localAppData =
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (!localAppData.isEmpty())
        candidates << localAppData;

    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    if (!local.isEmpty()) {
        candidates << local + QStringLiteral("/Microsoft/Windows/INetCache");
        candidates << local + QStringLiteral("/Microsoft/Windows/Explorer");
        candidates << local + QStringLiteral("/CrashDumps");
    }

    // Machine-wide temp; writable only when elevated, skipped otherwise.
    const QString windir = qEnvironmentVariable("SystemRoot");
    if (!windir.isEmpty())
        candidates << windir + QStringLiteral("/Temp");

    QStringList existing;
    for (const QString &path : candidates) {
        const QFileInfo info(QDir::cleanPath(path));
        if (info.exists() && info.isDir() && !existing.contains(info.absoluteFilePath()))
            existing << info.absoluteFilePath();
    }
    return existing;
}

quint64 TempCleanTweak::scanReclaimableBytes()
{
    quint64 total = 0;
    for (const QString &directory : targetDirectories()) {
        QDirIterator it(directory, QDir::Files | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            total += quint64(it.fileInfo().size());
        }
    }
    return total;
}

QStringList TempCleanTweak::plannedChanges() const
{
    QStringList lines;
    lines << QObject::tr("PERMANENTLY DELETE cached files from these folders:");
    for (const QString &directory : targetDirectories())
        lines << QObject::tr("    %1").arg(QDir::toNativeSeparators(directory));

    const quint64 bytes = scanReclaimableBytes();
    lines << QObject::tr("Roughly %1 MB found. Files currently in use are skipped.")
                 .arg(bytes / (1024 * 1024));
    lines << QObject::tr("This step cannot be undone by Revert.");
    return lines;
}

bool TempCleanTweak::isApplied() const
{
    // Cleaning is an action, not a state - it is always available to run again.
    return false;
}

bool TempCleanTweak::apply(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store)
    Q_UNUSED(error)

    m_lastFreedBytes = 0;

    for (const QString &directory : targetDirectories()) {
        const QString canonical = QDir(directory).canonicalPath();
        if (canonical.isEmpty())
            continue;

        QDirIterator it(directory, QDir::Files | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();

            // Belt and braces: never step outside the allow-listed directory.
            if (!info.absoluteFilePath().startsWith(canonical, Qt::CaseInsensitive))
                continue;

            const qint64 size = info.size();
            QFile file(info.absoluteFilePath());
            // Locked / in-use files simply fail to remove; that is expected and
            // is not treated as an error.
            if (file.remove())
                m_lastFreedBytes += quint64(size);
        }

        // Sweep up directories that are now empty.
        QDirIterator dirs(directory, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                          QDirIterator::Subdirectories);
        QStringList emptied;
        while (dirs.hasNext())
            emptied << dirs.next();
        for (auto it2 = emptied.crbegin(); it2 != emptied.crend(); ++it2)
            QDir().rmdir(*it2);
    }

    return true;
}

bool TempCleanTweak::revert(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store)
    Q_UNUSED(error)
    // Deleted files cannot be restored; the confirmation dialog says so up front.
    return true;
}

// -------------------------------------------------------- DriveOptimiser ---

DriveOptimiseTweak::DriveOptimiseTweak(QString id, QString title, QString description)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::Machine)
{
}

QStringList DriveOptimiseTweak::plannedChanges() const
{
    const QStorageInfo root = QStorageInfo::root();
    return {QObject::tr("Run the built-in Windows optimiser on %1")
                .arg(QDir::toNativeSeparators(root.rootPath())),
            QObject::tr("Windows picks TRIM for SSDs and defragmentation for hard disks."),
            QObject::tr("This can take several minutes and does not change any settings.")};
}

bool DriveOptimiseTweak::isApplied() const
{
    // Optimising is an action, not a persistent state.
    return false;
}

bool DriveOptimiseTweak::apply(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store)

    const QStorageInfo root = QStorageInfo::root();
    QString drive = QDir::toNativeSeparators(root.rootPath());
    if (drive.endsWith(QLatin1Char('\\')))
        drive.chop(1);

    int code = 0;
    // /O lets Windows choose the right operation for the media type.
    const QString output = runTool(QStringLiteral("defrag"),
                                   {drive, QStringLiteral("/O")}, 15 * 60 * 1000, &code);
    if (code != 0) {
        if (error) {
            *error = QObject::tr("Drive optimisation failed. Administrator rights are "
                                 "required.\n%1")
                         .arg(output.trimmed().left(400));
        }
        return false;
    }
    return true;
}

bool DriveOptimiseTweak::revert(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store)
    Q_UNUSED(error)
    return true;
}

// ----------------------------------------------------------- WorkingSets ---

WorkingSetTweak::WorkingSetTweak(QString id, QString title, QString description)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::CurrentUser)
{
}

QStringList WorkingSetTweak::plannedChanges() const
{
    return {QObject::tr("Ask Windows to trim the working set of running user processes."),
            QObject::tr("This returns physical RAM to the system immediately."),
            QObject::tr("Nothing is closed and no setting changes. Pages are read back from "
                        "disk on demand, so this is a one-off reclaim rather than a "
                        "permanent gain.")};
}

bool WorkingSetTweak::isApplied() const
{
    return false;
}

bool WorkingSetTweak::apply(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store)
    m_lastTrimmed = 0;

#ifdef _WIN32
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        if (error)
            *error = QObject::tr("Could not enumerate running processes.");
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            // Leave the idle and system processes alone.
            if (entry.th32ProcessID <= 4)
                continue;

            HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_LIMITED_INFORMATION,
                                         FALSE, entry.th32ProcessID);
            if (!process)
                continue; // Protected or higher-privilege process: skip quietly.

            if (EmptyWorkingSet(process))
                ++m_lastTrimmed;
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return true;
#else
    if (error)
        *error = QObject::tr("Only supported on Windows.");
    return false;
#endif
}

bool WorkingSetTweak::revert(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store)
    Q_UNUSED(error)
    return true;
}

// --------------------------------------------------------------- Startup ---

StartupTweak::StartupTweak(QString id, QString title, QString description)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::CurrentUser)
{
}

QString StartupTweak::runKey()
{
    return QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

QStringList StartupTweak::entryNames()
{
    const QSettings settings(runKey(), QSettings::NativeFormat);
    return settings.childKeys();
}

QStringList StartupTweak::plannedChanges() const
{
    const QStringList names = entryNames();
    if (names.isEmpty())
        return {QObject::tr("No per-user startup programs are registered.")};

    QStringList lines;
    lines << QObject::tr("Stop these %n program(s) launching when you sign in:", nullptr,
                         names.size());

    const QSettings settings(runKey(), QSettings::NativeFormat);
    for (const QString &name : names) {
        lines << QObject::tr("    %1  —  %2")
                     .arg(name, settings.value(name).toString().left(90));
    }
    lines << QObject::tr("Only your own startup entries are touched; nothing machine-wide "
                         "changes, and Revert puts every entry back.");
    return lines;
}

bool StartupTweak::isApplied() const
{
    // Applied means we have emptied the key; an empty key with no backup just
    // means the user never had startup entries.
    return false;
}

bool StartupTweak::apply(TweakBackupStore &store, QString *error)
{
    QSettings settings(runKey(), QSettings::NativeFormat);
    if (!settings.isWritable()) {
        if (error)
            *error = QObject::tr("Could not open your startup registry key.");
        return false;
    }

    const QStringList names = settings.childKeys();
    if (names.isEmpty())
        return true;

    for (const QString &name : names) {
        if (!store.hasBackup(id(), name))
            store.record(id(), name, settings.value(name), true);
        settings.remove(name);
    }
    settings.sync();
    return true;
}

bool StartupTweak::revert(TweakBackupStore &store, QString *error)
{
    QSettings settings(runKey(), QSettings::NativeFormat);
    if (!settings.isWritable()) {
        if (error)
            *error = QObject::tr("Could not open your startup registry key.");
        return false;
    }

    // Slot names are the entry names, so every recorded entry goes straight back.
    const QStringList recorded = entryNames();
    Q_UNUSED(recorded)

    // The store keeps slots per tweak; ask it for each name we know about.
    for (const QString &name : store.recordedSlots(id())) {
        if (store.previousValueExisted(id(), name))
            settings.setValue(name, store.previousValue(id(), name));
    }
    settings.sync();
    return true;
}

// -------------------------------------------------------------- Services ---

ServiceTweak::ServiceTweak(QString id, QString title, QString description,
                           QVector<ServiceSpec> services)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::Machine)
    , m_services(std::move(services))
{
}

QString ServiceTweak::startType(const QString &service)
{
    // Read straight from the service's own registry key rather than shelling out
    // to `sc qc`. The value is the same one sc reports, and this runs thousands
    // of times faster - which matters because every visit to a section page
    // re-reads the state of every service in the list.
    const QSettings key(QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\")
                            + service,
                        QSettings::NativeFormat);
    const QVariant start = key.value(QStringLiteral("Start"));
    if (!start.isValid())
        return QString(); // No such service on this machine.

    switch (start.toInt()) {
    case 0:
        return QStringLiteral("boot");
    case 1:
        return QStringLiteral("system");
    case 2:
        // sc.exe spells a delayed automatic start differently, and revert has to
        // put that exact keyword back or the service loses its delay.
        return key.value(QStringLiteral("DelayedAutostart")).toInt() == 1
                   ? QStringLiteral("delayed-auto")
                   : QStringLiteral("auto");
    case 3:
        return QStringLiteral("demand");
    case 4:
        return QStringLiteral("disabled");
    default:
        return QString();
    }
}

QStringList ServiceTweak::plannedChanges() const
{
    QStringList lines;
    for (const ServiceSpec &spec : m_services) {
        const QString current = startType(spec.name);
        if (current.isEmpty())
            continue; // Not present on this machine.
        lines << QObject::tr("%1 (%2):  start %3  →  %4")
                     .arg(spec.displayName, spec.name, current, spec.targetStart);
    }
    if (lines.isEmpty())
        lines << QObject::tr("None of these optional services are present on this machine.");
    return lines;
}

bool ServiceTweak::isApplied() const
{
    bool anyPresent = false;
    for (const ServiceSpec &spec : m_services) {
        const QString current = startType(spec.name);
        if (current.isEmpty())
            continue;
        anyPresent = true;
        if (current != spec.targetStart)
            return false;
    }
    return anyPresent;
}

bool ServiceTweak::apply(TweakBackupStore &store, QString *error)
{
    bool changedAny = false;

    for (const ServiceSpec &spec : m_services) {
        const QString current = startType(spec.name);
        if (current.isEmpty())
            continue;

        if (!store.hasBackup(id(), spec.name))
            store.record(id(), spec.name, current, true);

        int code = 0;
        const QString output =
            runTool(QStringLiteral("sc"),
                    {QStringLiteral("config"), spec.name,
                     QStringLiteral("start=") , spec.targetStart},
                    20000, &code);

        if (code != 0) {
            if (error) {
                *error = QObject::tr("Could not change the %1 service. Administrator rights "
                                     "are required.\n%2")
                             .arg(spec.displayName, output.trimmed().left(300));
            }
            return false;
        }
        changedAny = true;
    }

    if (!changedAny && error)
        *error = QObject::tr("None of these services are present on this machine.");
    return changedAny;
}

bool ServiceTweak::revert(TweakBackupStore &store, QString *error)
{
    for (const ServiceSpec &spec : m_services) {
        if (!store.hasBackup(id(), spec.name))
            continue;

        const QString previous = store.previousValue(id(), spec.name).toString();
        int code = 0;
        runTool(QStringLiteral("sc"),
                {QStringLiteral("config"), spec.name, QStringLiteral("start="), previous},
                20000, &code);

        if (code != 0) {
            if (error)
                *error = QObject::tr("Could not restore the %1 service.").arg(spec.displayName);
            return false;
        }
    }
    return true;
}

// --------------------------------------------------------------- Network ---

NetworkTweak::NetworkTweak(QString id, QString title, QString description)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::Machine)
{
}

NetworkAdapter NetworkTweak::detectAdapter()
{
    return NetworkAdapterInfo::activeAdapter();
}

QVector<NetworkTweak::Target> NetworkTweak::targets() const
{
    QVector<Target> list;

    const NetworkAdapter adapter = detectAdapter();
    if (!adapter.valid)
        return list;

    // --- Per-interface TCP behaviour --------------------------------------
    const QString tcpKey = NetworkAdapterInfo::tcpInterfaceKey(adapter.guid);
    if (!tcpKey.isEmpty()) {
        // Delayed ACK holds back acknowledgements for up to ~200ms hoping to
        // piggyback them on outgoing data. That is a pure latency penalty for
        // interactive traffic, on both media.
        list.append({tcpKey, QStringLiteral("TcpAckFrequency"), 1,
                     QObject::tr("acknowledge packets immediately (no delayed ACK)")});
        list.append({tcpKey, QStringLiteral("TcpDelAckTicks"), 0,
                     QObject::tr("remove the delayed-ACK timer")});

        // Nagle batches small packets. Great for throughput on a clean wired
        // link, harmful for latency - but on Wi-Fi the extra small frames cost
        // more in retransmits than they save, so it stays on there.
        if (adapter.medium == NetworkAdapter::Medium::Ethernet) {
            list.append({tcpKey, QStringLiteral("TCPNoDelay"), 1,
                         QObject::tr("send small packets immediately (Nagle off)")});
        }
    }

    // --- Adapter power management -----------------------------------------
    const QString driverKey = NetworkAdapterInfo::driverInstanceKey(adapter);
    if (!driverKey.isEmpty()) {
        // 24 clears "allow the computer to turn off this device to save power".
        // A NIC that sleeps is the classic cause of stalls and Wi-Fi drop-outs.
        list.append({driverKey, QStringLiteral("PnPCapabilities"), 24,
                     QObject::tr("stop Windows powering the adapter down to save energy")});
    }

    return list;
}

QStringList NetworkTweak::plannedChanges() const
{
    const NetworkAdapter adapter = detectAdapter();
    QStringList lines;

    if (!adapter.valid) {
        lines << QObject::tr("No active network adapter was detected - nothing to change.");
        return lines;
    }

    lines << QObject::tr("Detected adapter: %1").arg(adapter.summary());
    lines << QStringLiteral("      %1").arg(adapter.description);
    lines << QString();

    const QVector<Target> list = targets();
    if (list.isEmpty()) {
        lines << QObject::tr("This adapter exposes no tunable settings.");
        return lines;
    }

    for (const Target &target : list) {
        lines << QStringLiteral("• %1").arg(target.reason);
        lines << QStringLiteral("      %1 = %2")
                     .arg(target.valueName, target.optimised.toString());
    }

    if (adapter.medium == NetworkAdapter::Medium::WiFi) {
        lines << QString();
        lines << QObject::tr("Nagle's algorithm is left enabled: on Wi-Fi it protects against "
                             "retransmits that would cost more than it saves.");
    }
    return lines;
}

bool NetworkTweak::isApplied() const
{
    const QVector<Target> list = targets();
    if (list.isEmpty())
        return false;

    for (const Target &target : list) {
        const QSettings key(target.path, QSettings::NativeFormat);
        const QVariant current = key.value(target.valueName);
        if (!current.isValid() || current.toInt() != target.optimised.toInt())
            return false;
    }
    return true;
}

bool NetworkTweak::apply(TweakBackupStore &store, QString *error)
{
    const QVector<Target> list = targets();
    if (list.isEmpty()) {
        if (error)
            *error = QObject::tr("No active network adapter was detected.");
        return false;
    }

    for (const Target &target : list) {
        QSettings key(target.path, QSettings::NativeFormat);
        if (!key.isWritable()) {
            if (error) {
                *error = QObject::tr("No permission to write %1. Administrator rights are "
                                     "required.")
                             .arg(target.path);
            }
            return false;
        }

        // Record the way back before touching anything.
        const QString slot = target.path + QLatin1Char('\\') + target.valueName;
        const QVariant current = key.value(target.valueName);
        if (!store.hasBackup(id(), slot))
            store.record(id(), slot, current, current.isValid());

        key.setValue(target.valueName, target.optimised);
        key.sync();
        if (key.status() != QSettings::NoError) {
            if (error)
                *error = QObject::tr("Failed to write %1.").arg(slot);
            return false;
        }
    }
    return true;
}

bool NetworkTweak::revert(TweakBackupStore &store, QString *error)
{
    // Revert walks the RECORDED slots rather than re-detecting: the adapter in
    // use may have changed (docked, unplugged, switched to Wi-Fi) since apply,
    // and we must always undo exactly what we actually wrote.
    // NOTE: not named "slots" - that is a Qt keyword macro and will not compile.
    const QStringList recorded = store.recordedSlots(id());
    for (const QString &slot : recorded) {
        const int split = slot.lastIndexOf(QLatin1Char('\\'));
        if (split <= 0)
            continue;
        const QString path = slot.left(split);
        const QString valueName = slot.mid(split + 1);

        QSettings key(path, QSettings::NativeFormat);
        if (!key.isWritable()) {
            if (error) {
                *error = QObject::tr("No permission to restore %1. Administrator rights are "
                                     "required.")
                             .arg(path);
            }
            return false;
        }

        if (store.previousValueExisted(id(), slot))
            key.setValue(valueName, store.previousValue(id(), slot));
        else
            key.remove(valueName);
        key.sync();
    }
    return true;
}
