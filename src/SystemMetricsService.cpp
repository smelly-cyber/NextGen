#include "SystemMetricsService.h"

// The Windows headers must come first and must target Windows 10 so that
// GetIfTable2() / MIB_IF_TABLE2 are declared.
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifdef _WIN32_WINNT
#    undef _WIN32_WINNT
#  endif
#  define _WIN32_WINNT 0x0A00
#  ifdef WINVER
#    undef WINVER
#  endif
#  define WINVER 0x0A00
#  ifdef NTDDI_VERSION
#    undef NTDDI_VERSION
#  endif
#  define NTDDI_VERSION 0x0A000000
#  include <winsock2.h>
#  include <ws2ipdef.h>
#  include <windows.h>
#  include <iphlpapi.h>
#  include <netioapi.h>
#  include <pdh.h>
#  include <pdhmsg.h>
#  include <tlhelp32.h>
#endif

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QSettings>
#include <QStorageInfo>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QtGlobal>

namespace {
/// How often the machine is polled.
constexpr int kPollIntervalMs = 1500;
/// How many samples the history keeps (60s of monitor at the poll interval).
constexpr int kHistoryLength = 41;
/// Ceiling used to scale the network reading onto the shared 0..100 charts.
constexpr qreal kNetworkScaleMbps = 100.0;

#ifdef Q_OS_WIN
quint64 toUInt64(const FILETIME &value)
{
    ULARGE_INTEGER converted;
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return converted.QuadPart;
}

QString registryValue(const QString &key, const QString &name)
{
    const QSettings settings(key, QSettings::NativeFormat);
    return settings.value(name).toString().trimmed();
}

/// Physical core count via the processor topology table.
int physicalCoreCount()
{
    DWORD length = 0;
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length))
        return 0;
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0)
        return 0;

    QByteArray buffer(int(length), Qt::Uninitialized);
    auto *info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length))
        return 0;

    int cores = 0;
    DWORD offset = 0;
    while (offset < length) {
        auto *entry =
            reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data() + offset);
        if (entry->Size == 0)
            break;
        if (entry->Relationship == RelationProcessorCore)
            ++cores;
        offset += entry->Size;
    }
    return cores;
}

/// Number of running processes right now.
int runningProcessCount()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return -1;

    int count = 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            ++count;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}

/// Live GPU utilisation via the same PDH "GPU Engine" counters Task Manager
/// uses. Utilisation is a rate, so the query is kept open and collected on every
/// poll; the first read has no delta and returns -1. The 3D engine is summed
/// across processes and capped at 100%, which tracks graphics/game load well.
class PdhGpuMeter
{
public:
    PdhGpuMeter()
    {
        if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS) {
            m_query = nullptr;
            return;
        }
        // English counter path is locale-independent.
        if (PdhAddEnglishCounterW(m_query,
                                  L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0,
                                  &m_counter)
            != ERROR_SUCCESS) {
            PdhCloseQuery(m_query);
            m_query = nullptr;
            m_counter = nullptr;
            return;
        }
        PdhCollectQueryData(m_query); // Prime; the next collect yields a rate.
    }

    ~PdhGpuMeter()
    {
        if (m_query)
            PdhCloseQuery(m_query);
    }

    /// 0..100 utilisation, or -1 when unavailable / not yet ready.
    qreal read()
    {
        if (!m_query || !m_counter)
            return -1.0;
        if (PdhCollectQueryData(m_query) != ERROR_SUCCESS)
            return -1.0;

        DWORD bufferSize = 0;
        DWORD itemCount = 0;
        PDH_STATUS status = PdhGetFormattedCounterArrayW(m_counter, PDH_FMT_DOUBLE, &bufferSize,
                                                         &itemCount, nullptr);
        if (status != PDH_STATUS(PDH_MORE_DATA) || bufferSize == 0)
            return m_hasValue ? m_lastValue : -1.0;

        QByteArray buffer(int(bufferSize), Qt::Uninitialized);
        auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
        if (PdhGetFormattedCounterArrayW(m_counter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items)
            != ERROR_SUCCESS)
            return m_hasValue ? m_lastValue : -1.0;

        double sum = 0.0;
        for (DWORD i = 0; i < itemCount; ++i) {
            if (items[i].FmtValue.CStatus == ERROR_SUCCESS)
                sum += items[i].FmtValue.doubleValue;
        }
        m_lastValue = qBound(0.0, sum, 100.0);
        m_hasValue = true;
        return m_lastValue;
    }

