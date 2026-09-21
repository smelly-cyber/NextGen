#include "DiscordWebhook.h"

#include "HardwareId.h"
#include "LicenseClient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSysInfo>
#include <QUrl>

namespace {
/// Brand blue (0x1E88FF) as the decimal colour Discord embeds expect.
constexpr int kBrandBlue = 0x1E88FF;
const QLatin1String kLogoResource(":/assets/logo/nextgen_tweaks_mark.png");
} // namespace

DiscordWebhook::DiscordWebhook(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString DiscordWebhook::webhookUrl()
{
    // 1. Environment variable.
    const QString fromEnv = qEnvironmentVariable("NGT_LOG_WEBHOOK").trimmed();
    if (!fromEnv.isEmpty())
        return fromEnv;

    // 2. A plain-text file next to the executable.
    QFile file(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("webhook.url")));
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        const QString fromFile = QString::fromUtf8(file.readAll()).trimmed();
        if (!fromFile.isEmpty())
            return fromFile;
    }

    // 3. A stored setting.
    const QString fromSettings =
        QSettings().value(QStringLiteral("logging/webhookUrl")).toString().trimmed();
    if (!fromSettings.isEmpty())
        return fromSettings;

    // 4. Default for packaged clients without a local override.
    return QStringLiteral("https://discord.com/api/webhooks/1550363895913250816/6X3rpqDMpzo8qa-EPxy2F4LTqFT6ldcCPFJDw_Chxctjq_NXBMHKGdO1IQOb0TEKyKAj");
}

bool DiscordWebhook::isConfigured()
{
    return webhookUrl().startsWith(QLatin1String("http"));
}

void DiscordWebhook::sendLoginLog(const QString &username, const QString &entitlement,
                                  bool licensed)
{
    const QString url = webhookUrl();
    if (!url.startsWith(QLatin1String("http")))
        return; // Not configured - nothing to do.

    const QString serial = HardwareId::motherboardSerial();

    const auto field = [](const QString &name, const QString &value, bool inlined) {
        QJsonObject f;
        f.insert(QStringLiteral("name"), name);
        f.insert(QStringLiteral("value"), value.isEmpty() ? QStringLiteral("—") : value);
        f.insert(QStringLiteral("inline"), inlined);
        return f;
    };

    QJsonArray fields;
    fields.append(field(QStringLiteral("User"), username, true));
    fields.append(field(QStringLiteral("Status"),
                        licensed ? QStringLiteral("✅ Licensed") : QStringLiteral("🔒 Unlicensed"),
                        true));
    fields.append(field(QStringLiteral("Entitlement"), entitlement, false));
    fields.append(field(QStringLiteral("Motherboard Serial"),
                        serial.isEmpty() ? QStringLiteral("(hidden by board)") : serial, true));
    fields.append(field(QStringLiteral("Machine ID"), LicenseClient::machineId(), true));
    fields.append(field(QStringLiteral("Device"), QSysInfo::machineHostName(), true));
    fields.append(field(QStringLiteral("OS"), QSysInfo::prettyProductName(), true));
    fields.append(field(QStringLiteral("App Version"),
                        QCoreApplication::applicationVersion(), true));

    QJsonObject embed;
    embed.insert(QStringLiteral("title"), QStringLiteral("Application Login"));
    embed.insert(QStringLiteral("color"), kBrandBlue);
    embed.insert(QStringLiteral("fields"), fields);
    embed.insert(QStringLiteral("thumbnail"),
                 QJsonObject{{QStringLiteral("url"), QStringLiteral("attachment://logo.png")}});
    embed.insert(QStringLiteral("author"),
                 QJsonObject{{QStringLiteral("name"), QStringLiteral("NextGen Tweaks")},
                             {QStringLiteral("icon_url"), QStringLiteral("attachment://logo.png")}});
    embed.insert(QStringLiteral("footer"),
                 QJsonObject{{QStringLiteral("text"), QStringLiteral("NextGen Tweaks · audit log")}});
    embed.insert(QStringLiteral("timestamp"),
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QJsonObject payload;
    payload.insert(QStringLiteral("username"), QStringLiteral("NextGen Tweaks"));
    payload.insert(QStringLiteral("embeds"), QJsonArray{embed});

    auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart jsonPart;
    jsonPart.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    jsonPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"payload_json\""));
    jsonPart.setBody(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    multi->append(jsonPart);

    // Attach the logo so the embed's thumbnail/author icon resolve.
    QFile logo(kLogoResource);
    if (logo.open(QIODevice::ReadOnly)) {
        QHttpPart logoPart;
        logoPart.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("image/png"));
        logoPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QStringLiteral("form-data; name=\"files[0]\"; filename=\"logo.png\""));
        logoPart.setBody(logo.readAll());
        multi->append(logoPart);
    }

    QNetworkRequest request{QUrl(url)};
    request.setTransferTimeout(10000);
    QNetworkReply *reply = m_network->post(request, multi);
    multi->setParent(reply); // Freed with the reply.
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
