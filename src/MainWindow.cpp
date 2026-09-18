#include "MainWindow.h"

#include "AdminPage.h"
#include "BrandingPanel.h"
#include "CustomTitleBar.h"
#include "EnhancePage.h"
#include "MainMenuPanel.h"
#include "OptimisePage.h"
#include "PanelSeparator.h"
#include "PerformPage.h"
#include "SectionPage.h"
#include "SettingsPage.h"
#include "SystemInfoPage.h"
#include "StatusStrip.h"
#include "Theme.h"
#include "LicenseActivationDialog.h"
#include "LicenseClient.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
/// Relative widths of the two columns on the main menu.
constexpr int kMenuBrandingStretch = 26;
constexpr int kMenuContentStretch = 74;
/// Tagline shown on the right of the status strip while a section is open.
const char *kSectionTagline = QT_TRANSLATE_NOOP("MainWindow", "Nextgen Tweaks  •  Built for Performance");
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowTitle(tr("NextGen Tweaks"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/logo/nextgen_tweaks_mark.png")));

    // The workspace is resizable by dragging its edges, but like the sign-in
    // dialog it is deliberately never maximised or made full screen.
    setMaximiseAllowed(false);
    setCloseOnEscape(false);

    // The section pages are dense, so the workspace opens larger than the login
    // dialog and refuses to be squeezed below a usable size.
    const Theme::Metrics &metrics = Theme::metrics();
    // 936 rather than 880. The section pages are dense, and at 880 the tallest
    // of them did not fit - a window that cannot hold its own content is what
    // made Windows grow it during a drag. The pages were trimmed to suit as
    // well; this leaves a little slack on top of that so a longer translation or
    // a different font does not put it back over the edge. Still comfortably
    // inside a 1080p screen once the taskbar is accounted for.
    setMinimumSize(1150 + 2 * metrics.glowMargin, 700 + 2 * metrics.glowMargin);
    resize(1500 + 2 * metrics.glowMargin, 936 + 2 * metrics.glowMargin);

    titleBar()->setBrand(tr("Nextgen Tweaks"));

    buildUi();
    connectSignals();
    showMainMenu();

    // The workspace is a fixed-size window: no edge-drag resizing at all.
    setResizable(false);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    QWidget *content = contentWidget();

    m_brandingPanel = new BrandingPanel(content);
    m_verticalSeparator = new PanelSeparator(Qt::Vertical, content);

    m_menuPanel = new MainMenuPanel(content);
    m_optimisePage = new OptimisePage(content);
    m_enhancePage = new EnhancePage(content);
    m_performPage = new PerformPage(content);
    m_systemInfoPage = new SystemInfoPage(content);
    m_settingsPage = new SettingsPage(content);
    m_adminPage = new AdminPage(content);

    m_pages.insert(int(MenuSection::Optimize), m_optimisePage);
    m_pages.insert(int(MenuSection::Enhance), m_enhancePage);
    m_pages.insert(int(MenuSection::Perform), m_performPage);
    m_pages.insert(int(MenuSection::SystemInfo), m_systemInfoPage);
    m_pages.insert(int(MenuSection::Settings), m_settingsPage);

    m_stack = new QStackedWidget(content);
    m_stack->addWidget(m_menuPanel);
    m_stack->addWidget(m_adminPage);
    for (SectionPage *page : {static_cast<SectionPage *>(m_optimisePage),
                              static_cast<SectionPage *>(m_enhancePage),
                              static_cast<SectionPage *>(m_performPage),
                              static_cast<SectionPage *>(m_systemInfoPage),
                              static_cast<SectionPage *>(m_settingsPage)}) {
        m_stack->addWidget(page);
    }

    auto *columns = new QHBoxLayout;
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(0);
    columns->addWidget(m_brandingPanel, kMenuBrandingStretch);
    columns->addWidget(m_verticalSeparator);
    columns->addWidget(m_stack, kMenuContentStretch);

    m_statusSeparator = new PanelSeparator(Qt::Horizontal, content);
    m_statusStrip = new StatusStrip(content);

    auto *stack = new QVBoxLayout(content);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(0);
    stack->addLayout(columns, 1);
    stack->addWidget(m_statusSeparator);
    stack->addWidget(m_statusStrip);

    titleBar()->raise();
}

void MainWindow::connectSignals()
{
    // The sidebar rail navigates as well as the cards do.
    m_brandingPanel->setNavigationVisible(true);
    connect(m_brandingPanel, &BrandingPanel::homeRequested, this, &MainWindow::showMainMenu);
    connect(m_brandingPanel, &BrandingPanel::sectionRequested, this, &MainWindow::showSection);
    connect(m_brandingPanel, &BrandingPanel::activateRequested, this,
            &MainWindow::openActivationDialog);

    connect(m_adminPage, &AdminPage::statusChanged, this,
            [this](const QString &t, const QColor &col) { m_statusStrip->setStatus(t, col); });
    connect(m_adminPage, &AdminPage::settingsRequested, this,
            [this] { showSection(MenuSection::Settings); });

    connect(m_menuPanel, &MainMenuPanel::sectionActivated, this, &MainWindow::showSection);
    connect(m_menuPanel, &MainMenuPanel::activateRequested, this,
            &MainWindow::openActivationDialog);
    connect(m_menuPanel, &MainMenuPanel::accountRequested, this, [this] {
        // TODO: open the account popover (profile, licence, sign out).
        emit signOutRequested();
    });

    // Licence redemption from the in-app activation prompt.
    m_license = new LicenseClient(this);
    connect(m_license, &LicenseClient::started, this, [this] {
        if (m_activation)
            m_activation->setBusy(true);
    });
    connect(m_license, &LicenseClient::failed, this, [this](const QString &reason) {
        if (m_activation) {
            m_activation->setBusy(false);
            m_activation->showError(reason);
        }
    });
    connect(m_license, &LicenseClient::succeeded, this, [this](const LicenseResult &result) {
        if (m_activation)
            m_activation->setBusy(false);
        if (result.licensed) {
            setLicensed(true, result.entitlementSummary());
            if (m_activation)
                m_activation->accept();
        } else if (m_activation) {
            // Authenticated but the key was rejected (invalid / expired / taken).
            m_activation->showError(result.reason);
        }
    });

    // Both the back control and the brand go home.
    connect(titleBar(), &CustomTitleBar::backRequested, this, &MainWindow::showMainMenu);
    connect(titleBar(), &CustomTitleBar::brandClicked, this, &MainWindow::showMainMenu);

    connect(m_settingsPage, &SettingsPage::signOutRequested, this,
            &MainWindow::signOutRequested);

    for (SectionPage *page : m_pages) {
        connect(page, &SectionPage::elevationRequested, this, &MainWindow::elevationRequested);
        connect(page, &SectionPage::statusChanged, this,
                [this, page](const QString &text, const QColor &color) {
                    if (m_stack->currentWidget() == page)
                        m_statusStrip->setStatus(text, color);
                });
    }
}

void MainWindow::applyMenuChrome()
{
    m_brandingPanel->setCompact(false);
    m_brandingPanel->setActivePillar(-1);

    m_brandingPanel->setMaximumWidth(QWIDGETSIZE_MAX);
    m_brandingPanel->setMinimumWidth(0);

    m_statusStrip->setTrailingText(tr("NEXT LEVEL PERFORMANCE"));
    m_statusStrip->setStatus(tr("Optimal"), Theme::colors().success);

    titleBar()->setBackVisible(false);
}

void MainWindow::applySectionChrome(SectionPage *page, int pillarIndex)
{
    const int railWidth = Theme::metrics().railCompactWidth;

    m_brandingPanel->setCompact(true);
    m_brandingPanel->setActivePillar(pillarIndex);
    m_brandingPanel->setMinimumWidth(railWidth);
    m_brandingPanel->setMaximumWidth(railWidth);

    m_statusStrip->setTrailingText(tr(kSectionTagline));
    m_statusStrip->setStatus(page->statusText(), page->statusColor());

    titleBar()->setBackVisible(true);
}

void MainWindow::showMainMenu()
{
    if (auto *current = qobject_cast<SectionPage *>(m_stack->currentWidget()))
        current->pageHidden();

    m_stack->setCurrentWidget(m_menuPanel);
    m_menuPanel->clearSelection(); // No sticky highlight on the menu itself.
    m_brandingPanel->setNavigationHome();
    applyMenuChrome();
}

void MainWindow::showSection(MenuSection section)
{
    if (section == MenuSection::Admin) {
        if (!m_isOwner)
            return; // Not reachable without the nav row, but belt and braces.
        if (auto *current = qobject_cast<SectionPage *>(m_stack->currentWidget()))
            current->pageHidden();
        m_brandingPanel->setNavigationSection(section);
        m_stack->setCurrentWidget(m_adminPage);
        // Admin is not a pillar; use the compact rail with no pillar highlight.
        const int railWidth = Theme::metrics().railCompactWidth;
        m_brandingPanel->setCompact(true);
        m_brandingPanel->setActivePillar(-1);
        m_brandingPanel->setMinimumWidth(railWidth);
        m_brandingPanel->setMaximumWidth(railWidth);
        titleBar()->setBackVisible(true);
        m_adminPage->refresh();
        return;
    }

    // The features are licence-gated: an unlicensed user is offered activation
    // instead of the page.
    if (!m_licensed) {
        openActivationDialog();
        return;
    }

    SectionPage *page = m_pages.value(int(section), nullptr);
    if (!page) {
        m_menuPanel->setCurrentSection(section);
        return;
    }

    if (auto *current = qobject_cast<SectionPage *>(m_stack->currentWidget()))
        current->pageHidden();

    int pillar = -1;
    if (section == MenuSection::Optimize)
        pillar = 0;
    else if (section == MenuSection::Enhance)
        pillar = 1;
    else if (section == MenuSection::Perform)
        pillar = 2;

    m_menuPanel->setCurrentSection(section);
    m_brandingPanel->setNavigationSection(section);
    m_stack->setCurrentWidget(page);
    applySectionChrome(page, pillar);
    page->pageShown();
}

void MainWindow::setUserName(const QString &name)
{
    m_menuPanel->setUserName(name);
    m_settingsPage->setUserName(name);
}

void MainWindow::setOwner(bool owner)
{
    m_isOwner = owner;
    m_brandingPanel->setAdminVisible(owner);
}

void MainWindow::setEntitlement(const QString &text)
{
    m_settingsPage->setEntitlement(text);
}

void MainWindow::setLicensed(bool licensed, const QString &entitlement)
{
    m_licensed = licensed;
    m_menuPanel->setLocked(!licensed);
    m_brandingPanel->setNavigationLocked(!licensed);
    setEntitlement(entitlement);

    if (licensed) {
        if (m_activation) {
            m_activation->accept();
            m_activation->deleteLater();
            m_activation = nullptr;
        }
        return;
    }

    // Unlicensed: make sure the user is on the (locked) menu, then prompt.
    showMainMenu();
    openActivationDialog();
}

void MainWindow::openActivationDialog()
{
    if (m_activation) {
        m_activation->raise();
        m_activation->activateWindow();
        return;
    }

    m_activation = new LicenseActivationDialog(this);
    connect(m_activation, &LicenseActivationDialog::submitted, this,
            [this] { m_license->redeemLicense(m_activation->licenseKey()); });
    connect(m_activation, &LicenseActivationDialog::signOutRequested, this, [this] {
        // Close the gate without quitting, then hand off to the sign-out flow so
        // the user lands back on the login screen to use a different account.
        if (m_activation)
            m_activation->accept();
        emit signOutRequested();
    });
    connect(m_activation, &QDialog::finished, this, [this] {
        if (m_activation) {
            m_activation->deleteLater();
            m_activation = nullptr;
        }
    });
    m_activation->open();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    const bool onSection = qobject_cast<SectionPage *>(m_stack->currentWidget()) != nullptr;

    if (onSection
        && (event->key() == Qt::Key_Escape
            || (event->key() == Qt::Key_Left && event->modifiers().testFlag(Qt::AltModifier)))) {
        showMainMenu();
        event->accept();
        return;
    }

    FramelessWindow::keyPressEvent(event);
}
