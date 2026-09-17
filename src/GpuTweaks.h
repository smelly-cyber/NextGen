// GpuTweaks.h - Graphics driver tweaks for NVIDIA and AMD cards.
//
// Both classes work out what to do at run time rather than holding a fixed
// list, because the right registry keys depend on which GPU is installed and
// which numbered driver instance Windows gave it.
#pragma once

#include "Tweak.h"

#include <QStringList>

/// The graphics vendors the GPU tweaks know how to tune.
enum class GpuVendor { Nvidia, Amd };

/// One installed graphics driver instance.
struct GpuAdapter
{
    GpuVendor vendor = GpuVendor::Nvidia;
    QString name;        ///< e.g. "NVIDIA GeForce RTX 3070 Ti".
    QString driverKey;   ///< Full native path of its display-class driver key.
};

namespace GpuDetection {
/// Every NVIDIA and AMD adapter with an installed driver. Other vendors
/// (Intel, virtual display adapters) are ignored.
QVector<GpuAdapter> adapters();
bool hasVendor(GpuVendor vendor);
} // namespace GpuDetection

// ----------------------------------------------------------- GpuDriver ------

/// Vendor-specific driver settings for whichever NVIDIA / AMD cards are present.
///
///   NVIDIA  per-core DPC handling (spreads the driver's deferred work across
///           cores instead of piling it onto core 0) and the driver's
///           telemetry opt-outs.
///   AMD     ULPS off (the ultra-low-power state that adds a wake-up stall),
///           GPU deep sleep off, and Frame Rate Target Control off (a driver
///           frame cap that adds latency when left on by accident).
///
/// A machine with neither vendor simply has nothing to change: the tweak
/// reports that and succeeds, so a group it belongs to still applies.
class GpuDriverTweak : public Tweak
{
public:
    GpuDriverTweak(QString id, QString title, QString description);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

private:
    /// The concrete registry writes for the adapters installed right now.
    RegistryTweak resolved() const;
};

// ---------------------------------------------------- NvidiaProfileImport ---

/// Imports an NVIDIA Profile Inspector profile (.nip) using Profile Inspector
/// itself.
///
/// Neither is bundled. Each run downloads Profile Inspector and the profile
/// from the GitHub links set at the top of GpuTweaks.cpp into a private
/// temporary folder, runs a silent import, makes sure Profile Inspector has
/// exited, and deletes the whole folder again. Nothing is left on the machine.
///
/// Release-asset links are checked against the SHA-256 GitHub publishes for
/// them. Without a Profile Inspector link the official release is used; without
/// a profile link, the newest .nip in profiles\nvidia\ beside the executable.
/// No NVIDIA card or no profile means nothing to do.
class NvidiaProfileTweak : public Tweak
{
public:
    NvidiaProfileTweak(QString id, QString title, QString description);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

    bool isAvailable() const override;
    QString unavailableReason() const override;

    /// Folder the profile is read from.
    static QString profileDirectory();
    /// The profile that would be imported, or an empty string.
    static QString profilePath();
};
