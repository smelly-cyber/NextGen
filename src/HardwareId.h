// HardwareId.h - Reads stable hardware identifiers for licence HWID-locking.
//
// The licence system binds a key to the machine's motherboard. The serial is
// read straight from the SMBIOS firmware table (the same value the "wmic
// baseboard get serialnumber" command reports), with graceful fallbacks so a
// board that hides its serial still yields a stable per-machine id.
#pragma once

#include <QString>

namespace HardwareId {

/// Raw motherboard (baseboard) serial number, e.g. "220839471000643", or an
/// empty string if the board does not expose one. Suitable for showing an admin.
QString motherboardSerial();

/// The most specific stable identifier available: the motherboard serial, else
/// the SMBIOS system UUID, else the OS machine id. Never empty on a real PC.
QString stableId();

} // namespace HardwareId