private:
    PDH_HQUERY m_query = nullptr;
    PDH_HCOUNTER m_counter = nullptr;
    qreal m_lastValue = -1.0;
    bool m_hasValue = false;
};
#endif

QString orDash(const QString &value)
{
    return value.isEmpty() ? QStringLiteral("—") : value;
}

} // namespace

// --------------------------------------------------------------- Report ----

QString SystemReport::toPlainText() const
{
    const auto line = [](const QString &label, const QString &value) {
        return QStringLiteral("  %1%2\n")
            .arg(label.leftJustified(22, QLatin1Char(' ')), orDash(value));
    };

    QString text;
    text += QStringLiteral("NEXTGEN TWEAKS - SYSTEM REPORT\n");
    text += QStringLiteral("==============================\n\n");

    text += QStringLiteral("HARDWARE\n");
    text += line(QStringLiteral("Processor:"), processor);
    text += line(QStringLiteral("Motherboard:"), motherboard);
    text += line(QStringLiteral("Memory:"), memory);
    text += line(QStringLiteral("Graphics:"), graphics);
    text += line(QStringLiteral("Storage:"), storage);
    text += line(QStringLiteral("Network:"), network);

    text += QStringLiteral("\nSOFTWARE & SYSTEM\n");
    text += line(QStringLiteral("Operating System:"), operatingSystem);
    text += line(QStringLiteral("OS Build:"), osBuild);
    text += line(QStringLiteral("System Type:"), systemType);
    text += line(QStringLiteral("BIOS Version:"), biosVersion);
    text += line(QStringLiteral("DirectX Version:"), directXVersion);
    text += line(QStringLiteral("Nextgen Tweaks:"), applicationVersion);

    return text;
}

// -------------------------------------------------------------- Service ----

SystemMetricsService::SystemMetricsService(QObject *parent)
    : QObject(parent)
{
    m_networkClock = new QElapsedTimer;
    readHardware();

    m_timer = new QTimer(this);
    m_timer->setInterval(kPollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SystemMetricsService::poll);
}

SystemMetricsService::~SystemMetricsService()
{
    delete m_networkClock;
}

SystemMetricsService *SystemMetricsService::instance()
{
    static SystemMetricsService service;
    return &service;
}

qreal SystemMetricsService::sampleIntervalSeconds()
{
    return kPollIntervalMs / 1000.0;
}

int SystemMetricsService::historyLength()
{
    return kHistoryLength;
}

void SystemMetricsService::start()
{
    if (m_timer->isActive())
        return;
    poll();
    m_timer->start();
}

void SystemMetricsService::stop()
{
    m_timer->stop();
}

void SystemMetricsService::readHardware()
{
#ifdef Q_OS_WIN
    const QString cpuKey =
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0");
    m_hardware.cpuName = registryValue(cpuKey, QStringLiteral("ProcessorNameString"));

    const QSettings cpuSettings(cpuKey, QSettings::NativeFormat);
    const uint megahertz = cpuSettings.value(QStringLiteral("~MHz")).toUInt();
    if (megahertz > 0)
        m_hardware.cpuClock = QStringLiteral("%1 GHz").arg(megahertz / 1000.0, 0, 'f', 1);

    m_hardware.gpuName = registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\"
                       "{4d36e968-e325-11ce-bfc1-08002be10318}\\0000"),
        QStringLiteral("DriverDesc"));
