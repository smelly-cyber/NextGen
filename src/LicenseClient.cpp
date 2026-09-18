#include "LicenseClient.h"

#include "HardwareId.h"
#include "LicensePublicKey.h"
#include "nglicense/License.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSysInfo>
#include <QUrl>

#include <functional>

namespace {
const QLatin1String kSessionTokenKey("license/sessionToken");
const QLatin1String kSessionUserKey("license/sessionUser");

// The owner access key. Not a signed licence - a memorable sentinel the server
// recognises to grant owner access. Must match `ownerKey` in the server exactly.
const QLatin1String kOwnerAccessKey("NGT-OWNER-ACCESS");
} // namespace

QString LicenseResult::entitlementSummary() const
{
    if (!ok)
        return reason;
    if (!licensed)
        return LicenseClient::tr("No licence — features locked");
    if (usesRemaining >= 0)
        return LicenseClient::tr("%n use(s) remaining", nullptr, int(usesRemaining));
    if (expiryUnix > 0) {
        const QDateTime expiry = QDateTime::fromSecsSinceEpoch(expiryUnix);
        return LicenseClient::tr("Licensed until %1").arg(expiry.toString(QStringLiteral("d MMM yyyy")));
    }
    return LicenseClient::tr("Lifetime licence");
}

LicenseClient::LicenseClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_serverUrl = qEnvironmentVariable(
        "NGT_AUTH_URL", QStringLiteral("https://nextgen-backend-n9i3.onrender.com"));
}

void LicenseClient::setServerUrl(const QString &url)
{
    m_serverUrl = url;
}

bool LicenseClient::isKeyWellFormed(const QString &licenseKey)
{
    const QString trimmed = licenseKey.trimmed();
    // The owner access key is a plain sentinel the server validates, not a signed
    // licence, so it would never pass the signature check. Let it through the
    // client-side gate so it reaches the server, which grants owner access.
    if (trimmed == kOwnerAccessKey)
        return true;
    // Offline Ed25519 signature check against the embedded issuer public key.
    return ngl::license::parseAndVerify(kNextGenLicensePublicKey, trimmed.toStdString())
        .has_value();
}

QString LicenseClient::machineId()
{
    // The licence is HWID-locked to the motherboard: the binding id is a salted
    // hash of the board's SMBIOS serial (falling back to the system UUID / OS
    // machine id). Hashing keeps the raw serial off the wire for the binding,
    // while the audit webhook reports the readable serial separately.
    QByteArray seed = HardwareId::stableId().toUtf8();
    seed.prepend("nextgen-tweaks-mb:");
    return QString::fromLatin1(
        QCryptographicHash::hash(seed, QCryptographicHash::Sha256).toHex().left(24));
}

bool LicenseClient::hasStoredSession() const
{
    QSettings settings;
    return !settings.value(kSessionTokenKey).toString().isEmpty();
}

void LicenseClient::clearStoredSession()
{
    QSettings settings;
    settings.remove(kSessionTokenKey);
    settings.remove(kSessionUserKey);
}

QString LicenseClient::storedUsername() const
{
    QSettings settings;
    return settings.value(kSessionUserKey).toString();
}

void LicenseClient::forgetSession()
{
    QSettings settings;
    settings.remove(kSessionTokenKey);
    settings.remove(kSessionUserKey);
}

void LicenseClient::storeSession(const QString &token, const QString &username)
{
    QSettings settings;
    if (token.isEmpty()) {
        settings.remove(kSessionTokenKey);
        settings.remove(kSessionUserKey);
    } else {
        settings.setValue(kSessionTokenKey, token);
        settings.setValue(kSessionUserKey, username);
    }
}

void LicenseClient::post(const QString &path, const QJsonObject &body,
                         const std::function<void(const QJsonObject &)> &onJson)
{
    emit started();

    QNetworkRequest request(QUrl(m_serverUrl + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(15000);

    QNetworkReply *reply =
        m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, onJson] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            // Keep this actionable: the raw Qt error string ("Connection
            // refused") tells the user nothing they can act on.
            emit failed(tr("Can't reach the licence server. Make sure it is running, then "
                           "try again."));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit failed(tr("The licence server returned an unexpected response."));
            return;
        }
        onJson(doc.object());
    });
}

