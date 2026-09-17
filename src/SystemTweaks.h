// SystemTweaks.h - Tweaks that do more than write a registry value.
//
// Power plan switching, temp file cleaning, drive optimisation, working set
// trimming and service start-type changes. Each one still honours the Tweak
// contract: preview, read back, apply, revert.
#pragma once

#include "NetworkAdapterInfo.h"
#include "Tweak.h"

#include <QStringList>
#include <QVariant>
#include <QVector>

// ------------------------------------------------------------- PowerPlan ---

/// Switches the active Windows power scheme via powercfg.
class PowerPlanTweak : public Tweak
{
public:
    PowerPlanTweak(QString id, QString title, QString description, QString targetGuid);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

    /// GUID of the scheme Windows currently has active, or an empty string.
    static QString activeSchemeGuid();
    /// Friendly name for a scheme GUID, falling back to the GUID itself.
    static QString schemeName(const QString &guid);

private:
    QString m_targetGuid;
};

// ------------------------------------------------------------ TempCleaner --

/// Deletes cached junk from known-safe temporary locations.
///
/// This is the only tweak that removes files, so it is deliberately the most
/// conservative: a fixed allow-list of directories, a dry-run scan that reports
/// what it found, files still in use are skipped, and nothing outside those
/// directories is ever touched. Deletion cannot be undone, so the confirmation
/// dialog says so explicitly.
class TempCleanTweak : public Tweak
{
public:
    TempCleanTweak(QString id, QString title, QString description);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

    /// Directories this tweak is allowed to clean, filtered to those that exist.
    static QStringList targetDirectories();
    /// Total bytes of removable content found across those directories.
    static quint64 scanReclaimableBytes();
    /// Bytes actually freed by the last apply().
    quint64 lastFreedBytes() const { return m_lastFreedBytes; }

private:
    quint64 m_lastFreedBytes = 0;
};

// -------------------------------------------------------- DriveOptimiser ---

/// Runs the built-in Windows optimiser (TRIM on SSDs, defrag on spinning disks).
class DriveOptimiseTweak : public Tweak
{
public:
    DriveOptimiseTweak(QString id, QString title, QString description);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;
};

// ----------------------------------------------------------- WorkingSets ---

/// Trims the working set of user processes, returning physical RAM to Windows.
///
/// Honest caveat carried into the UI: pages come back on demand, so this frees
/// RAM now but is not a permanent gain.
class WorkingSetTweak : public Tweak
{
public:
    WorkingSetTweak(QString id, QString title, QString description);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

    int lastTrimmedProcesses() const { return m_lastTrimmed; }

private:
    int m_lastTrimmed = 0;
};

// --------------------------------------------------------------- Startup ---

/// Disables per-user startup programs.
///
/// Only HKCU\...\Run is touched, so no administrator rights are needed and
/// nothing machine-wide is affected. Every entry it will disable is named in
/// plannedChanges(), so the confirmation dialog *is* the opt-in: the user sees
/// the exact program list before agreeing. Each value is recorded so revert
/// puts the entries back byte for byte.
class StartupTweak : public Tweak
{
public:
    StartupTweak(QString id, QString title, QString description);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

    /// Names of the startup entries currently registered for this user.
    static QStringList entryNames();

private:
    static QString runKey();
};

// --------------------------------------------------------------- Network ---

/// Tunes the network adapter you are actually using, wired or wireless.
///
/// This one is adaptive rather than a fixed list: it detects the live adapter
/// (see NetworkAdapterInfo) and picks settings to match the medium.
///
///   Both      stop Windows powering the adapter down to save energy, and
///             disable delayed ACK, which otherwise adds up to ~200ms before
///             small packets are acknowledged.
///   Ethernet  additionally disables Nagle's algorithm. On a stable wired link
///             sending small packets immediately is a clear latency win.
///   Wi-Fi     deliberately leaves Nagle enabled: on a shared, lossy medium the
///             extra tiny packets cost more in retransmits than they save.
///
/// Every value is a documented registry setting and is recorded before it is
/// touched, so revert puts the adapter back exactly as it was.
class NetworkTweak : public Tweak
{
public:
    NetworkTweak(QString id, QString title, QString description);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

    /// The adapter this tweak would act on right now.
    static NetworkAdapter detectAdapter();

private:
    /// One registry value this tweak owns, plus why it is being changed.
    struct Target
    {
        QString path;
        QString valueName;
        QVariant optimised;
        QString reason;
    };

    /// Resolves the exact writes for the currently detected adapter. Empty when
    /// no usable adapter is connected.
    QVector<Target> targets() const;
};

// -------------------------------------------------------------- Services ---

/// Sets the start type of a small, curated list of optional Windows services.
///
/// Only services that Windows itself runs fine without are listed, and the
/// original start type of each one is recorded so revert puts it back.
class ServiceTweak : public Tweak
{
public:
    struct ServiceSpec
    {
        QString name;        ///< Service key name, e.g. "DiagTrack".
        QString displayName; ///< What the user sees.
        QString targetStart; ///< "disabled" or "demand".
    };

    ServiceTweak(QString id, QString title, QString description,
                 QVector<ServiceSpec> services);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

    const QVector<ServiceSpec> &services() const { return m_services; }

    /// Current start type of \a service as an sc.exe keyword, or an empty string.
    static QString startType(const QString &service);

private:
    QVector<ServiceSpec> m_services;
};
