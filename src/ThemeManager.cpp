#include "ThemeManager.h"

#include "SettingsStore.h"

#include <QApplication>
#include <QList>
#include <QWidget>

ThemeManager *ThemeManager::instance()
{
    static ThemeManager manager;
    return &manager;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    SettingsStore *store = SettingsStore::instance();
    connect(store, &SettingsStore::changed, this, &ThemeManager::reapply);
    connect(store, &SettingsStore::reset, this, [this] { reapply(QString()); });
}

QColor ThemeManager::accentColour(const QString &name)
{
    // Any colour picked in Settings is stored as "#RRGGBB".
    if (name.startsWith(QLatin1Char('#'))) {
        const QColor custom(name);
        if (custom.isValid())
            return custom;
    }

    // Must match the choices offered on the Settings page.
    if (name == QLatin1String("Cyan"))
        return QColor(0x2E, 0xD3, 0xF0);
    if (name == QLatin1String("Violet"))
        return QColor(0xA8, 0x6C, 0xF0);
    if (name == QLatin1String("Green"))
        return QColor(0x35, 0xD0, 0x8A);
    return QColor(0x1E, 0x88, 0xFF); // Blue (default).
}

Theme::Config ThemeManager::configFromSettings() const
{
    SettingsStore *store = SettingsStore::instance();

    Theme::Config cfg;
    cfg.accent = accentColour(store->stringValue(SettingsStore::Keys::AccentColour));
    cfg.variant = store->stringValue(SettingsStore::Keys::Theme) == QLatin1String("Midnight")
                      ? Theme::Variant::Midnight
                      : Theme::Variant::Dark;
    cfg.animations = store->boolValue(SettingsStore::Keys::EnableAnimations);
    cfg.compact = store->boolValue(SettingsStore::Keys::CompactMode);
    return cfg;
}

void ThemeManager::applyFromSettings()
{
    Theme::setConfig(configFromSettings());
    m_compactAtLoad = SettingsStore::instance()->boolValue(SettingsStore::Keys::CompactMode);
}

void ThemeManager::reapply(const QString &changedKey)
{
    Theme::setConfig(configFromSettings());

    // Re-apply the global style sheet so QSS-owned chrome (tooltips) recolours.
    if (qApp)
        qApp->setStyleSheet(Theme::globalStyleSheet());

    repaintAllWidgets();
    emit themeChanged();

    // Compact mode is baked into the layout when widgets are built, so it needs
    // a relaunch. Only prompt when it actually changed. (Language has no visible
    // effect yet, so changing it never asks for a restart.)
    const bool compactNow = SettingsStore::instance()->boolValue(SettingsStore::Keys::CompactMode);
    const bool compactChanged =
        changedKey == QLatin1String(SettingsStore::Keys::CompactMode) && compactNow != m_compactAtLoad;

    if (compactChanged)
        emit restartRequested(tr("Compact mode"));
}

void ThemeManager::repaintAllWidgets()
{
    // Every custom widget paints from Theme::colors() on each paintEvent, so a
    // forced repaint of the whole tree is all a live re-theme needs.
    const QList<QWidget *> tops = QApplication::topLevelWidgets();
    for (QWidget *top : tops) {
        top->update();
        const QList<QWidget *> children = top->findChildren<QWidget *>();
        for (QWidget *child : children)
            child->update();
    }
}
