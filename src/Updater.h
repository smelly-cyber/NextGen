// Updater.h - Silent in-place auto-update for the desktop client.
//
// When an admin pushes a new version with a download URL, each running client
// fetches the new build, verifies it, and swaps itself for it on the next
// restart. The running executable cannot overwrite its own file while it is in
// use, so a short helper script (spawned detached) waits for this process to
// exit, replaces the exe, and relaunches the new one.
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(QObject *parent = nullptr);

    /// Considers an update advertised by the server. Downloads and installs it
    /// only when \a version differs from the running build and \a url is a
    /// direct https link. \a sha256 (optional) is verified before anything runs.
    /// Does nothing if an update is already in progress.
    void offer(const QString &version, const QString &url, const QString &sha256);

    /// Fully automatic update: looks up the latest release of the built-in
    /// GitHub repository, and if it is newer than this build, downloads the
    /// bundled .exe and installs it. No URL, no admin action, no user download.
    /// Safe to call on every launch.
    void checkAutomatic();

    /// True when \a candidate is a strictly newer version string than \a current
    /// (dotted numeric compare, a leading "v" ignored).
    static bool isNewer(const QString &candidate, const QString &current);

signals:
    /// A newer build is being downloaded/installed (for a status message).
    void updating(const QString &version);
    /// The update is staged; the app is about to restart into the new version.
    void restarting();
    /// The update could not be applied (left the current build untouched).
    void failed(const QString &reason);

private:
    void download(const QString &version, const QString &url, const QString &sha256);
    bool stageAndRestart(const QString &downloadedExe);

    QNetworkAccessManager *m_network = nullptr;
    bool m_busy = false;
};
