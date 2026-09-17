// MainWindow.h - The application workspace the user lands on after signing in.
//
// Shell comes from FramelessWindow. The window keeps a persistent branding rail
// on the left and a status strip along the bottom, and swaps the middle through
// a QStackedWidget: the main menu, then one page per section.
//
// Navigation lives here so the pages never have to know about each other.
#pragma once

#include "FramelessWindow.h"
#include "MenuCard.h"

#include <QHash>
#include <QString>

class BrandingPanel;
class EnhancePage;
class LicenseActivationDialog;
class LicenseClient;
struct LicenseResult;
class MainMenuPanel;
class OptimisePage;
class PanelSeparator;
class PerformPage;
class QStackedWidget;
class SectionPage;
class SettingsPage;
class StatusStrip;
class SystemInfoPage;
class AdminPage;

class MainWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setUserName(const QString &name);
    /// Reveals the owner-only admin console.
    void setOwner(bool owner);
    void setEntitlement(const QString &text);
    /// Locks / unlocks the workspace based on licence state.
    void setLicensed(bool licensed, const QString &entitlement);

public slots:
    /// Shows the section list.
    void showMainMenu();
    /// Navigates to \a section, or back to the menu if it has no page yet.
    void showSection(MenuSection section);

signals:
    /// The user wants to return to the sign-in screen.
    void signOutRequested();
    /// A page asked to restart the application with administrator rights.
    void elevationRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void buildUi();
    void connectSignals();
    void openActivationDialog();
    void applyMenuChrome();
    void applySectionChrome(SectionPage *page, int pillarIndex);

    BrandingPanel *m_brandingPanel = nullptr;
    PanelSeparator *m_verticalSeparator = nullptr;
    PanelSeparator *m_statusSeparator = nullptr;
    QStackedWidget *m_stack = nullptr;
    StatusStrip *m_statusStrip = nullptr;

    MainMenuPanel *m_menuPanel = nullptr;
    OptimisePage *m_optimisePage = nullptr;
    EnhancePage *m_enhancePage = nullptr;
    PerformPage *m_performPage = nullptr;
    SystemInfoPage *m_systemInfoPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    AdminPage *m_adminPage = nullptr;

    LicenseClient *m_license = nullptr;
    LicenseActivationDialog *m_activation = nullptr;
    bool m_licensed = false;
    bool m_isOwner = false;

    /// Sections that already have a page; anything else falls back to the menu.
    QHash<int, SectionPage *> m_pages;
};
