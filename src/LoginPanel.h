// LoginPanel.h - Right hand side of the login window: the actual sign-in form.
//
// The panel owns no business logic; it validates its own inputs and then emits
// loginRequested() so that a service object (AuthService today, a real API
// client tomorrow) can do the work.
#pragma once

#include <QString>
#include <QWidget>

class LinkLabel;
class ModernButton;
class ModernCheckBox;
class ModernLineEdit;
class OrDivider;
class QLabel;

class LoginPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPanel(QWidget *parent = nullptr);

    QString identifier() const;
    QString password() const;
    bool rememberMe() const;

public slots:
    /// Moves the keyboard focus to the username field.
    void focusFirstField();

    /// Puts the form into / out of its loading state without blocking the UI.
    void setBusy(bool busy);
    void showError(const QString &message);
    void showInfo(const QString &message);
    void clearStatus();

signals:
    void loginRequested(const QString &identifier, const QString &password, bool rememberMe);
    void createAccountRequested();
    void forgotPasswordRequested(const QString &identifier);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildUi();
    void submit();
    bool validateInputs();

    QLabel *m_heading = nullptr;
    QLabel *m_subHeading = nullptr;
    ModernLineEdit *m_identifierField = nullptr;
    ModernLineEdit *m_passwordField = nullptr;
    ModernCheckBox *m_rememberMe = nullptr;
    LinkLabel *m_forgotPassword = nullptr;
    QLabel *m_status = nullptr;
    ModernButton *m_loginButton = nullptr;
    OrDivider *m_divider = nullptr;
    ModernButton *m_createAccountButton = nullptr;

    bool m_busy = false;
};
