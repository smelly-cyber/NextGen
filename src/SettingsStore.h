// SettingsStore.h - Persistent user preferences.
//
// Thin, typed wrapper over QSettings so the Settings page never hard-codes key
// strings and every preference has one obvious default.
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

class QSettings;

class SettingsStore : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QObject *parent = nullptr);
    ~SettingsStore() override;

    /// Shared instance.
    static SettingsStore *instance();

    /// Every preference key in one place.
    struct Keys
    {
        static const char *LaunchOnStartup;
        static const char *MinimiseToTray;
        static const char *AutoCheckUpdates;
        static const char *Language;
        static const char *Theme;
        static const char *AccentColour;
        static const char *EnableAnimations;
        static const char *CompactMode;
        static const char *AlertOnComplete;
        static const char *PerformanceWarnings;
        static const char *UpdateNotifications;
    };

    bool boolValue(const char *key) const;
    void setBoolValue(const char *key, bool value);

    QString stringValue(const char *key) const;
    void setStringValue(const char *key, const QString &value);

    /// Restores every preference to its shipped default.
    void resetToDefaults();

signals:
    void changed(const QString &key);
    void reset();

private:
    static QVariant defaultFor(const char *key);

    QSettings *m_settings = nullptr;
};
