#include "Updater.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace {
/// Only https downloads are ever run.
bool isHttps(const QUrl &url)
{
    return url.isValid() && url.scheme() == QLatin1String("https");
}

// The built-in update source. Publish a new GitHub Release here (with the new
// NextGenTweaks.exe attached as an asset) and every client installs it on its
// own - no URL to paste, nothing for users to download. Change this one line if
// the repository ever moves.
const QLatin1String kUpdateRepo("smelly-cyber/NextGen");
} // namespace

bool Updater::isNewer(const QString &candidate, const QString &current)
{
    auto parts = [](QString v) {
        v = v.trimmed();
        if (v.startsWith(QLatin1Char('v')) || v.startsWith(QLatin1Char('V')))
            v.remove(0, 1);
        QList<int> out;
        for (const QString &p : v.split(QLatin1Char('.')))
            out << p.section(QLatin1Char('-'), 0, 0).toInt();
        return out;
    };
    const QList<int> a = parts(candidate);
    const QList<int> b = parts(current);
    for (int i = 0; i < qMax(a.size(), b.size()); ++i) {
        const int x = i < a.size() ? a.at(i) : 0;
        const int y = i < b.size() ? b.at(i) : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void Updater::offer(const QString &version, const QString &url, const QString &sha256)
{
    if (m_busy)
        return;
    if (!isNewer(version, QCoreApplication::applicationVersion()))
        return; // Same or older build - nothing to do.
    const QUrl link(url.trimmed());
    if (!isHttps(link))
        return; // No usable download; the banner already told the user.

    m_busy = true;
    emit updating(version);
    download(version, url.trimmed(), sha256.trimmed().toLower());
}

void Updater::checkAutomatic()
{
    if (m_busy)
        return;

    const QUrl api(QStringLiteral("https://api.github.com/repos/%1/releases/latest")
                       .arg(QString(kUpdateRepo)));
    QNetworkRequest request(api);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NextGenTweaks"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return; // Offline or rate-limited: try again next launch.

        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (!isNewer(tag, QCoreApplication::applicationVersion()))
            return; // Already up to date.

        // Find the Windows .exe asset and its GitHub-published SHA-256.
        QString url, sha;
        for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (!name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive))
                continue;
            const QUrl candidate(asset.value(QStringLiteral("browser_download_url")).toString());
            if (!isHttps(candidate))
                continue;
            url = candidate.toString();
            const QString digest = asset.value(QStringLiteral("digest")).toString();
            if (digest.startsWith(QLatin1String("sha256:")))
                sha = digest.mid(7);
            break;
        }
        if (!url.isEmpty())
            offer(tag, url, sha);
    });
}

void Updater::download(const QString &version, const QString &url, const QString &sha256)
{
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NextGenTweaks"));

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, version, sha256] {
        reply->deleteLater();
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            emit failed(tr("Could not download the update: %1").arg(reply->errorString()));
            return;
        }
        // Every redirect must stay on https.
        if (!isHttps(reply->url())) {
            emit failed(tr("The update download was redirected to an insecure location."));
            return;
        }
        const QByteArray payload = reply->readAll();
        if (payload.size() < 1024 || !payload.startsWith("MZ")) {
            emit failed(tr("The update download was not a valid program."));
            return;
        }
        if (!sha256.isEmpty()) {
            const QString got = QString::fromLatin1(
                QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
            if (got != sha256) {
                emit failed(tr("The update failed its checksum and was not installed."));
                return;
            }
        }

        // Write the new build beside a temp folder, then stage the swap.
        const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                .filePath(QStringLiteral("NextGenTweaks-update"));
        QDir().mkpath(dir);
        const QString newExe = QDir(dir).filePath(QStringLiteral("NextGenTweaks-new.exe"));
        QFile out(newExe);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || out.write(payload) != payload.size()) {
            emit failed(tr("Could not save the update."));
            return;
        }
        out.close();

        if (stageAndRestart(newExe))
            emit restarting();
        else
            emit failed(tr("Could not stage the update."));
    });
}

bool Updater::stageAndRestart(const QString &downloadedExe)
{
#ifdef Q_OS_WIN
    // A running executable is locked, so it cannot copy the new build over
    // itself. A detached cmd waits for this process id to disappear, replaces
    // the exe, relaunches it, and removes itself. Everything stays inside the
    // app's own install directory and temp folder.
    const QString target = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString source = QDir::toNativeSeparators(downloadedExe);
    const qint64 pid = QCoreApplication::applicationPid();

    const QString scriptPath =
        QDir(QFileInfo(downloadedExe).absolutePath()).filePath(QStringLiteral("apply-update.cmd"));
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    // Wait for the old process to exit, swap the file (retrying while the old
    // image is still unloading), relaunch, then delete this script.
    const QString cmd = QStringLiteral(
        "@echo off\r\n"
        "setlocal\r\n"
        ":waitloop\r\n"
        "tasklist /FI \"PID eq %1\" | find \"%1\" >nul 2>&1\r\n"
        "if not errorlevel 1 (\r\n"
        "  timeout /t 1 /nobreak >nul\r\n"
        "  goto waitloop\r\n"
        ")\r\n"
        ":copyloop\r\n"
        "copy /y \"%2\" \"%3\" >nul 2>&1\r\n"
        "if errorlevel 1 (\r\n"
        "  timeout /t 1 /nobreak >nul\r\n"
        "  goto copyloop\r\n"
        ")\r\n"
        "start \"\" \"%3\"\r\n"
        "del \"%2\" >nul 2>&1\r\n"
        "del \"%%~f0\" >nul 2>&1\r\n")
        .arg(QString::number(pid), source, target);
    script.write(cmd.toUtf8());
    script.close();

    // Launch the helper detached so it outlives this process.
    return QProcess::startDetached(QStringLiteral("cmd.exe"),
                                   {QStringLiteral("/c"), QDir::toNativeSeparators(scriptPath)});
#else
    Q_UNUSED(downloadedExe)
    return false;
#endif
}
