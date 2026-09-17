#include "GpuTweaks.h"

#include "TweakBackupStore.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace {

const QLatin1String kDisplayClassKey(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\"
    "{4d36e968-e325-11ce-bfc1-08002be10318}");
const QLatin1String kNvidiaService(
    "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\nvlddmkm");
const QLatin1String kNvidiaControlPanelClient(
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\NVIDIA Corporation\\NvControlPanel2\\Client");
const QLatin1String kNvidiaFts("HKEY_LOCAL_MACHINE\\SOFTWARE\\NVIDIA Corporation\\Global\\FTS");

// =============================================================================
//  NVIDIA PROFILE IMPORT LINKS - paste your two GitHub links here.
//
//  kInspectorLink  NVIDIA Profile Inspector. Either a release asset
//                  (.../releases/download/<tag>/<file>.zip or .exe) or a file
//                  in a repo (.../blob/<branch>/<file>.zip or .exe).
//  kProfileLink    Your .nip profile, e.g. .../blob/<branch>/<name>.nip
//
//  Both must be https GitHub links. "blob" page links are converted to their
//  raw download automatically. Leave kInspectorLink empty to use the official
//  Profile Inspector release instead. Leave kProfileLink empty to fall back to
//  a .nip placed in profiles\nvidia\ beside the executable.
// =============================================================================
const QLatin1String kInspectorLink(
    "https://raw.githubusercontent.com/smelly-cyber/NextGen/main/nvidiaProfileInspector.exe");
const QLatin1String kProfileLink(
    "https://raw.githubusercontent.com/smelly-cyber/NextGen/main/profile.nip");

/// SHA-256 the Profile Inspector download must match, whatever the link is.
///
/// A file stored in a repository (rather than attached to a Release) has no
/// checksum published by GitHub, so without this the only assurance would be
/// that the bytes arrived over HTTPS from GitHub. Since this executable is then
/// run with administrator rights, it is pinned instead: if the file in the repo
/// is ever replaced - by you or by anyone who gets into the account - the
/// download stops rather than running.
///
/// Re-uploading nvidiaProfileInspector.exe means updating this value. Get the
/// new one with:  certutil -hashfile nvidiaProfileInspector.exe SHA256
/// Leave it empty to skip the check entirely.
const QLatin1String kInspectorPinnedSha256(
    "071f38bfeec4fab0a33261f7fa010940e2c3c96fb21f03d18d490110ddb90b03");

// --- Profile Inspector defaults ------------------------------------------------

const QLatin1String kInspectorLatestRelease(
    "https://api.github.com/repos/Orbmu2k/nvidiaProfileInspector/releases/latest");
/// Used only if the GitHub API cannot be reached. The hash was taken from the
/// digest GitHub publishes for this exact release asset.
const QLatin1String kInspectorFallbackUrl(
    "https://github.com/Orbmu2k/nvidiaProfileInspector/releases/download/v3.0.2.1/"
    "nvidiaProfileInspector.zip");
const QLatin1String kInspectorFallbackSha256(
    "88dcf3514111e8de630688467c03c36d8c2a8ad9ebc8073f27c069f82b75bb40");

/// What was last imported: the profile link, or the SHA-256 of a local file.
const QLatin1String kImportedMarkerKey("gpu/nvidiaProfileImported");

/// Hosts a pasted link (or the redirect it follows) is allowed to resolve to.
bool isGitHubHost(const QString &host)
{
    return host == QLatin1String("github.com") || host == QLatin1String("api.github.com")
           || host == QLatin1String("raw.githubusercontent.com")
           || host == QLatin1String("objects.githubusercontent.com")
           || host == QLatin1String("release-assets.githubusercontent.com")
           || host == QLatin1String("codeload.github.com");
}

/// Turns whatever GitHub link was pasted into a direct download: a "blob" page
/// becomes its raw file. Returns an invalid URL for anything that is not an
/// https GitHub link.
QUrl directGitHubDownload(const QString &link)
{
    QUrl url(link.trimmed());
    if (!url.isValid() || url.scheme() != QLatin1String("https") || !isGitHubHost(url.host()))
        return QUrl();

    // github.com/<owner>/<repo>/blob/<ref>/<path>
    //   -> raw.githubusercontent.com/<owner>/<repo>/<ref>/<path>
    const QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (url.host() == QLatin1String("github.com") && parts.size() >= 5
        && parts.at(2) == QLatin1String("blob")) {
        QStringList raw = parts;
        raw.removeAt(2);
        QUrl direct;
        direct.setScheme(QStringLiteral("https"));
        direct.setHost(QStringLiteral("raw.githubusercontent.com"));
        direct.setPath(QLatin1Char('/') + raw.join(QLatin1Char('/')));
        return direct;
    }
    return url;
}

/// For a GitHub release asset link, the release API entry that describes it
/// (and therefore publishes its SHA-256). Empty for any other kind of link.
QUrl releaseApiFor(const QUrl &asset, QString *assetName)
{
    const QStringList parts = asset.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (asset.host() != QLatin1String("github.com") || parts.size() < 5
        || parts.at(2) != QLatin1String("releases"))
        return QUrl();

    const QString base = QStringLiteral("https://api.github.com/repos/%1/%2/releases/")
                             .arg(parts.at(0), parts.at(1));
    // .../releases/download/<tag>/<asset>
    if (parts.size() == 6 && parts.at(3) == QLatin1String("download")) {
        *assetName = parts.at(5);
        return QUrl(base + QStringLiteral("tags/") + parts.at(4));
    }
    // .../releases/latest/download/<asset>
    if (parts.size() == 6 && parts.at(3) == QLatin1String("latest")) {
        *assetName = parts.at(5);
        return QUrl(base + QStringLiteral("latest"));
    }
    return QUrl();
}

constexpr int kNetworkTimeoutMs = 60000;
constexpr int kImportTimeoutMs = 60000;

/// Blocking GET for use on the tweak engine's worker thread. That thread has no
/// running event loop of its own, so a local one waits for the reply.
QByteArray httpGet(const QUrl &url, QString *error)
{
    QNetworkAccessManager network;
    QNetworkRequest request(url);
    // GitHub's API rejects requests without a User-Agent.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NextGenTweaks"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    // Release downloads redirect from github.com to GitHub's storage host; only
    // HTTPS-to-HTTPS redirects are followed.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = network.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(kNetworkTimeoutMs);
    loop.exec();

    QByteArray body;
    if (!reply->isFinished()) {
        reply->abort();
        if (error)
            *error = QObject::tr("Timed out downloading %1.").arg(url.host());
    } else if (reply->error() != QNetworkReply::NoError) {
        if (error)
            *error = QObject::tr("Could not download from %1: %2")
                         .arg(url.host(), reply->errorString());
    } else if (!isGitHubHost(reply->url().host())) {
        // Wherever the redirects led, the file must still be served by GitHub.
        if (error)
            *error = QObject::tr("The download was redirected away from GitHub (%1) and was "
                                 "not used.")
                         .arg(reply->url().host());
    } else {
        body = reply->readAll();
    }
    reply->deleteLater();
    return body;
}

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

QString appSettingsValue(const QString &key)
{
    const QSettings settings;
    return settings.value(key).toString();
}

void setAppSettingsValue(const QString &key, const QString &value)
{
    QSettings settings;
    if (value.isEmpty())
        settings.remove(key);
    else
        settings.setValue(key, value);
    settings.sync();
}

/// Deletes a folder, retrying briefly: Windows can keep an executable's image
/// locked for a moment after its process has exited.
bool removeFolder(const QString &path)
{
    for (int attempt = 0; attempt < 10; ++attempt) {
        QDir dir(path);
        if (!dir.exists() || dir.removeRecursively())
            return true;
        QThread::msleep(300);
    }
    return !QDir(path).exists();
}

} // namespace

