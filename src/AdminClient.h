// AdminClient.h - The desktop app's link to the owner-only admin API.
//
// Uses the same session token LicenseClient stored at sign-in; every admin
// endpoint is authorised server-side (the account must be the owner), so this
// class holds no privileged secret of its own - it just carries the token.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;

class AdminClient : public QObject
{
    Q_OBJECT

public:
    explicit AdminClient(QObject *parent = nullptr);

    /// True when a session token is stored (i.e. someone is signed in).
    bool hasSession() const;

    using Callback = std::function<void(const QJsonObject &)>;

    /// POSTs {token, ...body} to \a path and delivers the JSON reply. On network
    /// failure the callback receives { ok:false, reason:"..." }.
    void request(const QString &path, const QJsonObject &body, const Callback &onReply);

    // Convenience wrappers used by the admin page.
    void overview(const Callback &cb) { request(QStringLiteral("/v1/admin/overview"), {}, cb); }
    void licenses(const QString &query, const QString &status, int page, const Callback &cb);
    void createLicense(const QString &kind, int durationDays, int uses, const QString &note,
                       const Callback &cb);
    void revoke(const QStringList &keyIds, const Callback &cb);
    void clearLicenses(const Callback &cb)
    {
        request(QStringLiteral("/v1/admin/clear-licenses"), {}, cb);
    }
    void command(const QString &kind, const QString &target, const QString &message,
                 const Callback &cb);
    void pushUpdate(const QString &version, const QString &notes, const QString &target,
                    const QString &url, const QString &sha256, const Callback &cb);
    void audit(const Callback &cb) { request(QStringLiteral("/v1/admin/audit"), {}, cb); }
    void exportUsers(const Callback &cb) { request(QStringLiteral("/v1/admin/export"), {}, cb); }

private:
    QString token() const;

    QNetworkAccessManager *m_network = nullptr;
    QString m_serverUrl;
};
