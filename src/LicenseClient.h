// LicenseClient.h - The desktop app's link to the licensing backend.
//
// This is the ONLY place the app talks to the NextGen Tweaks auth server. It
// performs two independent checks:
//   1. Offline: the pasted licence key must carry a valid Ed25519 signature
//      from the embedded issuer public key. A forged or mistyped key is
//      rejected instantly, before any network call.
//   2. Online: the server binds the key to the account + this machine, enforces
//      expiry / use limits / revocation, and returns a short session token.
//
// The token is cached (per user) so the app can silently re-validate on the
// next launch without asking for the password again.
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/// Outcome of a licensing call, mirrored from the server's JSON.
struct LicenseResult
{
    bool ok = false;        ///< Authenticated (signed in / account created).
    bool licensed = false;  ///< Has a live licence unlocking the features.
    bool owner = false;     ///< The owner account - unlocks the admin panel.
    QString reason;
    QString type;            ///< "duration" / "uses" / "lifetime".
    QString username;
    qint64 accountId = 0;
    qint64 expiryUnix = 0;   ///< 0 when not time-limited.
    qint64 usesRemaining = -1; ///< -1 when not use-limited.

    /// A friendly one-liner for the status strip, e.g. "12 uses left".
    QString entitlementSummary() const;
};

class LicenseClient : public QObject
{
    Q_OBJECT

public:
    explicit LicenseClient(QObject *parent = nullptr);

    /// Base URL of the auth server (NGT_AUTH_URL, else the hosted backend).
    void setServerUrl(const QString &url);
    QString serverUrl() const { return m_serverUrl; }

    /// Verifies a key's signature locally. Cheap, offline, no binding.
    static bool isKeyWellFormed(const QString &licenseKey);

    /// A stable, non-identifying id for this machine (hashed volume + user).
    static QString machineId();

    /// Do we have a cached session token to try a silent re-validation with?
    bool hasStoredSession() const;
    void clearStoredSession();
    QString storedUsername() const;

    /// Clears the saved session from anywhere (used on sign-out).
    static void forgetSession();

public slots:
    void createAccount(const QString &username, const QString &email, const QString &password,
                       const QString &licenseKey);
    void signIn(const QString &identifier, const QString &password, const QString &licenseKey);
    /// Redeems a licence for the already-signed-in session (the in-app prompt).
    void redeemLicense(const QString &licenseKey);
    /// Silent re-validation using the cached token.
    void resumeSession();

signals:
    void started();
    void succeeded(const LicenseResult &result);
    void failed(const QString &reason);

private:
    void post(const QString &path, const QJsonObject &body,
              const std::function<void(const QJsonObject &)> &onJson);
    void handleAuthResponse(const QJsonObject &json);
    void storeSession(const QString &token, const QString &username);

    QNetworkAccessManager *m_network = nullptr;
    QString m_serverUrl;
};