// ---------------------------------------------------------- GpuDetection ----

QVector<GpuAdapter> GpuDetection::adapters()
{
    QVector<GpuAdapter> found;
#ifdef _WIN32
    const QSettings classKey(QString(kDisplayClassKey), QSettings::NativeFormat);
    for (const QString &instance : classKey.childGroups()) {
        if (instance.size() != 4)
            continue; // Numbered driver instances only.

        const QString path = QString(kDisplayClassKey) + QLatin1Char('\\') + instance;
        const QSettings entry(path, QSettings::NativeFormat);
        const QString provider = entry.value(QStringLiteral("ProviderName")).toString();
        const QString matchingId = entry.value(QStringLiteral("MatchingDeviceId")).toString();

        GpuAdapter adapter;
        adapter.name = entry.value(QStringLiteral("DriverDesc")).toString();
        adapter.driverKey = path;

        // PCI vendor IDs are the reliable signal; the provider name is a
        // fallback for drivers that do not record a matching device id.
        if (matchingId.contains(QLatin1String("ven_10de"), Qt::CaseInsensitive)
            || provider.contains(QLatin1String("NVIDIA"), Qt::CaseInsensitive)) {
            adapter.vendor = GpuVendor::Nvidia;
        } else if (matchingId.contains(QLatin1String("ven_1002"), Qt::CaseInsensitive)
                   || provider.contains(QLatin1String("Advanced Micro Devices"),
                                        Qt::CaseInsensitive)
                   || provider == QLatin1String("AMD")) {
            adapter.vendor = GpuVendor::Amd;
        } else {
            continue;
        }
        found.append(adapter);
    }
#endif
    return found;
}