#endif

    if (m_hardware.cpuName.isEmpty())
        m_hardware.cpuName = tr("Processor");
    if (m_hardware.gpuName.isEmpty())
        m_hardware.gpuName = tr("Graphics adapter");

    const QStorageInfo root = QStorageInfo::root();
    m_hardware.storageName = root.displayName().isEmpty() ? root.rootPath() : root.displayName();
}

const SystemReport &SystemMetricsService::report()
{
    if (!m_reportReady)
        buildReport();
    return m_report;
}

void SystemMetricsService::refreshReport()
{
    readHardware();
    buildReport();
}

void SystemMetricsService::buildReport()
{
    SystemReport r;

    // --- Processor ----------------------------------------------------------
    const int threads = QThread::idealThreadCount();
    int cores = 0;
#ifdef Q_OS_WIN
    cores = physicalCoreCount();
#endif

    if (cores > 0 && threads > 0) {
        r.processor = tr("%1 (%2 cores / %3 threads)")
                          .arg(m_hardware.cpuName)
                          .arg(cores)
                          .arg(threads);
    } else if (threads > 0) {
        r.processor = tr("%1 (%2 threads)").arg(m_hardware.cpuName).arg(threads);
    } else {
        r.processor = m_hardware.cpuName;
    }

    r.cpuSummary = m_hardware.cpuClock.isEmpty()
                       ? m_hardware.cpuName
                       : tr("%1 @ %2").arg(m_hardware.cpuName, m_hardware.cpuClock);

    // --- Memory -------------------------------------------------------------
    if (m_latest.memoryTotalBytes == 0)
        poll();
    if (m_latest.memoryTotalBytes > 0) {
        const QString total = formatBytes(m_latest.memoryTotalBytes);
        r.memory = total;
        r.memorySummary = total;
    }

    // --- Graphics -----------------------------------------------------------
    r.graphics = m_hardware.gpuName;
    r.gpuSummary = m_hardware.gpuName;

    // --- Storage ------------------------------------------------------------
    const QStorageInfo root = QStorageInfo::root();
    if (root.isValid() && root.bytesTotal() > 0) {
        const QString total = formatBytes(quint64(root.bytesTotal()));
        const QString fs = QString::fromUtf8(root.fileSystemType());
        r.storage = tr("%1  %2  (%3)").arg(m_hardware.storageName, total, fs);
        r.storageSummary = tr("%1 %2").arg(total, fs);
    }

#ifdef Q_OS_WIN
    // --- Motherboard and BIOS ------------------------------------------------
    const QString biosKey =
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS");
    const QString boardVendor = registryValue(biosKey, QStringLiteral("BaseBoardManufacturer"));
    const QString boardProduct = registryValue(biosKey, QStringLiteral("BaseBoardProduct"));
    if (!boardVendor.isEmpty() || !boardProduct.isEmpty())
        r.motherboard = QStringLiteral("%1 %2").arg(boardVendor, boardProduct).trimmed();

    const QString biosVendor = registryValue(biosKey, QStringLiteral("BIOSVendor"));
    const QString biosVersion = registryValue(biosKey, QStringLiteral("BIOSVersion"));
    if (!biosVendor.isEmpty() || !biosVersion.isEmpty())
        r.biosVersion = QStringLiteral("%1 %2").arg(biosVendor, biosVersion).trimmed();

    // --- Operating system ----------------------------------------------------
    const QString osKey =
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    const QSettings osSettings(osKey, QSettings::NativeFormat);
    const QString build = osSettings.value(QStringLiteral("CurrentBuild")).toString();
    const uint updateBuild = osSettings.value(QStringLiteral("UBR")).toUInt();
    if (!build.isEmpty())
        r.osBuild = updateBuild > 0 ? QStringLiteral("%1.%2").arg(build).arg(updateBuild) : build;

    const QString directXRuntime = registryValue(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\DirectX"),
        QStringLiteral("Version"));

    // That registry value is the legacy runtime string (4.09.00.xxxx). What
    // people mean by "DirectX version" is the API level, which is 12 on every
    // Windows 10 or later build.
    r.directXVersion = build.toInt() >= 10240 ? QStringLiteral("12") : directXRuntime;
#endif

    r.operatingSystem = QSysInfo::prettyProductName();
    r.systemType = tr("%1-based PC").arg(QSysInfo::currentCpuArchitecture());
    r.applicationVersion = QCoreApplication::applicationVersion();

    r.network = m_networkAdapter;
    if (r.network.isEmpty() && m_latest.networkDownMbps >= 0.0)
        r.network = tr("Active adapter");

    m_report = r;
    m_reportReady = true;
}

