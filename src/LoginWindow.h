// LoginWindow.h - The sign-in screen.
//
// All of the window chrome (rounded body, blue outline, glow, dragging, edge
// resizing, minimise / close) comes from FramelessWindow; this class owns the
// branding panel, the login form and the wiring to the licensing backend
// (LicenseClient), which validates the account + licence against the server.
//
// By design the login window can never be maximised or made full screen - it is
// a fixed-format sign-in dialog.
#pragma once

#include "FramelessWindow.h"

#include <QString>

class BrandingPanel;
class CreateAccountDialog;
class LicenseClient;
struct LicenseResult;
class LoginPanel;
class PanelSeparator;

class LoginWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

    /// Tries to silently resume a saved session; on success signedIn() fires.
    void tryResumeSession();

signals:
    /// Emitted once the user is authenticated and licensed; carries the display
    /// name and a short entitlement summary for the workspace.
    void signedIn(const QString &displayName, bool licensed, bool owner, const QString &entitlement);

private:
    void buildUi();
    void connectSignals();
    void openCreateAccountDialog();

    BrandingPanel *m_brandingPanel = nullptr;
    PanelSeparator *m_separator = nullptr;
    LoginPanel *m_loginPanel = nullptr;

    LicenseClient *m_license = nullptr;
    CreateAccountDialog *m_createDialog = nullptr;

    /// True while the automatic "resume my session" request is in flight. A
    /// failure there is silent: the user has not asked for anything yet, so the
    /// form simply stays ready instead of showing an alarming error.
    bool m_resumingSession = false;
};
