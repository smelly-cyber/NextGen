// SingleInstanceGuard.h - Allows only one copy of the app to run at a time.
//
// The first process to start listens on a named local socket and becomes the
// "primary" instance. Any later launch finds that socket, pings it (which brings
// the running window to the front) and exits immediately.
//
// The application's own relaunch paths - the elevated restart and the
// "Restart Now" in Settings - start a new process while the outgoing one is
// still alive, so they pass kRelaunchFlag. That tells the newcomer to wait for
// the old instance to release the socket and take ownership itself, instead of
// mistaking it for a duplicate launch and quitting.
#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

/// Command-line flag used by the app's own relaunch paths.
inline constexpr char kRelaunchFlag[] = "--relaunch";

class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    /// Tries to claim ownership of \a key. When \a acquireTimeoutMs is greater
    /// than zero the constructor keeps retrying for that long, which is what a
    /// relaunch needs while the previous process shuts down.
    explicit SingleInstanceGuard(const QString &key, int acquireTimeoutMs = 0,
                                 QObject *parent = nullptr);

    /// True when this process owns the lock and should carry on starting up.
    bool isPrimary() const { return m_primary; }

    /// Secondary instances call this to ask the running copy to show itself.
    bool notifyPrimary();

signals:
    /// Emitted on the primary instance when another launch was attempted.
    void anotherInstanceStarted();

private:
    bool tryListen();

    QString m_key;
    QLocalServer *m_server = nullptr;
    bool m_primary = false;
};
