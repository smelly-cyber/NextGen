#include "BrandingPanel.h"

#include "NavList.h"
#include "SocialLinksBar.h"
#include "Theme.h"

#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include <utility>
#include <vector>

namespace {

/// Breathing room kept around the artwork so its halo can fade out completely.
/// Kept tight so the mark fills the rail the way the design calls for - every
/// pixel of padding here is a pixel the logo cannot use.
constexpr int kHaloPadding = 18;
/// Default ceiling for the rendered mark height (LogoView::setMaxHeight tunes it).
constexpr int kMaxLogoHeight = 300;
constexpr int kMinLogoHeight = 72;

/// How far a *raster* logo may be stretched beyond its own pixels.
///
/// 1.0 means one source pixel is never spread across more than one device
/// pixel, which is the only way to guarantee the mark can never look soft or
/// pixelated. The supplied nextgen_tweaks_logo.png is 128x128, so at 1.0 it
/// renders 128pt tall. Drop a larger PNG (512px+) or an SVG into assets/logo
/// and it will automatically render bigger AND stay sharp - raising this value
/// instead buys size at the cost of sharpness. Vector assets ignore it.
constexpr qreal kMaxRasterUpscale = 1.0;

/// Pixels darker than this are treated as the artwork's flat backdrop.
constexpr int kKeyFloor = 26;
/// Pixels brighter than this always stay fully opaque.
constexpr int kKeyCeiling = 64;

/// Candidate assets, best first. The scale is the number of device pixels the
/// asset provides per logical pixel of layout.
struct LogoCandidate
{
    const char *path;
    qreal scale;
};

const LogoCandidate kRasterCandidates[] = {
    {":/assets/logo/nextgen_tweaks_logo@3x.png", 3.0},
    {":/assets/logo/nextgen_tweaks_logo@2x.png", 2.0},
    {":/assets/logo/nextgen_tweaks_logo.png", 1.0},
};

const char *kVectorCandidate = ":/assets/logo/nextgen_tweaks_logo.svg";

/// Drops the flat backdrop of a logo that was exported without transparency so
/// the mark sits on the panel gradient instead of inside a black box. Images
/// that already carry an alpha channel are returned untouched.
QPixmap keyOutFlatBackdrop(const QPixmap &source)
{
    if (source.isNull() || source.hasAlphaChannel())
        return source;

    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            const int level = qMax(qRed(pixel), qMax(qGreen(pixel), qBlue(pixel)));
            if (level <= kKeyFloor) {
                line[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), 0);
            } else if (level < kKeyCeiling) {
                const int a = 255 * (level - kKeyFloor) / (kKeyCeiling - kKeyFloor);
                line[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), a);
            }
        }
    }
    return QPixmap::fromImage(image);
}

/// Removes a small strapline band from the bottom of the artwork.
///
/// The supplied logo bakes an "Optimise - Enhance - Perform" line underneath the
/// wordmark. It is part of the PNG, so it cannot simply be "not drawn" - it has
/// to be cropped away.
///
/// The rule is deliberately conservative: only a FINAL band of ink that is both
/// much shorter than the tallest band AND clearly separated from it by blank
/// rows is dropped. A logo that carries no strapline is therefore never damaged.
QPixmap trimTrailingStrapline(const QPixmap &source)
{
    if (source.isNull() || !source.hasAlphaChannel())
        return source;

    const QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    constexpr int kAlphaFloor = 8;

    // Build the row "ink profile": contiguous runs of rows containing artwork.
    std::vector<std::pair<int, int>> bands;
    int start = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        bool ink = false;
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) > kAlphaFloor) {
                ink = true;
                break;
            }
        }
        if (ink && start < 0)
            start = y;
        else if (!ink && start >= 0) {
            bands.emplace_back(start, y - 1);
            start = -1;
        }
    }
    if (start >= 0)
        bands.emplace_back(start, image.height() - 1);

    if (bands.size() < 2)
        return source;

    int tallest = 0;
    for (const auto &band : bands)
        tallest = qMax(tallest, band.second - band.first + 1);

    const auto &last = bands.back();
    const auto &previous = bands.at(bands.size() - 2);
    const int lastHeight = last.second - last.first + 1;
    const int gap = last.first - previous.second - 1;

    const bool muchShorter = lastHeight * 100 <= tallest * 22;
    const bool clearlySeparated = gap * 10 >= lastHeight * 4;
    if (!muchShorter || !clearlySeparated)
        return source;

    return source.copy(QRect(0, 0, source.width(), previous.second + 1));
}

