// ElevationHelper.h - Administrator detection and elevated relaunch.
//
// Machine-wide tweaks (HKLM, services, defrag) need an elevated token. The app
// deliberately runs unelevated by default: it only asks for administrator when
// the user actually chooses a tweak that needs it.
#pragma once

#include <QString>

namespace ElevationHelper {

/// True when the current process is running with an elevated admin token.
bool isElevated();

/// Relaunches this executable with the "runas" verb, showing the UAC prompt.
/// Returns true if the elevated instance started (the caller should then quit).
/// \a error receives the reason on failure, including user cancellation.
bool relaunchElevated(QString *error = nullptr);

} // namespace ElevationHelper
