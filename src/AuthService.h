// AuthService.h - Placeholder authentication back end.
//
// The login window talks to this object only through signals and slots, so the
// current stub can later be swapped for a real REST / WebSocket client without
// touching a single line of UI code.
#pragma once

#include <QObject>
#include <QString>

class QTimer;

/// Input checks shared by the UI and (later) the real back end.
namespace Validation {

/// True when \a value is a syntactically plausible e-mail address.
bool isEmail(const QString &value);

/// True when \a value is a usable user name (3+ chars, letters/digits/._-).
bool isUserName(const QString &value);

/// True when \a value is either a valid user name or a valid e-mail address.
bool isUserNameOrEmail(const QString &value);

/// Minimum accepted password length.
int minimumPasswordLength();

/// Turns a username or email address into a friendly display name
/// ("darcy.smith@mail.com" -> "Darcy.smith", "darcy" -> "Darcy").
QString displayNameFor(const QString &identifier);

} // namespace Validation

class AuthService : public QObject
{
    Q_OBJECT

public:
    explicit AuthService(QObject *parent = nullptr);
    ~AuthService() override;

    bool isBusy() const { return m_busy; }

public slots:
    /// Starts an asynchronous sign-in attempt. Never blocks the UI thread.
    void login(const QString &identifier, const QString &password, bool rememberMe);

    /// Aborts an in-flight request.
    void cancel();

    /// Placeholder hooks for the sign-up and password recovery flows.
    void requestRegistration();
    void requestPasswordReset(const QString &identifier);

signals:
    void loginStarted();
    void loginSucceeded(const QString &identifier, const QString &displayName);
    void loginFailed(const QString &reason);
    void busyChanged(bool busy);

    void registrationRequested();
    void passwordResetRequested(const QString &identifier);

private:
    void setBusy(bool busy);

    QTimer *m_requestTimer = nullptr;
    QString m_pendingIdentifier;
    bool m_busy = false;
};