bool GpuDetection::hasVendor(GpuVendor vendor)
{
    for (const GpuAdapter &adapter : adapters()) {
        if (adapter.vendor == vendor)
            return true;
    }
    return false;
}

// ----------------------------------------------------------- GpuDriver ------

GpuDriverTweak::GpuDriverTweak(QString id, QString title, QString description)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::Machine)
{
    setRequiresReboot(true); // Driver settings are read when the driver loads.
}

RegistryTweak GpuDriverTweak::resolved() const
{
    QVector<RegistryEntry> entries;
    bool nvidiaAdded = false;

    for (const GpuAdapter &adapter : GpuDetection::adapters()) {
        if (adapter.vendor == GpuVendor::Nvidia) {
            if (nvidiaAdded)
                continue; // These are driver-wide, not per card.
            nvidiaAdded = true;
            entries.append({QString(kNvidiaService),
                            QStringLiteral("RmGpsPsEnablePerCpuCoreDpc"), 1, 0});
            entries.append({QString(kNvidiaControlPanelClient),
                            QStringLiteral("OptInOrOutPreference"), 0, 1});
            entries.append({QString(kNvidiaFts), QStringLiteral("EnableRID44231"), 0, 1});
            entries.append({QString(kNvidiaFts), QStringLiteral("EnableRID64640"), 0, 1});
            entries.append({QString(kNvidiaFts), QStringLiteral("EnableRID66610"), 0, 1});
        } else {
            entries.append({adapter.driverKey, QStringLiteral("EnableUlps"), 0, 1});
            entries.append({adapter.driverKey, QStringLiteral("PP_SclkDeepSleepDisable"), 1, 0});
            entries.append({adapter.driverKey, QStringLiteral("KMD_FRTEnabled"), 0, 0});
        }
    }

    return RegistryTweak(id(), title(), description(), scope(), entries);
}

QStringList GpuDriverTweak::plannedChanges() const
{
    const QVector<GpuAdapter> adapters = GpuDetection::adapters();
    if (adapters.isEmpty())
        return {QObject::tr("No NVIDIA or AMD graphics card found - GPU driver settings skipped.")};

    QStringList lines;
    for (const GpuAdapter &adapter : adapters)
        lines << QObject::tr("Graphics card detected: %1").arg(adapter.name);
    lines << resolved().plannedChanges();
    return lines;
}

bool GpuDriverTweak::isApplied() const
{
    // Nothing to apply on a machine without a supported card counts as done.
    if (GpuDetection::adapters().isEmpty())
        return true;
    return resolved().isApplied();
}

bool GpuDriverTweak::apply(TweakBackupStore &store, QString *error)
{
    if (GpuDetection::adapters().isEmpty())
        return true;
    RegistryTweak writes = resolved();
    return writes.apply(store, error);
}

bool GpuDriverTweak::revert(TweakBackupStore &store, QString *error)
{
    RegistryTweak writes = resolved();
    return writes.revert(store, error);
}

// ---------------------------------------------------- NvidiaProfileImport ---

NvidiaProfileTweak::NvidiaProfileTweak(QString id, QString title, QString description)
    : Tweak(std::move(id), std::move(title), std::move(description), Scope::Machine)
{
}

QString NvidiaProfileTweak::profileDirectory()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("profiles/nvidia"));
}

QString NvidiaProfileTweak::profilePath()
{
    const QDir dir(profileDirectory());
    const QFileInfoList profiles =
        dir.entryInfoList({QStringLiteral("*.nip")}, QDir::Files, QDir::Time);
    return profiles.isEmpty() ? QString() : profiles.first().absoluteFilePath();
}