void SystemMetricsService::appendHistory(QList<qreal> &history, qreal value)
{
    if (value < 0.0)
        return;
    history.append(value);
    while (history.size() > kHistoryLength)
        history.removeFirst();
}

void SystemMetricsService::sampleNetwork(MetricSample &sample)
{
#ifdef Q_OS_WIN
    MIB_IF_TABLE2 *table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table)
        return;

    quint64 bytesIn = 0;
    quint64 bytesOut = 0;
    QString adapter;

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2 &row = table->Table[i];
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        if (row.OperStatus != IfOperStatusUp)
            continue;
        if (row.InterfaceAndOperStatusFlags.FilterInterface)
            continue;

        bytesIn += row.InOctets;
        bytesOut += row.OutOctets;

        if (adapter.isEmpty() && (row.InOctets > 0 || row.OutOctets > 0))
            adapter = QString::fromWCharArray(row.Description);
    }

    FreeMibTable(table);

    if (!adapter.isEmpty())
        m_networkAdapter = adapter;

    if (m_hasPrevNetwork && m_networkClock->isValid()) {
        const qreal seconds = m_networkClock->elapsed() / 1000.0;
        if (seconds > 0.05) {
            const quint64 deltaIn = bytesIn >= m_prevBytesIn ? bytesIn - m_prevBytesIn : 0;
            const quint64 deltaOut = bytesOut >= m_prevBytesOut ? bytesOut - m_prevBytesOut : 0;
            sample.networkDownMbps = (deltaIn * 8.0) / (seconds * 1000000.0);
            sample.networkUpMbps = (deltaOut * 8.0) / (seconds * 1000000.0);
        }
    }

    m_prevBytesIn = bytesIn;
    m_prevBytesOut = bytesOut;
    m_hasPrevNetwork = true;
    m_networkClock->restart();
#else
    Q_UNUSED(sample)
#endif
}

void SystemMetricsService::poll()
{
    MetricSample sample;

#ifdef Q_OS_WIN
    // --- CPU ---------------------------------------------------------------
    FILETIME idleTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        const quint64 idle = toUInt64(idleTime);
        // Kernel time already includes the idle time.
        const quint64 total = toUInt64(kernelTime) + toUInt64(userTime);

        if (m_hasPrevCpu && total > m_prevTotal) {
            const qreal totalDelta = qreal(total - m_prevTotal);
            const qreal idleDelta = qreal(idle - m_prevIdle);
            sample.cpuPercent = qBound(0.0, (1.0 - idleDelta / totalDelta) * 100.0, 100.0);
        }
        m_prevIdle = idle;
        m_prevTotal = total;
        m_hasPrevCpu = true;
    }

    // --- Memory ------------------------------------------------------------
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        sample.memoryTotalBytes = memory.ullTotalPhys;
        sample.memoryUsedBytes = memory.ullTotalPhys - memory.ullAvailPhys;
        if (sample.memoryTotalBytes > 0) {
            sample.memoryPercent =
                qreal(sample.memoryUsedBytes) / qreal(sample.memoryTotalBytes) * 100.0;
        }
    }
