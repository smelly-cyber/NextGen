#include "Theme.h"

#include "WindowStyle.h"

#include <QtGlobal>

namespace Theme {

namespace {

Palette makePalette()
{
    Palette p;

    p.windowTop            = QColor(0x10, 0x17, 0x24);
    p.windowBottom         = QColor(0x06, 0x09, 0x11);
    p.windowBorder         = QColor(0x1F, 0x6F, 0xEB);
    p.windowGlow           = QColor(0x1E, 0x7B, 0xFF);

    p.brandPanelTop        = QColor(0x0D, 0x14, 0x20);
    p.brandPanelBottom     = QColor(0x07, 0x0B, 0x13);
    p.formPanelTop         = QColor(0x0A, 0x10, 0x1A);
    p.formPanelBottom      = QColor(0x05, 0x08, 0x0F);
    p.panelDivider         = QColor(0x1B, 0x3A, 0x66);
    p.brandShape           = QColor(0x2C, 0x7B, 0xE0);

    p.primary              = QColor(0x1E, 0x88, 0xFF);
    p.primaryBright        = QColor(0x46, 0xA8, 0xFF);
    p.primaryDeep          = QColor(0x0B, 0x5E, 0xD6);
    p.cyan                 = QColor(0x2E, 0xD3, 0xF0);
    p.link                 = QColor(0x3D, 0x9B, 0xFF);

    p.textPrimary          = QColor(0xF3, 0xF7, 0xFC);
    p.textSecondary        = QColor(0x93, 0xA6, 0xC0);
    p.textMuted            = QColor(0x6F, 0x83, 0xA1);
    p.textOnPrimary        = QColor(0xFF, 0xFF, 0xFF);

    p.fieldBackground      = QColor(0x0B, 0x12, 0x1E);
    p.fieldBackgroundHover = QColor(0x0F, 0x18, 0x28);
    p.fieldBorder          = QColor(0x1C, 0x2C, 0x45);
    p.fieldBorderHover     = QColor(0x27, 0x44, 0x6D);
    p.fieldBorderFocus     = QColor(0x2E, 0x8B, 0xFF);
    p.fieldIcon            = QColor(0x5E, 0x74, 0x95);
    p.fieldIconActive      = QColor(0x7F, 0xB6, 0xFF);

    p.controlHover         = QColor(0xFF, 0xFF, 0xFF, 26);
    p.controlPressed       = QColor(0xFF, 0xFF, 0xFF, 46);
    p.closeHover           = QColor(0xE0, 0x35, 0x4B);

    p.outlineButtonBorder  = QColor(0x24, 0x49, 0x7A);
    p.outlineButtonHoverBg = QColor(0x1E, 0x88, 0xFF, 38);

    p.cardTop              = QColor(0x0E, 0x16, 0x25);
    p.cardBottom           = QColor(0x09, 0x0F, 0x1B);
    p.cardHoverTop         = QColor(0x14, 0x20, 0x35);
    p.cardHoverBottom      = QColor(0x0C, 0x15, 0x25);
    p.cardBorder           = QColor(0x1B, 0x2E, 0x4C);
    p.cardBorderSelected   = QColor(0x2E, 0x8B, 0xFF);
    p.iconTileTop          = QColor(0x16, 0x24, 0x3B);
    p.iconTileBottom       = QColor(0x0D, 0x17, 0x28);
    p.iconTileBorder       = QColor(0x24, 0x3C, 0x62);
    p.chevron              = QColor(0x5A, 0x7A, 0xA8);
    p.statusBarTop         = QColor(0x0A, 0x11, 0x1D);
    p.statusBarBottom      = QColor(0x06, 0x0B, 0x14);
    p.online               = QColor(0x35, 0xD0, 0x8A);

    p.toggleTrackOff       = QColor(0x25, 0x33, 0x4A);
    p.toggleTrackOn        = QColor(0x1E, 0x88, 0xFF);
    p.toggleKnob           = QColor(0xF5, 0xF9, 0xFF);
    p.badgeText            = QColor(0x6E, 0xB0, 0xFF);
    p.badgeOutline         = QColor(0x2A, 0x43, 0x69);
    p.gaugeTrack           = QColor(0x1A, 0x28, 0x3F);
    p.chartGrid            = QColor(0x1C, 0x2B, 0x43);
    p.chartBaseline        = QColor(0x7B, 0x8A, 0xA3);
    p.chartProjection      = QColor(0x36, 0x9C, 0xFF);

    p.divider              = QColor(0x1A, 0x2B, 0x47);
    p.danger               = QColor(0xFF, 0x53, 0x66);
    p.success              = QColor(0x35, 0xD0, 0x8A);

    p.selectionBackground  = QColor(0x1E, 0x88, 0xFF, 110);
    p.focusRing            = QColor(0x46, 0xA8, 0xFF, 150);

    return p;
}

/// Recolours one accent-family swatch to a new hue while preserving its own
/// saturation and lightness, so the whole blue chrome tracks the chosen accent
/// without disturbing the tuned depth relationships. Near-grey inputs are left
/// alone. Alpha is preserved.
QColor toAccentHue(const QColor &original, int hue, int saturation)
{
    int h = 0, s = 0, l = 0, a = 0;
    original.getHsl(&h, &s, &l, &a);
    QColor result;
    // Blend the swatch's own saturation towards the accent's so pale structural
    // blues stay subtle while vivid ones stay vivid.
    const int mixedSat = qBound(0, (s * 2 + saturation) / 3, 255);
    result.setHsl(hue, mixedSat, l, a);
    return result;
}

/// Remaps every accent-family colour in \a p to the hue of \a accent. Status
/// colours (danger/success), text and neutral greys are deliberately untouched.
void applyAccent(Palette &p, const QColor &accent)
{
    int h = 0, s = 0, l = 0, a = 0;
    accent.getHsl(&h, &s, &l, &a);
    if (h < 0)
        h = 212; // Achromatic accent: fall back to the brand blue hue.

    const auto shift = [&](QColor &c) { c = toAccentHue(c, h, s); };

    shift(p.windowBorder);
    shift(p.windowGlow);
    shift(p.brandShape);
    shift(p.panelDivider);

    p.primary       = accent;
    shift(p.primaryBright);
    shift(p.primaryDeep);
    shift(p.link);

    shift(p.fieldBorderHover);
    shift(p.fieldBorderFocus);
    shift(p.fieldIconActive);
    shift(p.outlineButtonBorder);
    shift(p.outlineButtonHoverBg);

    shift(p.cardBorderSelected);
    shift(p.iconTileBorder);

    shift(p.toggleTrackOn);
    shift(p.badgeText);
    shift(p.chartProjection);
    shift(p.selectionBackground);
    shift(p.focusRing);
}

/// Deepens the structural backgrounds for the Midnight variant, leaving accents
/// and text as they are.
void applyMidnight(Palette &p)
{
    const auto deepen = [](QColor &c, qreal factor) {
        int h = 0, s = 0, l = 0, a = 0;
        c.getHsl(&h, &s, &l, &a);
        c.setHsl(h, s, int(l * factor), a);
    };

    for (QColor *c : {&p.windowTop, &p.windowBottom, &p.brandPanelTop, &p.brandPanelBottom,
                      &p.formPanelTop, &p.formPanelBottom, &p.cardTop, &p.cardBottom,
                      &p.cardHoverTop, &p.cardHoverBottom, &p.iconTileTop, &p.iconTileBottom,
                      &p.statusBarTop, &p.statusBarBottom, &p.fieldBackground,
                      &p.fieldBackgroundHover})
        deepen(*c, 0.55);
}

Metrics makeMetrics(bool compact)
{
    Metrics m;

    // The window's outer shape comes from one place only - WindowStyle.h - so
    // the radius never drifts between the container, the clip and the outline.
    m.windowRadius        = WINDOW_CORNER_RADIUS;
    m.glowMargin          = WINDOW_GLOW_MARGIN;
    m.titleBarHeight      = 46;
    m.windowButtonSize    = 34;
    m.windowButtonRadius  = 8;

    m.controlRadius       = 12;
    m.fieldHeight         = 58;
    m.buttonHeight        = 58;
    m.iconSize            = 20;
    m.checkBoxSize        = 22;

    m.formMaxWidth        = 520;
    m.panelPadding        = compact ? 34 : 48;

    m.cardHeight          = compact ? 72 : 86;
    m.cardRadius          = 12;
    m.cardSpacing         = compact ? 8 : 12;
    m.iconTileSize        = compact ? 46 : 56;
    m.iconTileRadius      = 10;
    m.cardIconSize        = compact ? 24 : 30;
    m.chevronSize         = 18;
    m.avatarSize          = 64;
    m.statusBarHeight     = 58;
    m.menuMaxWidth        = 700;

    m.railCompactWidth    = 232;
    m.toggleWidth         = 46;
    m.toggleHeight        = 26;
    m.optionRowHeight     = compact ? 42 : 50;
    m.metricCardHeight    = compact ? 152 : 176;
    m.sectionPadding      = compact ? 16 : 26;

    m.windowMinWidth      = 960;
    m.windowMinHeight     = 620;
    m.windowDefaultWidth  = 1280;
    m.windowDefaultHeight = 750;

    m.resizeBorder        = 8;

    return m;
}

Durations makeDurations(bool animations)
{
    Durations d;
    if (!animations) {
        // Zero everywhere makes every animated control settle instantly.
        d.hover = d.focus = d.press = d.state = 0;
        d.spinner = 900; // The spinner must still rotate or it looks frozen.
        return d;
    }
    d.hover   = 140;
    d.focus   = 180;
    d.press   = 90;
    d.state   = 220;
    d.spinner = 900;
    return d;
}

Palette buildPalette(const Config &cfg)
{
    Palette p = makePalette();
    if (cfg.variant == Variant::Midnight)
        applyMidnight(p);
    // Only remap when the accent actually differs from the default blue, so the
    // shipped look is byte-for-byte identical out of the box.
    if (cfg.accent.isValid() && cfg.accent != QColor(0x1E, 0x88, 0xFF))
        applyAccent(p, cfg.accent);
    return p;
}

Config     g_config;
Palette    g_palette   = buildPalette(g_config);
Metrics    g_metrics   = makeMetrics(g_config.compact);
Durations  g_durations = makeDurations(g_config.animations);

} // namespace

const Palette &colors()
{
    return g_palette;
}

const Metrics &metrics()
{
    return g_metrics;
}

const Durations &durations()
{
    return g_durations;
}

const Config &config()
{
    return g_config;
}

void setConfig(const Config &cfg)
{
    g_config    = cfg;
    g_palette   = buildPalette(cfg);
    g_metrics   = makeMetrics(cfg.compact);
    g_durations = makeDurations(cfg.animations);
}

QFont font(int pixelSize, QFont::Weight weight, qreal letterSpacing)
{
    QFont f;
    f.setFamilies({QStringLiteral("Segoe UI"),
                   QStringLiteral("Segoe UI Variable Text"),
                   QStringLiteral("Inter"),
                   QStringLiteral("Noto Sans"),
                   QStringLiteral("DejaVu Sans")});
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    f.setHintingPreference(QFont::PreferNoHinting);
    if (!qFuzzyIsNull(letterSpacing))
        f.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    return f;
}

QColor mix(const QColor &from, const QColor &to, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(from.redF()   + (to.redF()   - from.redF())   * t,
                            from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF()  + (to.blueF()  - from.blueF())  * t,
                            from.alphaF() + (to.alphaF() - from.alphaF()) * t);
}

QColor alpha(const QColor &color, int a)
{
    QColor c = color;
    c.setAlpha(qBound(0, a, 255));
    return c;
}

QString globalStyleSheet()
{
    const Palette &c = colors();

    // Only the bits that are genuinely easier to express in QSS live here;
    // every custom widget paints itself with QPainter.
    return QStringLiteral("QToolTip {"
                          "  background-color: %1;"
                          "  color: %2;"
                          "  border: 1px solid %3;"
                          "  border-radius: 6px;"
                          "  padding: 6px 10px;"
                          "}")
        .arg(c.fieldBackground.name(), c.textPrimary.name(), c.fieldBorder.name());
}

} // namespace Theme
