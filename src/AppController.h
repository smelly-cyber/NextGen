// AppController.h - Top level navigation between the application's windows.
//
// Keeping the top level routing in one place means the windows never have to
// know about each other: LoginWindow just reports that somebody signed in and
// this controller decides what happens next. Navigation *within* the workspace
// (menu <-> section pages) belongs to MainWindow's stacked router.
#pragma once

#include "MenuCard.h"

#include <QObject>
#include <QPointer>
#include <QString>

class ClientCommandPoller;
class DiscordWebhook;
class LoginWindow;
class MainWindow;

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    /// Opens the first screen of the application.
    void start();

public slots:
    /// Brings whichever window is currently in use to the front, restoring it
    /// if minimised. Used when a second launch is turned away.
    void raiseActiveWindow();

private slots:
    void handleSignedIn(const QString &displayName, bool licensed, bool owner,
                        const QString &entitlement);
    void handleSignOut();
    void handleElevationRequest();

private:
    void showLogin();
    void showMainMenu(const QString &displayName, bool licensed, bool owner,
                      const QString &entitlement);

    /// Centres \a next on whatever \a previous currently occupies, so switching
    /// screens never makes the app jump across the desktop.
    static void inheritPlacement(QWidget *next, QWidget *previous);

    QPointer<LoginWindow> m_loginWindow;
    QPointer<MainWindow> m_mainWindow;
    DiscordWebhook *m_webhook = nullptr;
    ClientCommandPoller *m_poller = nullptr;
    class Updater *m_updater = nullptr;
    class QTimer *m_updateTimer = nullptr;

    /// Sets up the poller that receives admin commands while signed in.
    void startCommandPolling();
};