/// Trims fully transparent margins from the artwork.
///
/// The supplied logo is a square export with a lot of empty space around the
/// mark. Left uncropped, the widget reserves that padding as layout height, so
/// the logo both renders smaller than it could AND squeezes everything below it
/// in the rail. Cropping to the ink gives the mark its real aspect ratio.
QPixmap cropToArtwork(const QPixmap &source)
{
    if (source.isNull() || !source.hasAlphaChannel())
        return source;

    const QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    int left = image.width();
    int right = -1;
    int top = image.height();
    int bottom = -1;

    constexpr int kAlphaFloor = 8; // Ignore near-invisible stray pixels.
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) <= kAlphaFloor)
                continue;
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = qMax(bottom, y);
        }
    }

    if (right < left || bottom < top)
        return source; // Fully transparent - leave it alone.

    return source.copy(QRect(QPoint(left, top), QPoint(right, bottom)));
}

} // namespace

// ---------------------------------------------------------------- LogoView --

LogoView::LogoView(QWidget *parent)
    : QWidget(parent)
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void LogoView::setMaxHeight(int height)
{
    m_maxHeight = qMax(48, height);
    updateGeometry();
    update();
}

bool LogoView::loadBestAsset()
{
    // 1. Vector artwork wins outright - it is sharp at every size and DPI.
    QFile vector{QString::fromLatin1(kVectorCandidate)};
    if (vector.exists() && vector.open(QIODevice::ReadOnly)) {
        const QByteArray markup = vector.readAll();
        QSvgRenderer probe(markup);
        if (probe.isValid() && !probe.defaultSize().isEmpty()) {
            m_svg = markup;
            m_svgSize = probe.defaultSize();
            m_source = QPixmap();
            updateGeometry();
            update();
            return true;
        }
    }

    // 2. Otherwise take the highest resolution raster that is present.
    for (const LogoCandidate &candidate : kRasterCandidates) {
        QPixmap pixmap(QLatin1String(candidate.path));
        if (pixmap.isNull())
            continue;

        m_source = cropToArtwork(trimTrailingStrapline(keyOutFlatBackdrop(pixmap)));
        m_sourceScale = candidate.scale;
        m_svg.clear();
        updateGeometry();
        update();
        return true;
    }

    return false;
}

QSize LogoView::nativeLogicalSize() const
{
    if (!m_svg.isEmpty()) {
        // Vector art has no sharpness ceiling; use the design cap directly.
        return m_svgSize.scaled(QSize(m_maxHeight * 6, m_maxHeight), Qt::KeepAspectRatio);
    }

    if (m_source.isNull())
        return QSize();

    // One source pixel must never be stretched across more than
    // kMaxRasterUpscale device pixels - that is what keeps the mark crisp.
    const qreal deviceScale = qMax(0.5, devicePixelRatioF());
    const qreal logicalScale = (m_sourceScale * kMaxRasterUpscale) / deviceScale;

    QSize logical(qRound(m_source.width() * logicalScale),
                  qRound(m_source.height() * logicalScale));
    if (logical.height() > m_maxHeight)
        logical = logical.scaled(QSize(m_maxHeight * 6, m_maxHeight), Qt::KeepAspectRatio);

    return logical;
}

QSize LogoView::artworkSize() const
{
    const QSize natural = nativeLogicalSize();
    if (natural.isEmpty())
        return QSize();

    // Shrink (never grow) to fit the space the layout actually granted, minus
    // the padding the halo needs.
    const QSize available(qMax(1, width() - 2 * kHaloPadding),
                          qMax(1, height() - 2 * kHaloPadding));
    if (natural.width() <= available.width() && natural.height() <= available.height())
        return natural;

    return natural.scaled(available, Qt::KeepAspectRatio);
}

QSize LogoView::sizeHint() const
{
    const QSize natural = nativeLogicalSize();
    if (natural.isEmpty())
        return QSize(220, 160);
    return QSize(natural.width() + 2 * kHaloPadding,
                 qMax(kMinLogoHeight, natural.height()) + 2 * kHaloPadding);
}

int LogoView::heightForWidth(int width) const
{
    const QSize natural = nativeLogicalSize();
    if (natural.isEmpty())
        return kMinLogoHeight + 2 * kHaloPadding;

    // In a narrow rail the mark is limited by width, so the height it actually
    // occupies is the scaled-to-fit height - not the taller "natural" height it
    // would take if it had room. Reporting the latter made the view reserve
    // space it never painted into.
    const int available = qMax(1, width - 2 * kHaloPadding);
    QSize fitted = natural;
    if (fitted.width() > available)
        fitted.scale(available, natural.height(), Qt::KeepAspectRatio);

    return qMax(kMinLogoHeight, fitted.height()) + 2 * kHaloPadding;
}

