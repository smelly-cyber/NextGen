// Theme.h - Centralised design tokens for NEXTGEN TWEAKS.
//
// Every colour, radius, spacing value and animation duration used by the UI is
// declared here so the whole application can be re-skinned from a single place.
#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace Theme {

/// All colours used by the application.
struct Palette
{
    // Window / shell
    QColor windowTop;           ///< Top of the main window gradient.
    QColor windowBottom;        ///< Bottom of the main window gradient.
    QColor windowBorder;        ///< Thin blue outline around the window.
    QColor windowGlow;          ///< Outer glow bleeding around the window.

    // Panels
    QColor brandPanelTop;       ///< Left branding panel gradient (top).
    QColor brandPanelBottom;    ///< Left branding panel gradient (bottom).
    QColor formPanelTop;        ///< Right login panel gradient (top).
    QColor formPanelBottom;     ///< Right login panel gradient (bottom).
    QColor panelDivider;        ///< Vertical hairline between the two panels.
    QColor brandShape;          ///< Abstract diagonal shapes on the branding panel.

    // Accents
    QColor primary;             ///< Main brand blue.
    QColor primaryBright;       ///< Lighter end of the primary gradient.
    QColor primaryDeep;         ///< Darker end of the primary gradient.
    QColor cyan;                ///< Cyan accent lighting.
    QColor link;                ///< Link / highlighted word colour.

    // Text
    QColor textPrimary;         ///< Headings and field text.
    QColor textSecondary;       ///< Sub-headings and supporting copy.
    QColor textMuted;           ///< Placeholder / low emphasis copy.
    QColor textOnPrimary;       ///< Text drawn on the primary gradient.

    // Controls
    QColor fieldBackground;     ///< Input field fill.
    QColor fieldBackgroundHover;
    QColor fieldBorder;         ///< Idle input border.
    QColor fieldBorderHover;
    QColor fieldBorderFocus;    ///< Focused input border.
    QColor fieldIcon;           ///< Leading icon inside an input.
    QColor fieldIconActive;

    QColor controlHover;        ///< Generic hover wash (title bar buttons ...).
    QColor controlPressed;
    QColor closeHover;          ///< Close button hover colour.

    QColor outlineButtonBorder;
    QColor outlineButtonHoverBg;

    // Main menu
    QColor cardTop;             ///< Menu card fill (top of gradient).
    QColor cardBottom;          ///< Menu card fill (bottom of gradient).
    QColor cardHoverTop;
    QColor cardHoverBottom;
    QColor cardBorder;          ///< Idle menu card border.
    QColor cardBorderSelected;  ///< Selected / hovered menu card border.
    QColor iconTileTop;         ///< Rounded icon tile inside a card.
    QColor iconTileBottom;
    QColor iconTileBorder;
    QColor chevron;             ///< Trailing chevron on a menu card.
    QColor statusBarTop;        ///< Bottom status strip.
    QColor statusBarBottom;
    QColor online;              ///< Presence dot on the avatar.

    // Section pages
    QColor toggleTrackOff;      ///< Switch track when off.
    QColor toggleTrackOn;       ///< Switch track when on.
    QColor toggleKnob;          ///< Switch knob.
    QColor badgeText;           ///< "Recommended" label.
    QColor badgeOutline;        ///< "Optional" pill outline.
    QColor gaugeTrack;          ///< Unfilled part of a donut gauge.
    QColor chartGrid;           ///< Chart gridlines and axes.
    QColor chartBaseline;       ///< "Before" series.
    QColor chartProjection;     ///< "After (estimated)" series.

    QColor divider;             ///< "OR" divider lines.
    QColor danger;              ///< Validation / error state.
    QColor success;

    QColor selectionBackground;
    QColor focusRing;           ///< Keyboard focus indicator.
};

/// Geometry tokens (radii, sizes, spacing).
struct Metrics
{
    int windowRadius;           ///< Corner radius of the frameless window.
    int glowMargin;             ///< Space reserved around the window for the glow.
    int titleBarHeight;
    int windowButtonSize;
    int windowButtonRadius;

    int controlRadius;          ///< Radius for inputs and buttons.
    int fieldHeight;
    int buttonHeight;
    int iconSize;               ///< Standard glyph size inside controls.
    int checkBoxSize;

    int formMaxWidth;           ///< The login form never grows wider than this.
    int panelPadding;

    // Main menu
    int cardHeight;             ///< Height of one menu card.
    int cardRadius;
    int cardSpacing;
    int iconTileSize;           ///< Rounded square holding a card's glyph.
    int iconTileRadius;
    int cardIconSize;           ///< Glyph drawn inside the icon tile.
    int chevronSize;
    int avatarSize;
    int statusBarHeight;
    int menuMaxWidth;           ///< The card column never grows wider than this.

    // Section pages
    int railCompactWidth;       ///< Branding rail width on a section page.
    int toggleWidth;
    int toggleHeight;
    int optionRowHeight;
    int metricCardHeight;
    int sectionPadding;         ///< Padding inside a section page.

    int windowMinWidth;
    int windowMinHeight;
    int windowDefaultWidth;
    int windowDefaultHeight;

    int resizeBorder;           ///< Hit-test thickness for edge resizing.
};

/// Animation timings in milliseconds.
struct Durations
{
    int hover;
    int focus;
    int press;
    int state;
    int spinner;
};

/// Which base colour scheme to build from.
enum class Variant {
    Dark,     ///< The default deep-navy scheme.
    Midnight  ///< A darker, near-black variant.
};

/// Runtime appearance configuration. Driven by the Settings page through
/// ThemeManager; changing it rebuilds the palette / metrics / durations.
struct Config
{
    Variant variant = Variant::Dark;
    QColor accent = QColor(0x1E, 0x88, 0xFF); ///< Brand accent (default blue).
    bool animations = true;                   ///< When false, durations are zero.
    bool compact = false;                     ///< Denser metrics (applied on load).
};

const Palette   &colors();
const Metrics   &metrics();
const Durations &durations();

/// Current appearance configuration.
const Config &config();
/// Rebuilds the cached palette / metrics / durations from \a cfg. Widgets must
/// be told to repaint afterwards (ThemeManager does this).
void setConfig(const Config &cfg);

/// Builds a Segoe UI based font with a sensible fallback chain.
QFont font(int pixelSize, QFont::Weight weight = QFont::Normal, qreal letterSpacing = 0.0);

/// Linear interpolation between two colours (t is clamped to 0..1).
QColor mix(const QColor &from, const QColor &to, qreal t);

/// Returns a copy of \a color with the given alpha (0..255).
QColor alpha(const QColor &color, int a);

/// Application wide style sheet (only for things QPainter should not own).
QString globalStyleSheet();

} // namespace Theme
