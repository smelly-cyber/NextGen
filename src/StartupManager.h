// StartupManager.h - Real "launch on Windows startup" support.
//
// Adds or removes an entry under the per-user
// HKCU\Software\Microsoft\Windows\CurrentVersion\Run key so that Windows starts
// Nextgen Tweaks automatically at sign-in. This is a genuine system change (no
// admin rights needed) and is fully reversible.
#pragma once

class StartupManager
{
public:
    /// True when the Run entry currently points at this executable.
    static bool isEnabled();

    /// Writes (enable) or deletes (disable) the Run entry. Returns true on
    /// success. A no-op returning false on platforms without the registry.
    static bool setEnabled(bool enabled);
};
