// HardwareSerials.h - Reads the serial numbers stamped into your hardware.
//
// Pulls identifiers straight from the SMBIOS firmware tables (system, baseboard,
// chassis, processor and every populated memory slot) and from the storage
// driver for each physical disk. Nothing here needs administrator rights, and
// nothing is sent anywhere - it is read only, for the System Info page.
#pragma once

#include <QString>
#include <QVector>

struct HardwareSerial
{
    QString icon;  ///< Icon name from assets/icons.
    QString label; ///< e.g. "Motherboard".
    QString value; ///< The serial, or a clear reason it is unavailable.
};

namespace HardwareSerials {

/// Every serial the machine is willing to report, in display order.
QVector<HardwareSerial> collect();

} // namespace HardwareSerials
