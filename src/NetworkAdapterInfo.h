// NetworkAdapterInfo.h - Works out which network adapter you are actually using.
//
// Windows usually reports several adapters at once (Ethernet, Wi-Fi, virtual
// switches from VPN / Hyper-V / VirtualBox, loopback). Tuning the wrong one is
// worse than useless, so activeAdapter() scores the candidates and returns the
// single interface your traffic is really going through, along with whether it
// is wired or wireless - which decides which tweaks make sense.
#pragma once

#include <QString>
#include <QVector>

struct NetworkAdapter
{
    enum class Medium {
        Unknown,
        Ethernet, ///< Wired. Latency tuning is safe and effective.
        WiFi      ///< Wireless. Power-saving is the dominant latency source.
    };

    bool valid = false;
    Medium medium = Medium::Unknown;
    QString alias;        ///< Friendly name, e.g. "Ethernet" / "Wi-Fi".
    QString description;  ///< Hardware name, e.g. "Realtek Gaming 2.5GbE".
    QString guid;         ///< "{GUID}" - keys the per-interface TCP settings.
    quint64 linkSpeedBps = 0;
    bool connected = false;

    /// e.g. "Ethernet - 2.5 Gbps" for display.
    QString summary() const;
    /// "Wi-Fi" / "Ethernet" / "Unknown".
    QString mediumName() const;
};

namespace NetworkAdapterInfo {

/// The interface carrying your traffic right now, or an invalid adapter when
/// nothing usable is connected.
NetworkAdapter activeAdapter();

/// Every connected physical adapter, best first. Mostly useful for diagnostics.
QVector<NetworkAdapter> connectedAdapters();

/// Registry key holding the per-interface TCP settings for \a guid, or an empty
/// string when the guid is not usable.
QString tcpInterfaceKey(const QString &guid);

/// Registry key of the adapter's driver instance under the network class, which
/// is where the power-management and advanced properties live. Empty when the
/// adapter cannot be matched.
QString driverInstanceKey(const NetworkAdapter &adapter);

} // namespace NetworkAdapterInfo