namespace {

/// Where the profile comes from, resolved once per call.
struct ProfileSource
{
    QUrl link;          ///< Set when a profile link is configured.
    QString localPath;  ///< Set when falling back to a local file.
    bool linkInvalid = false;

    bool isEmpty() const { return link.isEmpty() && localPath.isEmpty(); }
};

ProfileSource profileSource()
{
    ProfileSource source;
    const QString link = QString(kProfileLink).trimmed();
    if (!link.isEmpty()) {
        source.link = directGitHubDownload(link);
        source.linkInvalid = !source.link.isValid();
        return source;
    }
    source.localPath = NvidiaProfileTweak::profilePath();
    return source;
}

/// A marker that changes whenever a different profile would be imported.
QString profileMarker(const ProfileSource &source)
{
    if (!source.link.isEmpty())
        return source.link.toString();
    return source.localPath.isEmpty() ? QString() : fileSha256(source.localPath);
}

/// Magic bytes of the two formats Profile Inspector is distributed in.
bool looksLikeZip(const QByteArray &data) { return data.startsWith("PK\x03\x04"); }
bool looksLikeExe(const QByteArray &data) { return data.startsWith("MZ"); }

} // namespace

bool NvidiaProfileTweak::isAvailable() const
{
    return GpuDetection::hasVendor(GpuVendor::Nvidia);
}

QString NvidiaProfileTweak::unavailableReason() const
{
    return QObject::tr("Requires an NVIDIA graphics card.");
}

QStringList NvidiaProfileTweak::plannedChanges() const
{
    if (!GpuDetection::hasVendor(GpuVendor::Nvidia))
        return {QObject::tr("No NVIDIA graphics card found - profile import skipped.")};

    const ProfileSource source = profileSource();
    if (source.linkInvalid)
        return {QObject::tr("The NVIDIA profile link is not an https GitHub link - import skipped.")};
    if (source.isEmpty()) {
        return {QObject::tr("No NVIDIA profile configured - import skipped.")};
    }

    const QString profileName = source.link.isEmpty()
                                    ? QFileInfo(source.localPath).fileName()
                                    : source.link.fileName();

    return {QObject::tr("Import NVIDIA profile: %1").arg(profileName),
            QObject::tr("    Downloads NVIDIA Profile Inspector%1 from GitHub, runs a silent "
                        "import, then closes it and deletes everything it downloaded.")
                .arg(source.link.isEmpty() ? QString() : QObject::tr(" and the profile")),
            QObject::tr("    Note: the import replaces NVIDIA driver profile settings and "
                        "cannot be undone by Revert. Use \"Restore defaults\" in the NVIDIA "
                        "Control Panel to reset them.")};
}

bool NvidiaProfileTweak::isApplied() const
{
    if (!GpuDetection::hasVendor(GpuVendor::Nvidia))
        return true;
    const ProfileSource source = profileSource();
    if (source.isEmpty() || source.linkInvalid)
        return true;
    // Applied means this exact profile has already been imported.
    const QString imported = appSettingsValue(QString(kImportedMarkerKey));
    return !imported.isEmpty() && imported == profileMarker(source);
}