QSize LogoView::minimumSizeHint() const
{
    return QSize(120, kMinLogoHeight + 2 * kHaloPadding);
}

void LogoView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!hasLogo()) {
        // The asset is missing - draw a discreet frame instead of nothing so the
        // window still renders while the logo is being dropped in.
        QPen pen(Theme::alpha(Theme::colors().primary, 90));
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 10, 10);
        painter.setPen(Theme::colors().textMuted);
        painter.setFont(Theme::font(12));
        painter.drawText(rect(), Qt::AlignCenter,
                         QObject::tr("assets/logo/nextgen_tweaks_logo.png"));
        return;
    }

    const QSize target = artworkSize();
    if (target.isEmpty())
        return;

    const QRect box(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2),
                    target);

    // Soft blue halo behind the mark. The gradient reaches zero alpha before the
    // widget edge so it never leaves a visible seam.
    QRadialGradient halo(QPointF(rect().center()), qMin(width(), height()) / 2.0);
    halo.setColorAt(0.0, Theme::alpha(Theme::colors().primary, 44));
    halo.setColorAt(0.45, Theme::alpha(Theme::colors().primary, 16));
    halo.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect(), halo);

    if (!m_svg.isEmpty()) {
        // Rasterise the vector straight onto the device pixel grid.
        QSvgRenderer renderer(m_svg);
        renderer.setAspectRatioMode(Qt::KeepAspectRatio);
        renderer.render(&painter, QRectF(box));
        return;
    }

    // Raster: draw at native device resolution whenever possible, and only ever
    // scale down, using a high quality filter.
    const qreal dpr = devicePixelRatioF();
    const QSize devicePixels(qRound(target.width() * dpr), qRound(target.height() * dpr));
    QPixmap scaled = (devicePixels == m_source.size())
                         ? m_source
                         : m_source.scaled(devicePixels, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    painter.drawPixmap(box.topLeft(), scaled);
}

// -------------------------------------------------------------- AccentLine --

AccentLine::AccentLine(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(3);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

QSize AccentLine::sizeHint() const
{
    return QSize(3, 48);
}

void AccentLine::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0.0, c.cyan);
    gradient.setColorAt(1.0, c.primary);

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(QRectF(rect()), 1.5, 1.5);
}

// ----------------------------------------------------------- BrandingPanel --

BrandingPanel::BrandingPanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    buildUi();
}

void BrandingPanel::buildUi()
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    m_logo = new LogoView(this);
    m_logo->loadBestAsset();

    m_logo->setMaxHeight(kMaxLogoHeight);

    // The "Optimise . Enhance . Perform" pillars are kept as a hidden widget so
    // the active-pillar API stays valid, but they are no longer shown - the logo
    // artwork already carries the wordmark.
    m_pillars = new QLabel(this);
    m_pillars->setTextFormat(Qt::RichText);
    m_pillars->setAlignment(Qt::AlignCenter);
    m_pillars->setFont(Theme::font(15, QFont::Medium, 0.5));
    m_pillars->hide();
    updatePillars();

    // The sidebar navigation sits under the logo. Hidden by default so the
    // sign-in window is unaffected; MainWindow switches it on.
    m_nav = new NavList(this);
    m_nav->hide();
    connect(m_nav, &NavList::homeRequested, this, &BrandingPanel::homeRequested);
    connect(m_nav, &NavList::sectionRequested, this, &BrandingPanel::sectionRequested);
    connect(m_nav, &NavList::activateRequested, this, &BrandingPanel::activateRequested);

    // Social links live in the bottom-left corner of the panel, so they appear
    // on the login screen, the main menu and (compact) on every section rail.
    m_social = new SocialLinksBar(this);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(m.panelPadding, m.panelPadding + m.titleBarHeight,
                                 m.panelPadding, 12);
    m_layout->setSpacing(0);
    m_layout->addStretch(1);
    m_layout->addWidget(m_logo, 0, Qt::AlignHCenter);
    m_layout->addSpacing(10);
    m_layout->addWidget(m_nav);
    m_layout->addStretch(2);
    // The social icons sit clear of the nav rail, but the gap is a fixed 20
    // rather than the 44 it used to be: the two stretches above already push
    // them apart, and the rail has to fit inside a fixed-height window.
    m_layout->addSpacing(20);
    m_layout->addWidget(m_social, 0, Qt::AlignLeft);
}

