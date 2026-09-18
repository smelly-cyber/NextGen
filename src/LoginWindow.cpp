#include "LoginWindow.h"

#include "BrandingPanel.h"
#include "CreateAccountDialog.h"
#include "CustomTitleBar.h"
#include "LicenseClient.h"
#include "LoginPanel.h"
#include "MessageDialog.h"
#include "PanelSeparator.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QUrl>

namespace {
/// Relative widths of the two panels (matches the reference design).
constexpr int kBrandingStretch = 48;
constexpr int kLoginStretch = 52;

/// Where "Forgot password?" sends people: the Discord server, to open a ticket.
const QLatin1String kDiscordInvite("https://discord.gg/5pQUCbjA8V");
} // namespace

LoginWindow::LoginWindow(QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowTitle(tr("NEXTGEN TWEAKS"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/logo/nextgen_tweaks_mark.png")));

    // A sign-in dialog is fixed format: no maximise, no full screen.
    setMaximiseAllowed(false);

    m_license = new LicenseClient(this);

    buildUi();
    connectSignals();

    // The sign-in window is a fixed-format dialog: not resizable by dragging.
    setResizable(false);
}

LoginWindow::~LoginWindow() = default;

void LoginWindow::buildUi()
{
    QWidget *content = contentWidget();

    m_brandingPanel = new BrandingPanel(content);
    m_separator = new PanelSeparator(Qt::Vertical, content);
    m_loginPanel = new LoginPanel(content);

    auto *panels = new QHBoxLayout(content);
    panels->setContentsMargins(0, 0, 0, 0);
    panels->setSpacing(0);
    panels->addWidget(m_brandingPanel, kBrandingStretch);
    panels->addWidget(m_separator);
    panels->addWidget(m_loginPanel, kLoginStretch);

    titleBar()->raise();
    m_loginPanel->focusFirstField();
}

void LoginWindow::connectSignals()
{
    // --- Sign in ------------------------------------------------------------
    connect(m_loginPanel, &LoginPanel::loginRequested, this,
            [this](const QString &identifier, const QString &password, bool rememberMe) {
                Q_UNUSED(rememberMe)
                // No licence is entered at sign-in any more; the account is
                // authenticated first and, if it has no linked licence, the
                // main window forces activation before anything can be used.
                m_license->signIn(identifier, password, QString());
            });

    connect(m_loginPanel, &LoginPanel::createAccountRequested, this,
            &LoginWindow::openCreateAccountDialog);

    connect(m_loginPanel, &LoginPanel::forgotPasswordRequested, this, [this] {
        // Password resets are handled by an admin, not self-service, so point the
        // user at the Discord support ticket flow in a proper popup window.
        MessageDialog dialog(this, tr("Forgot Password"));
        dialog.setTone(MessageDialog::Tone::Info);
        dialog.setHeading(tr("Reset your password"));
        dialog.setBody(tr("For security, passwords can only be reset by an admin.<br><br>"
                          "Open a support ticket in the <b>NEXTGEN TWEAKS</b> Discord server "
                          "and an admin will reset your account for you."));
        dialog.addButton(tr("Open Discord"), ModernButton::Variant::Primary, 1, true);
        dialog.addButton(tr("Close"), ModernButton::Variant::Secondary, 0);
        if (dialog.run() == 1)
            QDesktopServices::openUrl(QUrl(QString(kDiscordInvite)));
    });

    // --- Licensing backend --------------------------------------------------
    connect(m_license, &LicenseClient::started, this, [this] {
        m_loginPanel->setBusy(true);
        if (m_createDialog)
            m_createDialog->setBusy(true);
    });

    connect(m_license, &LicenseClient::failed, this, [this](const QString &reason) {
        m_loginPanel->setBusy(false);

        // An automatic session resume failing is not the user's problem: they
        // have not typed anything yet. Drop back to a clean sign-in form rather
        // than greeting them with a red error (which is what happened when the
        // licence server was not running at launch).
        if (m_resumingSession) {
            m_resumingSession = false;
            m_loginPanel->clearStatus();
            m_loginPanel->focusFirstField();
            return;
        }

        if (m_createDialog && m_createDialog->isVisible())
            m_createDialog->setBusy(false), m_createDialog->showError(reason);
        else
            m_loginPanel->showError(reason);
    });

    connect(m_license, &LicenseClient::succeeded, this, [this](const LicenseResult &result) {
        m_resumingSession = false;
        m_loginPanel->setBusy(false);
        if (m_createDialog) {
            m_createDialog->accept();
            m_createDialog->deleteLater();
            m_createDialog = nullptr;
        }
        emit signedIn(result.username, result.licensed, result.owner, result.entitlementSummary());
    });
}

void LoginWindow::openCreateAccountDialog()
{
    if (m_createDialog)
        return;

    m_createDialog = new CreateAccountDialog(this);

    connect(m_createDialog, &CreateAccountDialog::submitted, this, [this] {
        // Accounts are created without a licence; activation happens in-app.
        m_license->createAccount(m_createDialog->username(), m_createDialog->email(),
                                 m_createDialog->password(), QString());
    });
    connect(m_createDialog, &QDialog::finished, this, [this] {
        if (m_createDialog) {
            m_createDialog->deleteLater();
            m_createDialog = nullptr;
        }
    });

    m_createDialog->open();
}

void LoginWindow::tryResumeSession()
{
    if (!m_license->hasStoredSession())
        return;

    m_resumingSession = true;
    m_loginPanel->showInfo(tr("Resuming your session..."));
    m_license->resumeSession();
}
