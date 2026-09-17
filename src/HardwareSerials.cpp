#include "HardwareSerials.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <winioctl.h>
#endif

#include <QByteArray>
#include <QObject>

#include <cstring>

namespace {

/// Firmware often fills unknown serials with filler rather than leaving them
/// blank. Treat those as "not reported" so the UI never shows noise.
bool isPlaceholder(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v.isEmpty())
        return true;
    static const char *junk[] = {"to be filled by o.e.m.", "default string", "none", "n/a",
                                 "not applicable", "not specified", "unknown", "0",
                                 "00000000", "system serial number",
                                 "base board serial number", "chassis serial number",
                                 "modulepartnumber", "0123456789", "invalid", "null"};
    for (const char *j : junk) {
        if (v == QLatin1String(j))
            return true;
    }
    // All-zero or all-F serials are equally meaningless.
    bool allSame = true;
    for (const QChar &ch : v) {
        if (ch != v.at(0)) {
            allSame = false;
            break;
        }
    }
    return allSame && v.size() > 2;
}

QString cleaned(const QString &value)
{
    const QString trimmed = value.trimmed();
    return isPlaceholder(trimmed) ? QString() : trimmed;
}

#ifdef _WIN32

QByteArray readSmbios()
{
    const DWORD provider = 0x52534D42; // 'RSMB'
    const DWORD size = GetSystemFirmwareTable(provider, 0, nullptr, 0);
    if (size == 0)
        return {};
    QByteArray buffer(int(size), Qt::Uninitialized);
    const DWORD got = GetSystemFirmwareTable(provider, 0, buffer.data(), size);
    if (got == 0 || got > size)
        return {};
    buffer.resize(int(got));
    return buffer;
}

/// The idx-th (1-based) string following an SMBIOS structure's formatted area.
QString smbiosString(const quint8 *formatted, const quint8 *end, int idx)
{
    if (idx <= 0)
        return {};
    const quint8 *s = formatted;
    int current = 1;
    while (s < end && *s != 0) {
        const auto len = ::strnlen(reinterpret_cast<const char *>(s), size_t(end - s));
        if (current == idx)
            return QString::fromLatin1(reinterpret_cast<const char *>(s), int(len));
        s += len + 1;
        ++current;
    }
    return {};
}

/// Walks the SMBIOS table, handing each structure to \a visit.
template <typename Visitor>
void walkSmbios(const QByteArray &table, Visitor &&visit)
{
    if (table.size() < 8)
        return;
    const quint8 *data = reinterpret_cast<const quint8 *>(table.constData());
    DWORD length = 0;
    std::memcpy(&length, data + 4, sizeof(length));

    const quint8 *p = data + 8;
    const quint8 *end = data + table.size();
    if (length != 0 && p + length < end)
        end = p + length;

    while (p + 4 <= end) {
        const quint8 type = p[0];
        const quint8 formattedLen = p[1];
        const quint8 *formatted = p + formattedLen;
        if (formatted > end)
            break;

        visit(type, p, formattedLen,
              [&](int index) { return smbiosString(formatted, end, index); });

        const quint8 *s = formatted;
        while (s + 1 < end && (s[0] != 0 || s[1] != 0))
            ++s;
        p = s + 2;
    }
}

/// Serial + model for one physical disk, via the storage driver. Opened with no
/// access rights, which is what lets this work without administrator elevation.
bool queryDisk(int index, QString *model, QString *serial)
{
    const QString path = QStringLiteral("\\\\.\\PhysicalDrive%1").arg(index);
    HANDLE device = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), 0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                                nullptr);
    if (device == INVALID_HANDLE_VALUE)
        return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    QByteArray buffer(4096, Qt::Uninitialized);
    DWORD returned = 0;
    const bool ok = DeviceIoControl(device, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
                                    buffer.data(), DWORD(buffer.size()), &returned, nullptr);
    CloseHandle(device);
    if (!ok || returned < sizeof(STORAGE_DEVICE_DESCRIPTOR))
        return false;

    const auto *descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR *>(buffer.constData());
    const auto at = [&](DWORD offset) -> QString {
        if (offset == 0 || offset >= returned)
            return {};
        return QString::fromLatin1(buffer.constData() + offset).trimmed();
    };

    *model = at(descriptor->ProductIdOffset);
    *serial = at(descriptor->SerialNumberOffset);
    return !model->isEmpty() || !serial->isEmpty();
}

