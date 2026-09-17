// main.cpp - Entry point of the NEXTGEN TWEAKS desktop client.
//
// Keep this file thin: application wide setup only. AppController owns the
// navigation between screens.
#include "AppController.h"
#include "ElevationHelper.h"
#include "MessageDialog.h"
#include "NukeGuard.h"
#include "SingleInstanceGuard.h"
#include "Theme.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
    // Fractional scale factors are passed straight through so the custom
    // painting stays sharp on high DPI displays.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("Nextgen Tweaks"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("nextgentweaks.app"));
    QCoreApplication::setApplicationName(QStringLiteral("NextGen Tweaks"));

    // The nuke kill switch. If this client was nuked it must not run: check
    // with the server (silently, no window) and, unless it has been restored,
    // exit immediately so nothing appears on screen. A restore clears the
    // marker and the app continues as normal.
    if (NukeGuard::isEngaged()) {
        if (NukeGuard::stillNukedOnServer())
            return 3; // Refuse to start - the "it just closes / crashes" behaviour.
        NukeGuard::disengage();
    }

    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // Only one copy of Nextgen Tweaks may run at a time. A second launch pings
    // the one already running (which brings its window to the front) and quits.
    // Our own relaunches - the elevated restart and Settings' "Restart Now" -
    // pass --relaunch, so they wait for the outgoing process to let go of the
    // lock and take it over instead of being turned away as a duplicate.
    const bool relaunching = app.arguments().contains(QLatin1String(kRelaunchFlag));
    SingleInstanceGuard guard(QStringLiteral("NextGenTweaks.instance"),
                              relaunching ? 5000 : 0);
    if (!guard.isPrimary()) {
        guard.notifyPrimary();
        return 0;
    }

    // Load the saved appearance (accent, Dark/Midnight, animations, compact)
    // into the Theme before any window is built, and start watching settings so
    // later changes re-theme the running UI.
    ThemeManager::instance()->applyFromSettings();

    QApplication::setFont(Theme::font(14));
    app.setStyleSheet(Theme::globalStyleSheet());

    // Nextgen Tweaks writes machine-wide settings (services, HKLM policies, the
    // network adapter), none of which work without an elevated token. Rather
    // than let the user hit a wall of permission errors later, refuse up front
    // and offer to relaunch properly.
    if (!ElevationHelper::isElevated()) {
        MessageDialog dialog(nullptr, QObject::tr("Administrator Required"));
        dialog.setTone(MessageDialog::Tone::Warning);
        dialog.setHeading(QObject::tr("Nextgen Tweaks must be run as administrator."));
        dialog.setBody(QObject::tr(
            "The optimisations change machine-wide Windows settings, which Windows only "
            "allows with administrator rights.<br><br>Restart with administrator rights to "
            "continue."));
        dialog.addButton(QObject::tr("Exit"), ModernButton::Variant::Secondary, 0);
        dialog.addButton(QObject::tr("Restart as Administrator"), ModernButton::Variant::Primary,
                         1, true);

        if (dialog.run() == 1) {
            QString error;
            if (!ElevationHelper::relaunchElevated(&error)) {
                MessageDialog::information(
                    nullptr, QObject::tr("Administrator Required"),
                    QObject::tr("Could not restart with administrator rights."), error,
                    MessageDialog::Tone::Warning);
            }
        }
        return 0;
    }

    AppController controller;
    QObject::connect(&guard, &SingleInstanceGuard::anotherInstanceStarted, &controller,
                     &AppController::raiseActiveWindow);
    controller.start();

    return app.exec();
}
