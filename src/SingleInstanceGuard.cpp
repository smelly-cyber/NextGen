#include "SingleInstanceGuard.h"

#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

namespace {
/// How long to wait for an answer when probing / pinging the primary instance.
constexpr int kConnectTimeoutMs = 250;
} // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString &key, int acquireTimeoutMs, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
    QElapsedTimer clock;
    clock.start();

    for (;;) {
        // Is somebody already listening? If so we are a duplicate launch.
        QLocalSocket probe;
        probe.connectToServer(m_key);
        const bool occupied = probe.waitForConnected(kConnectTimeoutMs);
        if (occupied)
            probe.disconnectFromServer();

        if (!occupied && tryListen()) {
            m_primary = true;
            return;
        }

        // Out of patience (or not waiting at all): report as a secondary.
        if (clock.elapsed() >= acquireTimeoutMs) {
            m_primary = false;
            return;
        }
        QThread::msleep(100);
    }
}

bool SingleInstanceGuard::tryListen()
{
    // A crash can leave a stale socket behind, which would block listening
    // forever, so clear it first. (No-op on Windows named pipes.)
    QLocalServer::removeServer(m_key);

    m_server = new QLocalServer(this);
    if (!m_server->listen(m_key)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server->nextPendingConnection())
            client->deleteLater();
        emit anotherInstanceStarted();
    });
    return true;
}

bool SingleInstanceGuard::notifyPrimary()
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(kConnectTimeoutMs))
        return false;

    socket.write("raise");
    socket.flush();
    socket.waitForBytesWritten(kConnectTimeoutMs);
    socket.disconnectFromServer();
    return true;
}
