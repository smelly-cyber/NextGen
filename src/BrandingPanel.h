// BrandingPanel.h - Left hand side of the login window.
//
// Shows the official NEXTGEN TWEAKS logo (loaded through the Qt resource
// system), the product pillars and the strap line, on top of a very subtle
// geometric background.
#pragma once

#include "NavList.h" // NavList + MenuSection

#include <QByteArray>
#include <QPixmap>
#include <QSize>
#include <QWidget>

class QLabel;
class SocialLinksBar;

/// Draws the supplied logo asset centred, always aspect-ratio correct and
/// always at the sharpest size the source can deliver.
///
/// The view resolves the best available asset by itself:
///   1. nextgen_tweaks_logo.svg      - vector, pin sharp at any size
///   2. nextgen_tweaks_logo@3x.png   - 3x raster
///   3. nextgen_tweaks_logo@2x.png   - 2x raster
///   4. nextgen_tweaks_logo.png      - base raster
///
/// A raster source is never scaled above 1:1 in device pixels, so it can look
/// small but it can never look pixelated.
class LogoView : public QWidget
{
public:
    explicit LogoView(QWidget *parent = nullptr);

    /// Loads the best logo asset found in :/assets/logo. Returns false when
    /// none of the candidates exist.
    bool loadBestAsset();

    bool hasLogo() const { return !m_source.isNull() || !m_svg.isEmpty(); }

    /// Caps how tall the mark is drawn (the rail uses a smaller value).
    void setMaxHeight(int height);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// The mark is width-constrained in a narrow rail, so its height depends on
    /// the width it is given. Without this the view reserves the height it would
    /// need at full size and starves everything below it in the rail.
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// Natural size of the artwork in logical pixels (its sharpness ceiling).
    QSize nativeLogicalSize() const;
    /// Size the artwork is actually drawn at, given the space granted.
    QSize artworkSize() const;

    QPixmap m_source;      ///< Raster artwork (already alpha keyed).
    qreal m_sourceScale = 1.0; ///< Device pixels per logical pixel in m_source.
    QByteArray m_svg;      ///< Vector artwork markup, when available.
    QSize m_svgSize;       ///< Intrinsic size of the vector artwork.
    int m_maxHeight = 300; ///< Ceiling for the rendered height.
};

/// Thin vertical gradient bar used next to the strap line.
class AccentLine : public QWidget
{
public:
    explicit AccentLine(QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

class BrandingPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BrandingPanel(QWidget *parent = nullptr);

    /// Narrow rail layout used by the section pages: smaller logo, tighter
    /// padding and no strap line.
    void setCompact(bool compact);
    bool isCompact() const { return m_compact; }

    /// Highlights one of the three pillars (0 = Optimise ... 2 = Perform);
    /// pass -1 for none.
    void setActivePillar(int index);

    /// Shows the sidebar navigation. Only the workspace turns this on; the
    /// sign-in window keeps a plain branding panel.
    void setNavigationVisible(bool visible);

    /// Mirrors the current destination into the rail.
    void setNavigationSection(MenuSection section);
    void setNavigationHome();
    /// Locks the section rows behind a licence.
    void setNavigationLocked(bool locked);
    void setAdminVisible(bool visible);

signals:
    void homeRequested();
    void sectionRequested(MenuSection section);
    /// A locked rail row was clicked.
    void activateRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildUi();

    void updatePillars();

    LogoView *m_logo = nullptr;
    QLabel *m_pillars = nullptr;
    NavList *m_nav = nullptr;
    SocialLinksBar *m_social = nullptr;
    class QVBoxLayout *m_layout = nullptr;
    bool m_compact = false;
    int m_activePillar = -1;
};