void BrandingPanel::setNavigationVisible(bool visible)
{
    m_nav->setVisible(visible);
}


void BrandingPanel::setNavigationSection(MenuSection section)
{
    m_nav->setActiveSection(section);
}

void BrandingPanel::setNavigationHome()
{
    m_nav->setHomeActive();
}

void BrandingPanel::setNavigationLocked(bool locked)
{
    m_nav->setLocked(locked);
}

void BrandingPanel::setAdminVisible(bool visible)
{
    m_nav->setAdminVisible(visible);
}

void BrandingPanel::updatePillars()
{
    const Theme::Palette &c = Theme::colors();
    const QString labels[] = {tr("Optimise"), tr("Enhance"), tr("Perform")};

    QString markup;
    for (int i = 0; i < 3; ++i) {
        if (i > 0) {
            markup += QStringLiteral("<span style=\"color:%1;\">"
                                     "&nbsp;&nbsp;&#8226;&nbsp;&nbsp;</span>")
                          .arg(c.primary.name());
        }
        const QColor colour = (i == m_activePillar) ? c.link : c.textSecondary;
        markup += QStringLiteral("<span style=\"color:%1;\">%2</span>")
                      .arg(colour.name(), labels[i]);
    }
    m_pillars->setText(markup);
}

void BrandingPanel::setActivePillar(int index)
{
    if (m_activePillar == index)
        return;
    m_activePillar = index;
    updatePillars();
}

void BrandingPanel::setCompact(bool compact)
{
    if (m_compact == compact)
        return;

    m_compact = compact;
    const Theme::Metrics &m = Theme::metrics();

    m_logo->setMaxHeight(compact ? 190 : kMaxLogoHeight);
    m_social->setIconSize(compact ? 22 : 26);
    m_pillars->setFont(Theme::font(compact ? 13 : 15, QFont::Medium, compact ? 0.2 : 0.5));

    const int side = compact ? 18 : m.panelPadding;
    m_layout->setContentsMargins(side, (compact ? 16 : m.panelPadding) + m.titleBarHeight, side,
                                 compact ? 22 : 18);

    updateGeometry();
    update();
}

void BrandingPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF body(rect());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // --- Base gradient -----------------------------------------------------
    QLinearGradient base(body.topLeft(), body.bottomRight());
    base.setColorAt(0.0, c.brandPanelTop);
    base.setColorAt(1.0, c.brandPanelBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(body);

    // --- Abstract diagonal shapes (kept deliberately faint) ----------------
    painter.save();
    painter.setClipRect(body);

    const qreal w = body.width();
    const qreal h = body.height();

    QPainterPath shardA;
    shardA.moveTo(-w * 0.10, h * 0.02);
    shardA.lineTo(w * 0.62, -h * 0.18);
    shardA.lineTo(w * 0.92, h * 0.22);
    shardA.lineTo(w * 0.24, h * 0.46);
    shardA.closeSubpath();

    QPainterPath shardB;
    shardB.moveTo(w * 0.34, h * 1.18);
    shardB.lineTo(w * 1.16, h * 0.52);
    shardB.lineTo(w * 1.28, h * 1.02);
    shardB.closeSubpath();

    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::alpha(c.brandShape, 7));
    painter.drawPath(shardA);
    painter.setBrush(Theme::alpha(c.brandShape, 6));
    painter.drawPath(shardB);

    // Hairline diagonals.
    QPen hairline(Theme::alpha(c.brandShape, 18));
    hairline.setWidthF(1.0);
    painter.setPen(hairline);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(-w * 0.05, h * 0.74), QPointF(w * 0.78, h * 0.08));
    painter.drawLine(QPointF(w * 0.12, h * 1.05), QPointF(w * 1.05, h * 0.32));

    // --- Corner glow -------------------------------------------------------
    QRadialGradient glow(QPointF(w * 0.18, h * 0.12), qMax(w, h) * 0.75);
    glow.setColorAt(0.0, Theme::alpha(c.primary, 34));
    glow.setColorAt(1.0, Qt::transparent);
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawRect(body);

    QRadialGradient underGlow(QPointF(w * 0.10, h * 0.95), qMax(w, h) * 0.55);
    underGlow.setColorAt(0.0, Theme::alpha(c.cyan, 20));
    underGlow.setColorAt(1.0, Qt::transparent);
    painter.setBrush(underGlow);
    painter.drawRect(body);

    painter.restore();
}
