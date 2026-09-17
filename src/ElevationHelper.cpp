#include "ElevationHelper.h"

#include "SingleInstanceGuard.h" // kRelaunchFlag

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QObject>

namespace ElevationHelper {

bool isElevated()
{
#ifdef _WIN32
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;

    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation),
                                        &size);
    CloseHandle(token);

    return ok && elevation.TokenIsElevated != 0;
#else
    return false;
#endif
}

bool relaunchElevated(QString *error)
{
#ifdef _WIN32
    const QString program = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString workingDirectory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());

    // Tell the new process this is our own relaunch, so the single-instance
    // guard waits for this one to exit instead of treating it as a duplicate.
    const QString parameters = QString::fromLatin1(kRelaunchFlag);

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"runas";
    info.lpFile = reinterpret_cast<LPCWSTR>(program.utf16());
    info.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
    info.lpDirectory = reinterpret_cast<LPCWSTR>(workingDirectory.utf16());
    info.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&info)) {
        if (info.hProcess)
            CloseHandle(info.hProcess);
        return true;
    }

    const DWORD code = GetLastError();
    if (error) {
        *error = code == ERROR_CANCELLED
                     ? QObject::tr("Administrator access was declined.")
                     : QObject::tr("Could not restart with administrator rights (error %1).")
                           .arg(code);
    }
    return false;
#else
    if (error)
        *error = QObject::tr("Elevation is only supported on Windows.");
    return false;
#endif
}

} // namespace ElevationHelper
