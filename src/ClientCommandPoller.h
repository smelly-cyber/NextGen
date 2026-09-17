// ClientCommandPoller.h - Receives admin commands for this running client.
//
// While the app is open it polls the server every so often. The server hands
// back any commands an owner has issued for this user (or everyone) plus the
// current disabled state and update announcement. The poller carries them out
// itself for the simple cases and signals the rest to the AppController.
#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QTimer;

class ClientCommandPoller : public QObject
{
    Q_OBJECT

public:
    explicit ClientCommandPoller(QObject *parent = nullptr);

    void start();
    void stop();

    /// Records a line in the server's activity feed for the signed-in user, so
    /// the owner's admin console reflects what users are actually doing.
    void reportActivity(const QString &kind, const QString &detail);

signals:
    /// An admin sent this user a message to display.
    void messageReceived(const QString &text);
    /// An update was published (shown once).
    void updateAvailable(const QString &version, const QString &notes, const QString &url,
                         const QString &sha256);
    /// This client has been disabled and must stop.
    void disableRequested();
    /// Wipe app data + licence and quit (irreversible on this device).
    void nukeRequested();
    /// Reset app data, settings and session (back to sign-in).
    void resetRequested();

private:
    void poll();
public:
    /// The stored session token (used by the nuke guard).
    QString token() const;

private:

    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_timer = nullptr;
    QString m_serverUrl;
    QSet<qint64> m_acked;      ///< Command ids carried out, sent on next poll.
    /// Version of the last update banner shown. Persisted, so a user who has
    /// already dismissed it does not meet it again on every sign-in.
    QString m_lastUpdateShown;
};