#endif

    // --- Storage -----------------------------------------------------------
    const QStorageInfo root = QStorageInfo::root();
    if (root.isValid() && root.bytesTotal() > 0) {
        sample.storageTotalBytes = quint64(root.bytesTotal());
        sample.storageUsedBytes = quint64(root.bytesTotal() - root.bytesAvailable());
        sample.storagePercent =
            qreal(sample.storageUsedBytes) / qreal(sample.storageTotalBytes) * 100.0;
    }

    // --- Network -----------------------------------------------------------
    sampleNetwork(sample);

    // --- GPU ---------------------------------------------------------------
    // Real GPU utilisation via PDH GPU-engine counters (same source as Task
    // Manager). The meter lives across polls so its rate deltas accumulate.
#ifdef Q_OS_WIN
    static PdhGpuMeter gpuMeter;
    sample.gpuPercent = gpuMeter.read();
    sample.processCount = runningProcessCount();
#else
    sample.gpuPercent = -1.0;
#endif

    // Carry readings over on the very first poll, where there is no delta yet.
    if (sample.cpuPercent < 0.0 && m_latest.cpuPercent >= 0.0)
        sample.cpuPercent = m_latest.cpuPercent;
    if (sample.networkDownMbps < 0.0 && m_latest.networkDownMbps >= 0.0)
        sample.networkDownMbps = m_latest.networkDownMbps;
    if (sample.gpuPercent < 0.0 && m_latest.gpuPercent >= 0.0)
        sample.gpuPercent = m_latest.gpuPercent;

    m_latest = sample;
    appendHistory(m_cpuHistory, sample.cpuPercent);
    appendHistory(m_memoryHistory, sample.memoryPercent);
    appendHistory(m_storageHistory, sample.storagePercent);
    appendHistory(m_gpuHistory, sample.gpuPercent);

    if (sample.networkDownMbps >= 0.0) {
        appendHistory(m_networkHistory,
                      qBound(0.0, sample.networkDownMbps / kNetworkScaleMbps * 100.0, 100.0));
    }

    emit sampled(m_latest);
}

qreal SystemMetricsService::performanceScore() const
{
    // Weighted headroom across whatever could actually be measured.
    qreal weighted = 0.0;
    qreal weight = 0.0;

    const auto add = [&](qreal percent, qreal w) {
        if (percent >= 0.0) {
            weighted += (100.0 - percent) * w;
            weight += w;
        }
    };

    add(m_latest.cpuPercent, 0.35);
    add(m_latest.memoryPercent, 0.35);
    add(m_latest.storagePercent, 0.30);

    if (weight <= 0.0)
        return -1.0;

    return qBound(0.0, weighted / weight, 100.0);
}

qint64 SystemMetricsService::uptimeSeconds() const
{
#ifdef Q_OS_WIN
    // GetTickCount64() is the milliseconds since boot; always available, no
    // admin, and never blocks.
    return qint64(GetTickCount64() / 1000ULL);
#else
    return -1;
#endif
}

QString SystemMetricsService::uptimeText() const
{
    const qint64 total = uptimeSeconds();
    if (total < 0)
        return QString();

    const qint64 days = total / 86400;
    const qint64 hours = (total % 86400) / 3600;
    const qint64 minutes = (total % 3600) / 60;

    if (days > 0)
        return QStringLiteral("%1d %2h").arg(days).arg(hours);
    if (hours > 0)
        return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    if (minutes > 0)
        return QStringLiteral("%1m").arg(minutes);
    return QStringLiteral("%1s").arg(total);
}

QString SystemMetricsService::formatBytes(quint64 bytes)
{
    constexpr qreal kGigabyte = 1024.0 * 1024.0 * 1024.0;
    const qreal gigabytes = bytes / kGigabyte;

    if (gigabytes >= 100.0)
        return QStringLiteral("%1 GB").arg(qRound(gigabytes));
    return QStringLiteral("%1 GB").arg(gigabytes, 0, 'f', 1);
}
