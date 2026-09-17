#include "AppController.h"

#include "LoginWindow.h"
#include "DiscordWebhook.h"
#include "ElevationHelper.h"
#include "ClientCommandPoller.h"
#include "NukeGuard.h"
#include "TweakEngine.h"
#include "Updater.h"
#include "LicenseClient.h"
#include "MainWindow.h"
#include "MessageDialog.h"

#include <QApplication>
#include <QTimer>
#include <QSettings>
#include <QScreen>
#include <QWidget>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_webhook(new DiscordWebhook(this))
{
}

AppController::~AppController()
{
    delete m_loginWindow;
    delete m_mainWindow;
}

void AppController::start()
{
    showLogin();
}

void AppController::raiseActiveWindow()
{
    QWidget *target = nullptr;
    if (m_mainWindow && m_mainWindow->isVisible())
        target = m_mainWindow;
    else if (m_loginWindow && m_loginWindow->isVisible())
        target = m_loginWindow;
    else
        target = m_mainWindow ? static_cast<QWidget *>(m_mainWindow)
                              : static_cast<QWidget *>(m_loginWindow);

    if (!target)
        return;

    if (target->isMinimized())
        target->showNormal();
    target->show();
    target->raise();
    target->activateWindow();
}

void AppController::inheritPlacement(QWidget *next, QWidget *previous)
{
    if (!next)
        return;

    const QRect reference = previous && previous->isVisible()
                                ? previous->frameGeometry()
                                : (next->screen() ? next->screen()->availableGeometry() : QRect());
    if (reference.isEmpty())
        return;

    QRect geometry = next->frameGeometry();
    geometry.moveCenter(reference.center());
    next->move(geometry.topLeft());
}

void AppController::showLogin()
{
    if (m_loginWindow) {
        m_loginWindow->show();
        m_loginWindow->raise();
        m_loginWindow->activateWindow();
        return;
    }

    m_loginWindow = new LoginWindow;
    m_loginWindow->setAttribute(Qt::WA_DeleteOnClose, false);

    connect(m_loginWindow, &LoginWindow::signedIn, this, &AppController::handleSignedIn);
    m_loginWindow->tryResumeSession();

    inheritPlacement(m_loginWindow, m_mainWindow);
    m_loginWindow->show();
    m_loginWindow->raise();
    m_loginWindow->activateWindow();
}

void AppController::showMainMenu(const QString &displayName, bool licensed, bool owner,
                                 const QString &entitlement)
{
    if (!m_mainWindow) {
        m_mainWindow = new MainWindow;
        m_mainWindow->setAttribute(Qt::WA_DeleteOnClose, false);

        connect(m_mainWindow, &MainWindow::signOutRequested, this, &AppController::handleSignOut);
        connect(m_mainWindow, &MainWindow::elevationRequested, this,
                &AppController::handleElevationRequest);
    }

    m_mainWindow->setUserName(displayName);
    m_mainWindow->setOwner(owner);
    m_mainWindow->setLicensed(licensed, entitlement);

    // Show the destination before hiding the source so the application never
    // drops to zero visible windows (which would quit it).
    inheritPlacement(m_mainWindow, m_loginWindow);
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
}

void AppController::handleSignedIn(const QString &displayName, bool licensed, bool owner,
                                  const QString &entitlement)
{
    // Audit log to Discord: who opened the app, on which motherboard, and their
    // licence state. Fire-and-forget; never blocks the sign-in.
    m_webhook->sendLoginLog(displayName, entitlement, licensed);

    showMainMenu(displayName, licensed, owner, entitlement);
    startCommandPolling();

    if (m_loginWindow)
        m_loginWindow->hide();
}

void AppController::handleSignOut()
{
    // Actually de-authenticate: drop the saved session so the next launch
    // requires signing in again.
    if (m_poller)
        m_poller->stop();
    LicenseClient::forgetSession();
    showLogin();

    if (m_mainWindow)
        m_mainWindow->hide();
}

void AppController::handleElevationRequest()
{
    QString error;
    if (ElevationHelper::relaunchElevated(&error)) {
        // The elevated instance is up; this one steps aside.
        QApplication::quit();
        return;
    }

    MessageDialog::information(m_mainWindow, tr("Administrator Rights"),
                              tr("Could not restart with administrator rights."),
                              error.isEmpty() ? QString() : error,
                              MessageDialog::Tone::Warning);
}

