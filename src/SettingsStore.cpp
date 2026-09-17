#include "SettingsStore.h"

#include <QSettings>

const char *SettingsStore::Keys::LaunchOnStartup = "general/launchOnStartup";
const char *SettingsStore::Keys::MinimiseToTray = "general/minimiseToTray";
const char *SettingsStore::Keys::AutoCheckUpdates = "general/autoCheckUpdates";
const char *SettingsStore::Keys::Language = "general/language";
const char *SettingsStore::Keys::Theme = "appearance/theme";
const char *SettingsStore::Keys::AccentColour = "appearance/accentColour";
const char *SettingsStore::Keys::EnableAnimations = "appearance/enableAnimations";
const char *SettingsStore::Keys::CompactMode = "appearance/compactMode";
const char *SettingsStore::Keys::AlertOnComplete = "notifications/alertOnComplete";
const char *SettingsStore::Keys::PerformanceWarnings = "notifications/performanceWarnings";
const char *SettingsStore::Keys::UpdateNotifications = "notifications/updateNotifications";

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{
    // Organisation and application names are set in main(), so this lands in
    // the usual per-user location.
    m_settings = new QSettings(this);
}

SettingsStore::~SettingsStore() = default;

SettingsStore *SettingsStore::instance()
{
    static SettingsStore store;
    return &store;
}

QVariant SettingsStore::defaultFor(const char *key)
{
    const QLatin1String name(key);

    if (name == QLatin1String(Keys::Language))
        return QStringLiteral("English");
    if (name == QLatin1String(Keys::Theme))
        return QStringLiteral("Dark");
    if (name == QLatin1String(Keys::AccentColour))
        return QStringLiteral("Blue");
    if (name == QLatin1String(Keys::CompactMode))
        return false;
    // Free-form strings default to empty rather than the switch default (true).
    if (name == QLatin1String("account/displayName"))
        return QString();

    // Every remaining preference is a switch that ships on.
    return true;
}

bool SettingsStore::boolValue(const char *key) const
{
    return m_settings->value(QString::fromLatin1(key), defaultFor(key)).toBool();
}

void SettingsStore::setBoolValue(const char *key, bool value)
{
    const QString name = QString::fromLatin1(key);
    if (m_settings->value(name, defaultFor(key)).toBool() == value)
        return;

    m_settings->setValue(name, value);
    emit changed(name);
}

QString SettingsStore::stringValue(const char *key) const
{
    return m_settings->value(QString::fromLatin1(key), defaultFor(key)).toString();
}

void SettingsStore::setStringValue(const char *key, const QString &value)
{
    const QString name = QString::fromLatin1(key);
    if (m_settings->value(name, defaultFor(key)).toString() == value)
        return;

    m_settings->setValue(name, value);
    emit changed(name);
}

void SettingsStore::resetToDefaults()
{
    m_settings->clear();
    m_settings->sync();
    emit reset();
}