bool NvidiaProfileTweak::apply(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store) // A driver profile import has no registry value to record.

    if (!GpuDetection::hasVendor(GpuVendor::Nvidia))
        return true;

    const ProfileSource source = profileSource();
    if (source.linkInvalid) {
        if (error)
            *error = QObject::tr("The NVIDIA profile link must be an https GitHub link.");
        return false;
    }
    if (source.isEmpty())
        return true;

    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    // Everything downloaded lives in one private folder that is deleted at the
    // end, whether the import worked or not.
    QTemporaryDir workspace(QDir::temp().filePath(QStringLiteral("NextGenTweaks-npi-XXXXXX")));
    if (!workspace.isValid())
        return fail(QObject::tr("Could not create a temporary folder for the NVIDIA import."));
    workspace.setAutoRemove(false); // removeFolder() below retries around file locks.
    const QString workPath = workspace.path();
    const QDir work(workPath);

    const auto cleanUp = [&workPath] { removeFolder(workPath); };

    // --- 1. The profile -----------------------------------------------------
    QString profileFile = source.localPath;
    if (!source.link.isEmpty()) {
        QString downloadError;
        const QByteArray nip = httpGet(source.link, &downloadError);
        if (nip.isEmpty()) {
            cleanUp();
            return fail(downloadError.isEmpty()
                            ? QObject::tr("The NVIDIA profile download was empty.")
                            : downloadError);
        }
        // A .nip is XML. Pasting a repository page link instead of the file
        // would download HTML; catch that here rather than feed it to the driver.
        // The leading UTF-8 byte order mark that editors add is stepped over -
        // it is not whitespace, so trimmed() alone would leave "<?xml" unmatched.
        QByteArray head = nip.left(512);
        if (head.startsWith("\xEF\xBB\xBF"))
            head.remove(0, 3);
        head = head.trimmed();
        if (!head.startsWith("<?xml") && !head.contains("<ArrayOfProfile")) {
            cleanUp();
            return fail(QObject::tr("The NVIDIA profile link did not download a .nip profile. "
                                    "Use the link to the file itself."));
        }
        profileFile = work.filePath(QStringLiteral("profile.nip"));
        QFile out(profileFile);
        if (!out.open(QIODevice::WriteOnly) || out.write(nip) != nip.size()) {
            cleanUp();
            return fail(QObject::tr("Could not save the NVIDIA profile download."));
        }
    }

    // --- 2. Profile Inspector: where from, and the hash to check it against --
    QUrl inspectorUrl;
    QString expectedSha;

    const QString pastedInspector = QString(kInspectorLink).trimmed();
    if (!pastedInspector.isEmpty()) {
        inspectorUrl = directGitHubDownload(pastedInspector);
        if (!inspectorUrl.isValid()) {
            cleanUp();
            return fail(QObject::tr("The Profile Inspector link must be an https GitHub link."));
        }
        // A release asset has a SHA-256 published by GitHub; use it when there
        // is one. A plain repository file has no published hash, so it relies
        // on the download coming over HTTPS from GitHub itself.
        QString assetName;
        const QUrl api = releaseApiFor(inspectorUrl, &assetName);
        if (api.isValid()) {
            QString lookupError;
            const QByteArray json = httpGet(api, &lookupError);
            const QJsonArray assets =
                QJsonDocument::fromJson(json).object().value(QStringLiteral("assets")).toArray();
            for (const QJsonValue &value : assets) {
                const QJsonObject asset = value.toObject();
                const QString digest = asset.value(QStringLiteral("digest")).toString();
                if (asset.value(QStringLiteral("name")).toString() == assetName
                    && digest.startsWith(QLatin1String("sha256:"))) {
                    expectedSha = digest.mid(7).toLower();
                    break;
                }
            }
        }
    } else {
        inspectorUrl = QUrl(kInspectorFallbackUrl);
        expectedSha = QString(kInspectorFallbackSha256);

        QString lookupError;
        const QByteArray json = httpGet(QUrl(kInspectorLatestRelease), &lookupError);
        const QJsonArray assets =
            QJsonDocument::fromJson(json).object().value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString digest = asset.value(QStringLiteral("digest")).toString();
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            if (asset.value(QStringLiteral("name")).toString().endsWith(QLatin1String(".zip"),
                                                                         Qt::CaseInsensitive)
                && digest.startsWith(QLatin1String("sha256:")) && isGitHubHost(url.host())
                && url.scheme() == QLatin1String("https")) {
                inspectorUrl = url;
                expectedSha = digest.mid(7).toLower();
                break;
            }
        }
    }

    // A pinned hash always wins: it is the only real protection for a file that
    // is downloaded and then executed with administrator rights.
    if (!QString(kInspectorPinnedSha256).trimmed().isEmpty())
        expectedSha = QString(kInspectorPinnedSha256).trimmed().toLower();

    // --- 3. Download and verify Profile Inspector ------------------------------
    QString downloadError;
    const QByteArray download = httpGet(inspectorUrl, &downloadError);
    if (download.isEmpty()) {
        cleanUp();
        return fail(downloadError.isEmpty()
                        ? QObject::tr("The Profile Inspector download was empty.")
                        : downloadError);
    }
    if (!expectedSha.isEmpty() && sha256Hex(download) != expectedSha) {
        cleanUp();
        return fail(QObject::tr("The Profile Inspector download did not match its expected "
                                "checksum and was not run. If you replaced the file on GitHub "
                                "on purpose, update kInspectorPinnedSha256 in GpuTweaks.cpp."));
    }

    // --- 4. Unpack (a .zip) or use as-is (an .exe) -----------------------------
    QString inspectorExe;
    if (looksLikeExe(download)) {
        inspectorExe = work.filePath(QStringLiteral("nvidiaProfileInspector.exe"));
        QFile out(inspectorExe);
        if (!out.open(QIODevice::WriteOnly) || out.write(download) != download.size()) {
            cleanUp();
            return fail(QObject::tr("Could not save the Profile Inspector download."));
        }
    } else if (looksLikeZip(download)) {
        const QString zipPath = work.filePath(QStringLiteral("nvidiaProfileInspector.zip"));
        {
            QFile out(zipPath);
            if (!out.open(QIODevice::WriteOnly) || out.write(download) != download.size()) {
                cleanUp();
                return fail(QObject::tr("Could not save the Profile Inspector download."));
            }
        }
        const QString extractPath = work.filePath(QStringLiteral("app"));
        QDir().mkpath(extractPath);

        // tar.exe ships with Windows 10 and 11 and reads zip archives natively.
        const QString tar = QDir(QString::fromLocal8Bit(qgetenv("SystemRoot")))
                                .filePath(QStringLiteral("System32/tar.exe"));
        QProcess extract;
        extract.start(tar, {QStringLiteral("-xf"), zipPath, QStringLiteral("-C"), extractPath});
        if (!extract.waitForFinished(kNetworkTimeoutMs) || extract.exitCode() != 0) {
            extract.kill();
            extract.waitForFinished(2000);
            cleanUp();
            return fail(QObject::tr("Could not extract NVIDIA Profile Inspector."));
        }

        // Prefer the Profile Inspector executable; fall back to the only .exe.
        QStringList executables;
        QDirIterator it(extractPath, {QStringLiteral("*.exe")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext())
            executables << it.next();
        for (const QString &candidate : executables) {
            if (QFileInfo(candidate).fileName().contains(QLatin1String("ProfileInspector"),
                                                         Qt::CaseInsensitive)) {
                inspectorExe = candidate;
                break;
            }
        }
        if (inspectorExe.isEmpty() && executables.size() == 1)
            inspectorExe = executables.first();
    } else {
        cleanUp();
        return fail(QObject::tr("The Profile Inspector link did not download a .zip or .exe. "
                                "Use the link to the file itself."));
    }

    if (inspectorExe.isEmpty()) {
        cleanUp();
        return fail(QObject::tr("The Profile Inspector download did not contain the program."));
    }

    // --- 5. Silent import, then make sure it has closed ------------------------
    // Profile Inspector's silent import is "-silentImport <file>": the flag has
    // to come FIRST, with the .nip path after it. In the other order it treats
    // the .nip as a profile to open and shows the full window instead of
    // importing. With the flag first it imports with no window and exits on its
    // own (exit code 0), which is what makes the download-import-delete flow work
    // without anything appearing on screen.
    QProcess inspector;
    inspector.setWorkingDirectory(QFileInfo(inspectorExe).absolutePath());
    inspector.start(inspectorExe,
                    {QStringLiteral("-silentImport"), QDir::toNativeSeparators(profileFile)});

    bool importOk = false;
    QString importError;
    if (!inspector.waitForStarted(15000)) {
        importError = QObject::tr("NVIDIA Profile Inspector could not start: %1")
                          .arg(inspector.errorString());
    } else if (!inspector.waitForFinished(kImportTimeoutMs)) {
        importError = QObject::tr("NVIDIA Profile Inspector did not finish the import in time.");
    } else if (inspector.exitStatus() != QProcess::NormalExit || inspector.exitCode() != 0) {
        importError = QObject::tr("NVIDIA Profile Inspector reported an error (exit code %1).")
                          .arg(inspector.exitCode());
    } else {
        importOk = true;
    }

    if (inspector.state() != QProcess::NotRunning) {
        inspector.kill();
        inspector.waitForFinished(5000);
    }

    // --- 6. Delete everything that was downloaded --------------------------------
    cleanUp();

    if (!importOk)
        return fail(importError);

    setAppSettingsValue(QString(kImportedMarkerKey), profileMarker(source));
    return true;
}

bool NvidiaProfileTweak::revert(TweakBackupStore &store, QString *error)
{
    Q_UNUSED(store)
    Q_UNUSED(error)
    // A profile import overwrites driver profile data that Profile Inspector
    // cannot export back out silently, so there is no exact way back. The
    // confirmation dialog says so before anything runs. Clearing the marker
    // lets the import run again later.
    setAppSettingsValue(QString(kImportedMarkerKey), QString());
    return true;
}