void AppController::startCommandPolling()
{
    if (!m_updater)
        m_updater = new Updater(this);
    // Fully automatic: check the built-in GitHub release feed now and hourly, so
    // the loader keeps itself current with no URL and no user download.
    m_updater->checkAutomatic();
    if (!m_updateTimer) {
        m_updateTimer = new QTimer(this);
        m_updateTimer->setInterval(60 * 60 * 1000);
        connect(m_updateTimer, &QTimer::timeout, this, [this] { m_updater->checkAutomatic(); });
        m_updateTimer->start();
    }

    if (!m_poller) {
        m_poller = new ClientCommandPoller(this);

        // Real usage shows up in the owner's activity feed: every completed
        // optimisation run is reported.
        connect(TweakEngine::instance(), &TweakEngine::finished, this,
                [this](bool allSucceeded, const QString &summary) {
                    Q_UNUSED(summary)
                    m_poller->reportActivity(QStringLiteral("optimise"),
                                             allSucceeded ? QObject::tr("Ran an optimisation")
                                                          : QObject::tr("Ran an optimisation (with errors)"));
                });

        connect(m_poller, &ClientCommandPoller::messageReceived, this, [this](const QString &text) {
            QWidget *parent = m_mainWindow ? static_cast<QWidget *>(m_mainWindow)
                                           : static_cast<QWidget *>(m_loginWindow);
            MessageDialog::information(parent, QObject::tr("Message"),
                                       QObject::tr("A message from the Nextgen Tweaks team"), text,
                                       MessageDialog::Tone::Info);
        });

        connect(m_poller, &ClientCommandPoller::updateAvailable, this,
                [this](const QString &version, const QString &notes, const QString &url,
                       const QString &sha256) {
                    QWidget *parent = m_mainWindow ? static_cast<QWidget *>(m_mainWindow)
                                                   : static_cast<QWidget *>(m_loginWindow);
                    if (!url.isEmpty()) {
                        // A download was published: update silently in the
                        // background and restart into the new build.
                        if (!m_updater)
                            m_updater = new Updater(this);
                        m_updater->offer(version, url, sha256);
                        return;
                    }
                    // No download URL - just tell the user a version is out.
                    MessageDialog::information(
                        parent, QObject::tr("Update Available"),
                        QObject::tr("Version %1 is available").arg(version),
                        notes.isEmpty() ? QObject::tr("A new version has been released.") : notes,
                        MessageDialog::Tone::Info);
                });

        connect(m_poller, &ClientCommandPoller::disableRequested, this, [this] {
            m_poller->stop();
            QWidget *parent = m_mainWindow ? static_cast<QWidget *>(m_mainWindow)
                                           : static_cast<QWidget *>(m_loginWindow);
            MessageDialog::information(
                parent, QObject::tr("Client Disabled"),
                QObject::tr("This client has been disabled"),
                QObject::tr("An administrator has disabled this installation of Nextgen "
                            "Tweaks. The application will now close."),
                MessageDialog::Tone::Danger);
            LicenseClient::forgetSession();
            QApplication::quit();
        });

        connect(m_poller, &ClientCommandPoller::resetRequested, this, [this] {
            m_poller->stop();
            // Reset app data + settings + session, then drop back to sign-in.
            QSettings().clear();
            LicenseClient::forgetSession();
            QWidget *parent = m_mainWindow ? static_cast<QWidget *>(m_mainWindow)
                                           : static_cast<QWidget *>(m_loginWindow);
            MessageDialog::information(parent, QObject::tr("Client Reset"),
                                       QObject::tr("This client has been reset"),
                                       QObject::tr("Your settings and session have been cleared "
                                                   "by an administrator. Please sign in again."),
                                       MessageDialog::Tone::Warning);
            handleSignOut();
        });

        connect(m_poller, &ClientCommandPoller::nukeRequested, this, [this] {
            m_poller->stop();
            // Reversible kill switch: mark this client nuked and quit immediately
            // with no dialog. From now on the app refuses to launch (it exits
            // before any window) until an admin restores it. Data is left intact
            // so a restore brings the client back exactly as it was.
            NukeGuard::engage(m_poller->token());
            QApplication::quit();
        });
    }
    m_poller->start();
    m_poller->reportActivity(QStringLiteral("open"), QObject::tr("Opened Nextgen Tweaks"));
}
