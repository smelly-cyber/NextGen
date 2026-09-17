#include "NetworkAdapterInfo.h"

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
#  include <winsock2.h>
#  include <ws2ipdef.h>
#  include <windows.h>
#  include <iphlpapi.h>
#  include <netioapi.h>
#endif

#include <QSettings>
#include <QStringList>

namespace {

#ifdef _WIN32
/// Network adapter class GUID - every NIC driver instance lives under this key.
const QLatin1String kNetClassKey(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{4D36E972-E325-11CE-BFC1-08002BE10318}");

QString fromWide(const wchar_t *text)
{
    return text ? QString::fromWCharArray(text) : QString();
}

/// Higher is better. Rejects anything that is not a real, connected, physical
/// link so VPN / Hyper-V / loopback adapters can never win.
int scoreOf(const MIB_IF_ROW2 &row)
{
    if (row.OperStatus != IfOperStatusUp)
        return -1;
    if (row.InterfaceAndOperStatusFlags.FilterInterface)
        return -1;
    // Hardware-backed only: excludes virtual switches and tunnels.
    if (!row.InterfaceAndOperStatusFlags.HardwareInterface)
        return -1;
    if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL)
        return -1;
    if (row.Type != IF_TYPE_ETHERNET_CSMACD && row.Type != IF_TYPE_IEEE80211)
        return -1;

    // Prefer the link that is actually moving bytes, then wired over wireless,
    // then raw link speed. Traffic is the strongest signal of "the one in use".
    int score = 0;
    if (row.InOctets > 0 || row.OutOctets > 0)
        score += 1000;
    if (row.Type == IF_TYPE_ETHERNET_CSMACD)
        score += 100;
    score += int(qMin<quint64>(row.ReceiveLinkSpeed / 1000000ULL, 90));
    return score;
}

NetworkAdapter toAdapter(const MIB_IF_ROW2 &row)
{
    NetworkAdapter adapter;
    adapter.valid = true;
    adapter.connected = row.OperStatus == IfOperStatusUp;
    adapter.alias = fromWide(row.Alias);
    adapter.description = fromWide(row.Description);
    adapter.linkSpeedBps = row.ReceiveLinkSpeed;
    adapter.medium = row.Type == IF_TYPE_IEEE80211 ? NetworkAdapter::Medium::WiFi
                                                   : NetworkAdapter::Medium::Ethernet;

    wchar_t guid[64] = {};
    if (StringFromGUID2(row.InterfaceGuid, guid, 64) > 0)
        adapter.guid = QString::fromWCharArray(guid);

    return adapter;
}
#endif // _WIN32

QString formatSpeed(quint64 bitsPerSecond)
{
    if (bitsPerSecond == 0)
        return {};
    if (bitsPerSecond >= 1000000000ULL) {
        const double gbps = double(bitsPerSecond) / 1e9;
        return QStringLiteral("%1 Gbps").arg(gbps, 0, 'g', 3);
    }
    return QStringLiteral("%1 Mbps").arg(bitsPerSecond / 1000000ULL);
}

} // namespace

QString NetworkAdapter::mediumName() const
{
    switch (medium) {
    case Medium::Ethernet:
        return QStringLiteral("Ethernet");
    case Medium::WiFi:
        return QStringLiteral("Wi-Fi");
    default:
        return QStringLiteral("Unknown");
    }
}

QString NetworkAdapter::summary() const
{
    if (!valid)
        return QStringLiteral("No active connection");
    const QString speed = formatSpeed(linkSpeedBps);
    return speed.isEmpty() ? mediumName() : QStringLiteral("%1 · %2").arg(mediumName(), speed);
}

QVector<NetworkAdapter> NetworkAdapterInfo::connectedAdapters()
{
    QVector<NetworkAdapter> found;
#ifdef _WIN32
    MIB_IF_TABLE2 *table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table)
        return found;

    QVector<QPair<int, NetworkAdapter>> scored;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2 &row = table->Table[i];
        const int score = scoreOf(row);
        if (score < 0)
            continue;
        scored.append({score, toAdapter(row)});
    }
    FreeMibTable(table);

    std::sort(scored.begin(), scored.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &entry : scored)
        found.append(entry.second);
#endif
    return found;
}

NetworkAdapter NetworkAdapterInfo::activeAdapter()
{
    const QVector<NetworkAdapter> adapters = connectedAdapters();
    return adapters.isEmpty() ? NetworkAdapter() : adapters.first();
}

QString NetworkAdapterInfo::tcpInterfaceKey(const QString &guid)
{
    if (guid.isEmpty())
        return {};
    return QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\"
                          "Parameters\\Interfaces\\%1")
        .arg(guid);
}

QString NetworkAdapterInfo::driverInstanceKey(const NetworkAdapter &adapter)
{
#ifdef _WIN32
    if (!adapter.valid || adapter.guid.isEmpty())
        return {};

    // The class key holds one numbered subkey per driver instance (0000, 0001,
    // ...). The right one is whichever carries our adapter's NetCfgInstanceId.
    const QSettings classKey(QString(kNetClassKey), QSettings::NativeFormat);
    const QStringList instances = classKey.childGroups();
    for (const QString &instance : instances) {
        if (instance.size() != 4)
            continue; // Skip "Properties" and friends.
        const QString path = QStringLiteral("%1\\%2").arg(QString(kNetClassKey), instance);
        const QSettings entry(path, QSettings::NativeFormat);
        if (entry.value(QStringLiteral("NetCfgInstanceId")).toString().compare(
                adapter.guid, Qt::CaseInsensitive)
            == 0) {
            return path;
        }
    }
#else
    Q_UNUSED(adapter)
#endif
    return {};
}
