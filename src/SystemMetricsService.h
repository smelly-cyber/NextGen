// SystemMetricsService.h - Live system telemetry and the static machine report.
//
// Mirrors the AuthService pattern: the UI only ever talks to this object
// through signals, so the sampling back end can be replaced (WMI, PDH, vendor
// GPU SDKs) without touching a single widget.
//
// What is real today:
//   CPU load     - GetSystemTimes() deltas
//   Memory       - GlobalMemoryStatusEx()
//   Storage      - QStorageInfo on the root volume
//   Network      - GetIfTable2() octet counter deltas
//   Machine facts- registry + QSysInfo + Win32 topology calls
// What is not:
//   GPU load     - needs a vendor SDK; reported as unavailable so the UI can
//                  say so rather than invent a number.
#pragma once

#include <QList>
#include <QObject>
#include <QString>

class QElapsedTimer;
class QTimer;

/// One poll of the machine. A negative value means "not available".
struct MetricSample
{
    qreal cpuPercent = -1.0;
    qreal memoryPercent = -1.0;
    qreal storagePercent = -1.0;
    qreal gpuPercent = -1.0;

    qreal networkDownMbps = -1.0;
    qreal networkUpMbps = -1.0;
    int processCount = -1;      ///< Running processes right now.

    quint64 memoryUsedBytes = 0;
    quint64 memoryTotalBytes = 0;
    quint64 storageUsedBytes = 0;
    quint64 storageTotalBytes = 0;
};

/// Static description of the machine, read once at start up.
struct HardwareInfo
{
    QString cpuName;
    QString cpuClock;
    QString gpuName;
    QString storageName;
};

/// Everything the System Info page shows. Fields that could not be read are
/// left empty and the UI renders a dash instead of a guess.
struct SystemReport
{
    // Summary tiles
    QString cpuSummary;
    QString gpuSummary;
    QString memorySummary;
    QString storageSummary;

    // Hardware details
    QString processor;
    QString motherboard;
    QString memory;
    QString graphics;
    QString storage;
    QString network;

    // Software and system
    QString operatingSystem;
    QString osBuild;
    QString systemType;
    QString biosVersion;
    QString directXVersion;
    QString applicationVersion;

    /// Renders the whole report as a plain text file body.
    QString toPlainText() const;
};

class SystemMetricsService : public QObject
{
    Q_OBJECT

public:
    explicit SystemMetricsService(QObject *parent = nullptr);
    ~SystemMetricsService() override;

    /// Shared instance - the pages all read from the same poller.
    static SystemMetricsService *instance();

    const HardwareInfo &hardware() const { return m_hardware; }
    const MetricSample &latest() const { return m_latest; }

    /// Machine facts; built on first use and cached.
    const SystemReport &report();
    /// Re-reads the machine facts (the System Info "Refresh" button).
    void refreshReport();

    /// Recent values for the sparklines and the live monitor, oldest first.
    const QList<qreal> &cpuHistory() const { return m_cpuHistory; }
    const QList<qreal> &memoryHistory() const { return m_memoryHistory; }
    const QList<qreal> &storageHistory() const { return m_storageHistory; }
    const QList<qreal> &gpuHistory() const { return m_gpuHistory; }
    const QList<qreal> &networkHistory() const { return m_networkHistory; }

    /// Seconds between samples, so charts can label their time axis.
    static qreal sampleIntervalSeconds();
    /// How many samples the history keeps.
    static int historyLength();

    /// Seconds the machine has been running since its last boot, or -1 if it
    /// cannot be read. Sourced from the system tick count, so always available.
    qint64 uptimeSeconds() const;

    /// Human-readable uptime such as "2h 14m" or "3d 5h", or an empty string
    /// when unavailable.
    QString uptimeText() const;

    /// 0..100 health score derived from the current load. Returns -1 when not
    /// enough of the machine could be measured to be meaningful.
    qreal performanceScore() const;

    /// Formats a byte count as "15.3 GB".
    static QString formatBytes(quint64 bytes);

public slots:
    void start();
    void stop();
    void poll();

signals:
    void sampled(const MetricSample &sample);

private:
    void readHardware();
    void buildReport();
    void appendHistory(QList<qreal> &history, qreal value);
    void sampleNetwork(MetricSample &sample);

    QTimer *m_timer = nullptr;
    HardwareInfo m_hardware;
    SystemReport m_report;
    bool m_reportReady = false;
    MetricSample m_latest;

    QList<qreal> m_cpuHistory;
    QList<qreal> m_memoryHistory;
    QList<qreal> m_storageHistory;
    QList<qreal> m_gpuHistory;
    QList<qreal> m_networkHistory;

    // Previous CPU time counters, for the load delta.
    quint64 m_prevIdle = 0;
    quint64 m_prevTotal = 0;
    bool m_hasPrevCpu = false;

    // Previous network octet counters, for the throughput delta.
    quint64 m_prevBytesIn = 0;
    quint64 m_prevBytesOut = 0;
    bool m_hasPrevNetwork = false;
    QString m_networkAdapter;   ///< Description of the busiest live adapter.
    QElapsedTimer *m_networkClock = nullptr;
};
