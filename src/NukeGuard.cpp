#include "NukeGuard.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {

/// The marker lives outside QSettings so a settings wipe cannot remove it, and
/// in a fixed per-user location so it survives everything except an explicit
/// restore.
QString markerPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    QDir().mkpath(dir + QStringLiteral("/NextgenTweaks"));
    return dir + QStringLiteral("/NextgenTweaks/client.lock");
}

QString serverUrl()
{
    return qEnvironmentVariable(
        "NGT_AUTH_URL", QStringLiteral("https://nextgen-backend-n9i3.onrender.com"));
}

} // namespace

bool NukeGuard::isEngaged()
{
    return QFile::exists(markerPath());
}

void NukeGuard::engage(const QString &sessionToken)
{
    QFile file(markerPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(sessionToken.toUtf8());
        file.close();
    }
}

void NukeGuard::disengage()
{
    QFile::remove(markerPath());
}

QString NukeGuard::storedToken()
{
    QFile file(markerPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool NukeGuard::stillNukedOnServer()
{
    // Prefer the CURRENT signed-in session over whatever token the marker was
    // written with. This is what lets a not-nuked user (the owner especially)
    // start on a machine that still carries a marker from a different, nuked
    // account: their own status is what decides, not the marker's.
    QString token = QSettings().value(QStringLiteral("license/sessionToken")).toString();
    if (token.isEmpty())
        token = storedToken();
    if (token.isEmpty())
        return true; // No way to prove a restore, so stay nuked.

    QNetworkAccessManager network;
    QNetworkRequest request{QUrl(serverUrl() + QStringLiteral("/v1/client/nukestate"))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    const QJsonObject body{{QStringLiteral("token"), token}};
    QNetworkReply *reply = network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(4000);
    loop.exec();

    bool stillNuked = true; // Unreachable / malformed => fail closed.
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        // Only an explicit nuked:false lifts the gate.
        stillNuked = json.value(QStringLiteral("nuked")).toBool(true);
    }
    if (!reply->isFinished())
        reply->abort();
    reply->deleteLater();
    return stillNuked;
}
