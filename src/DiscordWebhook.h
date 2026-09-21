// DiscordWebhook.h - Sends application audit logs to a Discord webhook.
//
// When someone signs in, an embed is posted to the owner's Discord: who signed
// in, their motherboard serial / machine id, the licence entitlement, and the
// build / OS. Everything is a rich embed in the brand blue with the NextGen
// Tweaks logo. Fire-and-forget: a missing or unreachable webhook never blocks
// or disrupts the app.
//
// The webhook URL is read (in order) from:
//   1. the NGT_LOG_WEBHOOK environment variable,
//   2. a "webhook.url" text file next to the executable,
//   3. the "logging/webhookUrl" setting,
//   4. the built-in default for packaged clients.
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

class DiscordWebhook : public QObject
{
    Q_OBJECT

public:
    explicit DiscordWebhook(QObject *parent = nullptr);

    /// True when a webhook URL is configured, so callers can skip work.
    static bool isConfigured();

    /// Posts a "signed in" audit embed. Safe to call even if unconfigured.
    void sendLoginLog(const QString &username, const QString &entitlement, bool licensed);

private:
    static QString webhookUrl();

    QNetworkAccessManager *m_network = nullptr;
};