void LicenseClient::handleAuthResponse(const QJsonObject &json)
{
    LicenseResult result;
    result.ok = json.value(QStringLiteral("ok")).toBool();
    result.reason = json.value(QStringLiteral("reason")).toString();

    if (!result.ok) {
        emit failed(result.reason.isEmpty() ? tr("Licence check failed.") : result.reason);
        return;
    }

    result.licensed = json.value(QStringLiteral("licensed")).toBool();
    result.owner = json.value(QStringLiteral("owner")).toBool();
    result.type = json.value(QStringLiteral("type")).toString();
    result.username = json.value(QStringLiteral("username")).toString();
    result.accountId = qint64(json.value(QStringLiteral("accountId")).toDouble());
    result.expiryUnix = qint64(json.value(QStringLiteral("expiryUnix")).toDouble());
    result.usesRemaining = json.contains(QStringLiteral("usesRemaining"))
                               ? qint64(json.value(QStringLiteral("usesRemaining")).toDouble())
                               : -1;

    const QString token = json.value(QStringLiteral("token")).toString();
    if (!token.isEmpty())
        storeSession(token, result.username);

    emit succeeded(result);
}

void LicenseClient::createAccount(const QString &username, const QString &email,
                                  const QString &password, const QString &licenseKey)
{
    // The licence is optional at sign-up: an account can be created unlicensed
    // and activated later from inside the app.
    if (!licenseKey.trimmed().isEmpty() && !isKeyWellFormed(licenseKey)) {
        emit failed(tr("That licence key is not valid. Check you copied it correctly."));
        return;
    }

    post(QStringLiteral("/v1/account/create"),
         {{"username", username},
          {"email", email},
          {"password", password},
          {"license", licenseKey.trimmed()},
          {"machine", machineId()}},
         [this](const QJsonObject &json) { handleAuthResponse(json); });
}

void LicenseClient::signIn(const QString &identifier, const QString &password,
                           const QString &licenseKey)
{
    if (!licenseKey.trimmed().isEmpty() && !isKeyWellFormed(licenseKey)) {
        emit failed(tr("That licence key is not valid. Check you copied it correctly."));
        return;
    }

    post(QStringLiteral("/v1/account/signin"),
         {{"identifier", identifier},
          {"password", password},
          {"license", licenseKey.trimmed()},
          {"machine", machineId()}},
         [this](const QJsonObject &json) { handleAuthResponse(json); });
}

void LicenseClient::redeemLicense(const QString &licenseKey)
{
    if (!isKeyWellFormed(licenseKey)) {
        emit failed(tr("That licence key is not valid. Check you copied it correctly."));
        return;
    }
    QSettings settings;
    const QString token = settings.value(kSessionTokenKey).toString();
    if (token.isEmpty()) {
        emit failed(tr("Your session has expired. Please sign in again."));
        return;
    }
    post(QStringLiteral("/v1/license/redeem"),
         {{"token", token}, {"license", licenseKey.trimmed()}, {"machine", machineId()}},
         [this](const QJsonObject &json) { handleAuthResponse(json); });
}

void LicenseClient::resumeSession()
{
    QSettings settings;
    const QString token = settings.value(kSessionTokenKey).toString();
    if (token.isEmpty()) {
        emit failed(tr("No saved session."));
        return;
    }

    post(QStringLiteral("/v1/session/validate"),
         {{"token", token}, {"machine", machineId()}},
         [this](const QJsonObject &json) {
             if (!json.value(QStringLiteral("ok")).toBool()) {
                 clearStoredSession();
                 emit failed(json.value(QStringLiteral("reason")).toString());
                 return;
             }
             handleAuthResponse(json);
         });
}
