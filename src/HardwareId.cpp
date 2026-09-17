#include "HardwareId.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <QSysInfo>

#include <cstring>

namespace {

#ifdef _WIN32
/// True for the placeholder strings boards ship when no real serial is set.
bool isGenericSerial(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v.isEmpty())
        return true;
    static const char *junk[] = {"to be filled by o.e.m.", "default string", "none",
                                 "not applicable", "n/a", "system serial number",
                                 "base board serial number", "0", "00000000", "invalid"};
    for (const char *j : junk) {
        if (v == QLatin1String(j))
            return true;
    }
    return false;
}

/// Reads the raw SMBIOS firmware table into a buffer.
QByteArray readSmbios()
{
    const DWORD provider = 0x52534D42; // 'RSMB' - raw SMBIOS table provider signature.
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

/// Returns the idx-th (1-based) string that follows an SMBIOS structure's
/// formatted area at \a formatted, stopping at \a end.
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

/// Walks the SMBIOS structures, returning the first field the visitor accepts.
/// The visitor gets the structure type, its formatted bytes and length, plus a
/// helper to resolve a string index. It returns a non-empty QString to stop.
template <typename Visitor>
QString walkSmbios(const QByteArray &table, Visitor &&visit)
{
    if (table.size() < 8)
        return {};
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

        const auto getString = [&](int index) { return smbiosString(formatted, end, index); };
        const QString found = visit(type, p, formattedLen, getString);
        if (!found.isEmpty())
            return found;

        // Advance past the string-set, which ends at a double null.
        const quint8 *s = formatted;
        while (s + 1 < end && (s[0] != 0 || s[1] != 0))
            ++s;
        p = s + 2;
    }
    return {};
}

QString baseboardSerialWin()
{
    const QByteArray table = readSmbios();
    if (table.isEmpty())
        return {};
    // SMBIOS type 2 (Baseboard): serial number is the string index at offset 7.
    return walkSmbios(table, [](quint8 type, const quint8 *fields, quint8 len,
                                const auto &getString) -> QString {
        if (type == 2 && len > 7) {
            const QString serial = getString(fields[7]).trimmed();
            if (!isGenericSerial(serial))
                return serial;
        }
        return {};
    });
}

QString systemUuidWin()
{
    const QByteArray table = readSmbios();
    if (table.isEmpty())
        return {};
    // SMBIOS type 1 (System Information): UUID is 16 raw bytes at offset 8.
    return walkSmbios(table, [](quint8 type, const quint8 *fields, quint8 len,
                                const auto &) -> QString {
        if (type == 1 && len >= 24) {
            const quint8 *uuid = fields + 8;
            bool allZero = true, allFF = true;
            for (int i = 0; i < 16; ++i) {
                allZero &= (uuid[i] == 0x00);
                allFF &= (uuid[i] == 0xFF);
            }
            if (allZero || allFF)
                return {};
            QString hex;
            for (int i = 0; i < 16; ++i)
                hex += QString::asprintf("%02x", uuid[i]);
            return hex;
        }
        return {};
    });
}
#endif // _WIN32

} // namespace

QString HardwareId::motherboardSerial()
{
#ifdef _WIN32
    return baseboardSerialWin();
#else
    return {};
#endif
}

QString HardwareId::stableId()
{
#ifdef _WIN32
    const QString serial = baseboardSerialWin();
    if (!serial.isEmpty())
        return serial;
    const QString uuid = systemUuidWin();
    if (!uuid.isEmpty())
        return uuid;
#endif
    const QByteArray machine = QSysInfo::machineUniqueId();
    if (!machine.isEmpty())
        return QString::fromLatin1(machine);
    return QSysInfo::machineHostName();
}
