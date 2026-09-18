#include "AdminClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStringList>
#include <QUrl>

namespace {
// Same keys LicenseClient uses, so the admin API rides the signed-in session.
const QLatin1String kSessionTokenKey("license/sessionToken");
} // namespace

AdminClient::AdminClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_serverUrl = qEnvironmentVariable(
        "NGT_AUTH_URL", QStringLiteral("https://nextgen-backend-n9i3.onrender.com"));
}

QString AdminClient::token() const
{
    return QSettings().value(kSessionTokenKey).toString();
}

bool AdminClient::hasSession() const
{
    return !token().isEmpty();
}

void AdminClient::request(const QString &path, const QJsonObject &body, const Callback &onReply)
{
    QJsonObject payload = body;
    payload.insert(QStringLiteral("token"), token());

    QNetworkRequest req{QUrl(m_serverUrl + path)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply *reply = m_network->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, onReply] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        QJsonObject json = QJsonDocument::fromJson(data).object();
        if (reply->error() != QNetworkReply::NoError && !json.contains(QStringLiteral("ok"))) {
            json = QJsonObject{{QStringLiteral("ok"), false},
                               {QStringLiteral("reason"),
                                reply->error() == QNetworkReply::ContentAccessDenied
                                    ? QObject::tr("Owner access required.")
                                    : QObject::tr("Could not reach the server.")}};
        }
        if (onReply)
            onReply(json);
    });
}

void AdminClient::licenses(const QString &query, const QString &status, int page,
                           const Callback &cb)
{
    request(QStringLiteral("/v1/admin/licenses"),
            {{QStringLiteral("query"), query},
             {QStringLiteral("status"), status},
             {QStringLiteral("page"), page}},
            cb);
}

void AdminClient::createLicense(const QString &kind, int durationDays, int uses,
                                const QString &note, const Callback &cb)
{
    request(QStringLiteral("/v1/admin/license/create"),
            {{QStringLiteral("kind"), kind},
             {QStringLiteral("durationDays"), durationDays},
             {QStringLiteral("uses"), uses},
             {QStringLiteral("note"), note}},
            cb);
}

void AdminClient::revoke(const QStringList &keyIds, const Callback &cb)
{
    request(QStringLiteral("/v1/admin/license/revoke"),
            {{QStringLiteral("keyIds"), QJsonArray::fromStringList(keyIds)}}, cb);
}

void AdminClient::command(const QString &kind, const QString &target, const QString &message,
                          const Callback &cb)
{
    request(QStringLiteral("/v1/admin/command"),
            {{QStringLiteral("kind"), kind},
             {QStringLiteral("target"), target},
             {QStringLiteral("message"), message}},
            cb);
}

void AdminClient::pushUpdate(const QString &version, const QString &notes, const QString &target,
                             const QString &url, const QString &sha256, const Callback &cb)
{
    request(QStringLiteral("/v1/admin/push-update"),
            {{QStringLiteral("version"), version},
             {QStringLiteral("notes"), notes},
             {QStringLiteral("target"), target},
             {QStringLiteral("url"), url},
             {QStringLiteral("sha256"), sha256}},
            cb);
}
