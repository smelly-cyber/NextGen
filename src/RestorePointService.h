// RestorePointService.h - Creates a Windows System Restore point.
//
// Machine-level tweaks run behind one of these, so there is always a way back
// even if something outside the app's undo record goes wrong.
#pragma once

#include <QString>

namespace RestorePointService {

/// True when System Protection is on for the system drive, so a restore point
/// can actually be created.
bool isAvailable();

/// Creates a MODIFY_SETTINGS restore point named \a description.
/// Requires an elevated token. Returns false with a reason in \a error.
bool create(const QString &description, QString *error = nullptr);

} // namespace RestorePointService
