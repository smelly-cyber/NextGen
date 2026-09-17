#include "ClientCommandPoller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace {
const QLatin1String kSessionTokenKey("license/sessionToken");
/// Remembers the update already shown, so it is announced once and not again on
/// the next launch.
const QLatin1String kLastUpdateKey("updates/lastShownVersion");
constexpr int kPollIntervalMs = 30000;
/// A command is only forgotten by the server once we acknowledge it. Waiting a
/// full poll interval to do that means closing the app first would make the
/// same message reappear next time, so the acknowledgement is flushed promptly.
constexpr int kAckFlushMs = 1200;
} // namespace

ClientCommandPoller::ClientCommandPoller(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    m_serverUrl = qEnvironmentVariable("NGT_AUTH_URL", QStringLiteral("http://127.0.0.1:8787"));
    m_lastUpdateShown = QSettings().value(kLastUpdateKey).toString();
    m_timer->setInterval(kPollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &ClientCommandPoller::poll);
}

QString ClientCommandPoller::token() const
{
    return QSettings().value(kSessionTokenKey).toString();
}

void ClientCommandPoller::start()
{
    if (token().isEmpty())
        return;
    poll(); // Once immediately, then on the timer.
    m_timer->start();
}

void ClientCommandPoller::stop()
{
    m_timer->stop();
}

void ClientCommandPoller::reportActivity(const QString &kind, const QString &detail)
{
    const QString sessionToken = token();
    if (sessionToken.isEmpty())
        return;
    const QJsonObject body{{QStringLiteral("token"), sessionToken},
                           {QStringLiteral("kind"), kind},
                           {QStringLiteral("detail"), detail}};
    QNetworkRequest req{QUrl(m_serverUrl + QStringLiteral("/v1/client/activity"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void ClientCommandPoller::poll()
{
    const QString sessionToken = token();
    if (sessionToken.isEmpty())
        return;

    QJsonArray ack;
    for (qint64 id : m_acked)
        ack.append(id);

    const QJsonObject body{{QStringLiteral("token"), sessionToken},
                           {QStringLiteral("machine"), QString()},
                           {QStringLiteral("ack"), ack}};

    QNetworkRequest req{QUrl(m_serverUrl + QStringLiteral("/v1/client/poll"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply *reply = m_network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        if (!json.value(QStringLiteral("ok")).toBool())
            return;

        // Commands the server confirmed it has received (in our ack list) can be
        // forgotten now, so the set does not grow without bound.
        // The server only re-sends commands we have not acked, so anything not
        // present this round is done.
        const QJsonArray commands = json.value(QStringLiteral("commands")).toArray();
        QSet<qint64> stillPending;
        for (const QJsonValue &value : commands)
            stillPending.insert(qint64(value.toObject().value(QStringLiteral("id")).toDouble()));
        m_acked.intersect(stillPending);

        bool nuke = false, reset = false, disable = false;
        bool newlyHandled = false;
        for (const QJsonValue &value : commands) {
            const QJsonObject command = value.toObject();
            const qint64 id = qint64(command.value(QStringLiteral("id")).toDouble());
            if (m_acked.contains(id))
                continue; // Already carried out, waiting for the server to drop it.
            const QString kind = command.value(QStringLiteral("kind")).toString();
            const QString payload = command.value(QStringLiteral("payload")).toString();

            if (kind == QLatin1String("message"))
                emit messageReceived(payload);
            else if (kind == QLatin1String("nuke"))
                nuke = true;
            else if (kind == QLatin1String("reset"))
                reset = true;
            else if (kind == QLatin1String("disable"))
                disable = true;

            m_acked.insert(id);
            newlyHandled = true;
        }

        // Tell the server straight away that these are done. Without this the
        // receipt would wait a full poll interval, and quitting before then
        // would make the same message or update show up again next launch.
        if (newlyHandled)
            QTimer::singleShot(kAckFlushMs, this, &ClientCommandPoller::poll);

        if (json.value(QStringLiteral("disabled")).toBool())
            disable = true;

        // Destructive actions win and are emitted once; the AppController quits.
        if (nuke)
            emit nukeRequested();
        else if (reset)
            emit resetRequested();
        else if (disable)
            emit disableRequested();

        const QJsonObject update = json.value(QStringLiteral("update")).toObject();
        const QString version = update.value(QStringLiteral("version")).toString();
        if (!version.isEmpty() && version != m_lastUpdateShown) {
            m_lastUpdateShown = version;
            QSettings().setValue(kLastUpdateKey, version);
            emit updateAvailable(version, update.value(QStringLiteral("notes")).toString(),
                                 update.value(QStringLiteral("url")).toString(),
                                 update.value(QStringLiteral("sha256")).toString());
        }
    });
}
