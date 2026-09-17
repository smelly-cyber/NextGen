// ThemeManager.h - Bridges saved preferences to the live Theme.
//
// Watches SettingsStore and, whenever an appearance preference changes, rebuilds
// the Theme palette / metrics / durations and repaints every open widget. Accent
// colour, the Dark/Midnight variant and the animation toggle all take effect
// immediately; compact mode and language are read at construction, so those are
// reported via needsRestart() for the Settings page to prompt a relaunch.
#pragma once

#include "Theme.h"

#include <QColor>
#include <QObject>
#include <QString>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager *instance();

    /// Reads every appearance preference and applies it to the Theme without
    /// repainting. Call once from main() before the first window is built.
    void applyFromSettings();

    /// Maps a stored accent to its colour: a "#RRGGBB" picked with the colour
    /// picker, or one of the original names ("Blue", "Green" ...) from older
    /// settings.
    static QColor accentColour(const QString &name);

signals:
    /// Emitted after a live re-theme so interested widgets can refresh any
    /// colour they cached in a style sheet.
    void themeChanged();

    /// Emitted when a preference that only applies on restart changed (compact
    /// mode or language), so the UI can offer to relaunch.
    void restartRequested(const QString &reason);

private:
    explicit ThemeManager(QObject *parent = nullptr);

    Theme::Config configFromSettings() const;
    void reapply(const QString &changedKey);
    static void repaintAllWidgets();

    bool m_compactAtLoad = false;
};