#endif // _WIN32

} // namespace

QVector<HardwareSerial> HardwareSerials::collect()
{
    QVector<HardwareSerial> entries;

#ifdef _WIN32
    const QByteArray table = readSmbios();

    QString systemSerial, systemProduct, boardSerial, boardProduct, chassisSerial, cpuSerial,
        cpuVersion;
    struct MemoryModule
    {
        QString locator;
        QString serial;
        QString part;
        int megabytes = 0;
    };
    QVector<MemoryModule> memory;

    walkSmbios(table, [&](quint8 type, const quint8 *f, quint8 len, const auto &str) {
        switch (type) {
        case 1: // System Information
            if (len > 7) {
                systemProduct = str(f[5]);
                systemSerial = str(f[7]);
            }
            break;
        case 2: // Baseboard
            if (len > 7) {
                boardProduct = str(f[5]);
                boardSerial = str(f[7]);
            }
            break;
        case 3: // Chassis
            if (len > 7)
                chassisSerial = str(f[7]);
            break;
        case 4: // Processor
            if (len > 0x10)
                cpuVersion = str(f[0x10]);
            if (len > 0x20)
                cpuSerial = str(f[0x20]);
            break;
        case 17: { // Memory Device - one per slot, populated or not.
            if (len <= 0x1A)
                break;
            quint16 sizeField = 0;
            std::memcpy(&sizeField, f + 0x0C, sizeof(sizeField));
            if (sizeField == 0) // Empty slot.
                break;
            MemoryModule module;
            // Bit 15 set means the value is in kB rather than MB.
            module.megabytes = (sizeField & 0x8000) ? (sizeField & 0x7FFF) / 1024 : sizeField;
            module.locator = str(f[0x10]);
            module.serial = str(f[0x18]);
            module.part = str(f[0x1A]);
            memory.append(module);
            break;
        }
        default:
            break;
        }
    });

    const auto add = [&entries](const QString &icon, const QString &label, const QString &value) {
        entries.append({icon, label,
                        value.isEmpty() ? QObject::tr("Not reported by firmware") : value});
    };

    add(QStringLiteral("cpu"), QObject::tr("Processor"),
        cleaned(cpuSerial).isEmpty() ? cleaned(cpuVersion) : cleaned(cpuSerial));
    add(QStringLiteral("board"), QObject::tr("Motherboard"), cleaned(boardSerial));
    if (!cleaned(boardProduct).isEmpty())
        add(QStringLiteral("nodes"), QObject::tr("Board model"), cleaned(boardProduct));
    add(QStringLiteral("monitor"), QObject::tr("System"),
        cleaned(systemSerial).isEmpty() ? cleaned(systemProduct) : cleaned(systemSerial));
    if (!cleaned(chassisSerial).isEmpty())
        add(QStringLiteral("window"), QObject::tr("Chassis"), cleaned(chassisSerial));

    int slot = 1;
    for (const MemoryModule &module : memory) {
        const QString label = module.locator.isEmpty()
                                  ? QObject::tr("Memory %1").arg(slot)
                                  : QObject::tr("Memory (%1)").arg(module.locator);
        QString value = cleaned(module.serial);
        if (!cleaned(module.part).isEmpty()) {
            value = value.isEmpty()
                        ? cleaned(module.part)
                        : QStringLiteral("%1  ·  %2").arg(value, cleaned(module.part));
        }
        if (module.megabytes > 0)
            value = QStringLiteral("%1  ·  %2 GB").arg(value).arg(module.megabytes / 1024);
        add(QStringLiteral("memory"), label, value);
        ++slot;
    }

    for (int i = 0; i < 16; ++i) {
        QString model, serial;
        if (!queryDisk(i, &model, &serial))
            continue;
        QString value = cleaned(serial);
        if (!model.isEmpty())
            value = value.isEmpty() ? model : QStringLiteral("%1  ·  %2").arg(value, model);
        add(QStringLiteral("disk"), QObject::tr("Disk %1").arg(i), value);
    }
#endif

    if (entries.isEmpty()) {
        entries.append({QStringLiteral("info"), QObject::tr("Serials"),
                        QObject::tr("Not available on this system")});
    }
    return entries;
}
