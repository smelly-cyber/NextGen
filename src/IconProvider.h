// IconProvider.h - Renders the bundled SVG icons in any colour and size.
//
// The SVG assets in :/assets/icons are authored with #FFFFFF as their stroke /
// fill colour. IconProvider swaps that token for the requested colour before
// handing the markup to QSvgRenderer, which keeps every glyph crisp at any DPI
// while still allowing the theme to recolour icons on hover / focus.
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace IconProvider {

/// Logical icon names (they map 1:1 onto the files in assets/icons).
namespace Name {
inline const char *User      = "user";
inline const char *Lock      = "lock";
inline const char *Eye       = "eye";
inline const char *EyeOff    = "eye_off";
inline const char *Login     = "login";
inline const char *UserPlus  = "user_plus";
inline const char *Minimize  = "minimize";
inline const char *Maximize  = "maximize";
inline const char *Restore   = "restore";
inline const char *Close     = "close";
inline const char *Check     = "check";
} // namespace Name

/// Returns \a name rendered at \a size in \a color for a screen with the given
/// device pixel ratio. Results are cached.
QPixmap pixmap(const QString &name, const QSize &size, const QColor &color, qreal devicePixelRatio = 1.0);

/// Convenience overload for square icons.
QPixmap pixmap(const QString &name, int size, const QColor &color, qreal devicePixelRatio = 1.0);

/// Wraps pixmap() in a QIcon (useful for QAction / window icons).
QIcon icon(const QString &name, int size, const QColor &color);

/// Drops every cached pixmap (call if the palette changes at runtime).
void clearCache();

} // namespace IconProvider
