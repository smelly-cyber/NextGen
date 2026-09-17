#include "MenuCard.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

namespace {
/// Space reserved around the card body for its selection glow.
constexpr int kGlowPadding = 6;
/// Inner padding from the card edge to its content.
constexpr int kPadding = 22;
/// Gap between the icon tile and the text column.
constexpr int kTextGap = 18;
/// Rounded icon tile.
constexpr int kTileSize = 84;
constexpr int kTileRadius = 18;
constexpr int kTileIconSize = 38;
/// Circular chevron button on the right.
constexpr int kChevronBox = 46;
constexpr int kChevronGlyph = 19;
/// Capability chips.
constexpr int kTagHeight = 27;
constexpr int kTagGap = 10;
constexpr int kTagPaddingX = 13;
/// Card heights, with and without the chip row.
constexpr int kBodyHeightTagged = 196;
constexpr int kBodyHeightPlain = 122;
} // namespace

MenuCard::MenuCard(MenuSection section, const QString &iconName, const QString &title,
                   const QString &subtitle, QWidget *parent)
    : QAbstractButton(parent)
    , m_section(section)
    , m_iconName(iconName)
    , m_subtitle(subtitle)
    , m_watermarkIcon(iconName) // Artwork mirrors the tile icon by default.
{
    setText(title);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setFont(Theme::font(21, QFont::DemiBold));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(title);
    setAccessibleDescription(subtitle);

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_pressAnimation = new QPropertyAnimation(this, "pressProgress", this);
    m_pressAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_selectAnimation = new QPropertyAnimation(this, "selectProgress", this);
    m_selectAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        animateTo(m_selectAnimation, m_selectProgress, on ? 1.0 : 0.0, Theme::durations().state);
    });
}

MenuCard::~MenuCard() = default;

void MenuCard::setSubtitle(const QString &subtitle)
{
    if (m_subtitle == subtitle)
        return;
    m_subtitle = subtitle;
    setAccessibleDescription(subtitle);
    update();
}

void MenuCard::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    setChecked(false);
    update();
}

void MenuCard::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

void MenuCard::setPressProgress(qreal value)
{
    if (qFuzzyCompare(m_pressProgress, value))
        return;
    m_pressProgress = value;
    update();
}

void MenuCard::setSelectProgress(qreal value)
{
    if (qFuzzyCompare(m_selectProgress, value))
        return;
    m_selectProgress = value;
    update();
}

void MenuCard::setTags(const QStringList &tags)
{
    if (m_tags == tags)
        return;
    m_tags = tags;
    updateGeometry();
    update();
}

void MenuCard::setWatermarkIcon(const QString &iconName)
{
    if (m_watermarkIcon == iconName)
        return;
    m_watermarkIcon = iconName;
    update();
}

QSize MenuCard::sizeHint() const
{
    const int body = m_tags.isEmpty() ? kBodyHeightPlain : kBodyHeightTagged;
    return QSize(520, body + 2 * kGlowPadding);
}

QSize MenuCard::minimumSizeHint() const
{
    const int body = m_tags.isEmpty() ? kBodyHeightPlain : kBodyHeightTagged;
    return QSize(320, body + 2 * kGlowPadding);
}

void MenuCard::animateTo(QPropertyAnimation *animation, qreal current, qreal target, int duration)
{
    animation->stop();
    animation->setDuration(duration);
    animation->setStartValue(current);
    animation->setEndValue(target);
    animation->start();
}

void MenuCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();
    const qreal radius = m.cardRadius;

    // How "lit up" the card is: selected counts fully, hover a bit less.
    const qreal accent = qBound(0.0, qMax(m_selectProgress, m_hoverProgress * 0.85), 1.0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_locked)
        painter.setOpacity(0.5);

    QRectF body = QRectF(rect()).adjusted(kGlowPadding, kGlowPadding, -kGlowPadding, -kGlowPadding);
    const qreal shrink = 1.0 * m_pressProgress;
    body = body.adjusted(shrink, shrink, -shrink, -shrink);

    // --- Selection glow ----------------------------------------------------
    if (accent > 0.001) {
        painter.setBrush(Qt::NoBrush);
        for (int i = 1; i <= kGlowPadding; ++i) {
            QColor ring = c.primary;
            ring.setAlphaF(0.15 * accent * (1.0 - qreal(i - 1) / kGlowPadding));
            QPen pen(ring);
            pen.setWidthF(1.8);
            painter.setPen(pen);
            painter.drawRoundedRect(body.adjusted(-i, -i, i, i), radius + i, radius + i);
        }
    }

    // --- Card fill ---------------------------------------------------------
    QLinearGradient fill(body.topLeft(), body.bottomRight());
    fill.setColorAt(0.0, Theme::mix(c.cardTop, c.cardHoverTop, accent));
    fill.setColorAt(1.0, Theme::mix(c.cardBottom, c.cardHoverBottom, accent));
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(body, radius, radius);

    // A soft blue wash bleeding in from the left edge of an active card.
    if (accent > 0.001) {
        QPainterPath clip;
        clip.addRoundedRect(body, radius, radius);
        painter.save();
        painter.setClipPath(clip);
        QLinearGradient wash(body.topLeft(), QPointF(body.left() + body.width() * 0.6, body.top()));
        wash.setColorAt(0.0, Theme::alpha(c.primary, int(38 * accent)));
        wash.setColorAt(1.0, Qt::transparent);
        painter.setBrush(wash);
        painter.drawRect(body);
        painter.restore();
    }

    // --- Border ------------------------------------------------------------
    QPen border(Theme::mix(c.cardBorder, c.cardBorderSelected, accent));
    border.setWidthF(1.0 + 0.4 * accent);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    const qreal half = border.widthF() / 2.0;
    painter.drawRoundedRect(body.adjusted(half, half, -half, -half), radius, radius);

    if (hasFocus()) {
        QPen ring(Theme::colors().focusRing);
        ring.setWidthF(1.2);
        painter.setPen(ring);
        painter.drawRoundedRect(body.adjusted(-3, -3, 3, 3), radius + 3, radius + 3);
    }

    // --- Watermark artwork --------------------------------------------------
    // A large glyph bled off the bottom-right of the card. Clipped to the card
    // so it never spills past the rounded corners, and paired with a soft blue
    // wash so the artwork sits in light rather than floating on the flat fill.
    if (!m_watermarkIcon.isEmpty()) {
        QPainterPath clip;
        clip.addRoundedRect(body, radius, radius);
        painter.save();
        painter.setClipPath(clip);

        QLinearGradient wash(QPointF(body.center().x(), body.top()), body.bottomRight());
        wash.setColorAt(0.0, Qt::transparent);
        wash.setColorAt(1.0, Theme::alpha(c.primary, int(22 + 30 * accent)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(wash);
        painter.drawRect(body);

        const int artSize = qRound(body.height() * 1.30);
        const QPixmap art = IconProvider::pixmap(m_watermarkIcon, artSize,
                                                 Theme::alpha(c.primaryBright,
                                                              int(40 + 30 * accent)),
                                                 devicePixelRatioF());
        if (!art.isNull()) {
            painter.drawPixmap(QPointF(body.right() - artSize * 0.80,
                                       body.bottom() - artSize * 0.80),
                               art);
        }
        painter.restore();
    }

    // --- Accent bar along the top edge --------------------------------------
    {
        QPainterPath clip;
        clip.addRoundedRect(body, radius, radius);
        painter.save();
        painter.setClipPath(clip);
        const qreal barWidth = body.width() * (0.16 + 0.10 * accent);
        QLinearGradient barFill(body.topLeft(), QPointF(body.left() + barWidth, body.top()));
        barFill.setColorAt(0.0, c.cyan);
        barFill.setColorAt(1.0, Theme::alpha(c.primary, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(barFill);
        painter.drawRect(QRectF(body.left(), body.top(), barWidth, 3.0));
        painter.restore();
    }

    // --- Icon tile ----------------------------------------------------------
    const bool tagged = !m_tags.isEmpty();
    const qreal contentTop = body.top() + kPadding;
    const QRectF tile(body.left() + kPadding,
                      tagged ? contentTop : body.center().y() - kTileSize / 2.0,
                      kTileSize, kTileSize);

    QLinearGradient tileFill(tile.topLeft(), tile.bottomRight());
    tileFill.setColorAt(0.0, Theme::mix(c.iconTileTop, c.primary, 0.22 + 0.30 * accent));
    tileFill.setColorAt(1.0, Theme::mix(c.iconTileBottom, c.primaryDeep, 0.18 + 0.25 * accent));
    painter.setPen(Qt::NoPen);
    painter.setBrush(tileFill);
    painter.drawRoundedRect(tile, kTileRadius, kTileRadius);

    QPen tileBorder(Theme::mix(c.iconTileBorder, c.primaryBright, 0.25 + 0.55 * accent));
    tileBorder.setWidthF(1.0);
    painter.setPen(tileBorder);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(tile.adjusted(0.5, 0.5, -0.5, -0.5), kTileRadius, kTileRadius);

    const QColor glyphColor = Theme::mix(c.primaryBright, c.cyan, accent * 0.5);
    const QPixmap glyph = IconProvider::pixmap(m_iconName, kTileIconSize, glyphColor,
                                               devicePixelRatioF());
    if (!glyph.isNull()) {
        painter.drawPixmap(QPointF(tile.center().x() - kTileIconSize / 2.0,
                                   tile.center().y() - kTileIconSize / 2.0),
                           glyph);
    }

    // --- Circular chevron ---------------------------------------------------
    const QRectF chevronBox(body.right() - kPadding - kChevronBox,
                            tile.center().y() - kChevronBox / 2.0, kChevronBox, kChevronBox);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::alpha(c.primary, int(26 + 44 * accent)));
    painter.drawEllipse(chevronBox);

    QPen chevronRing(Theme::mix(c.cardBorder, c.primaryBright, 0.3 + 0.6 * accent));
    chevronRing.setWidthF(1.0);
    painter.setPen(chevronRing);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(chevronBox.adjusted(0.5, 0.5, -0.5, -0.5));

    const QString trailingIcon = m_locked ? QStringLiteral("lock")
                                          : QStringLiteral("chevron_right");
    const QColor chevronColor = m_locked ? c.textMuted
                                         : Theme::mix(c.chevron, c.primaryBright, accent);
    const qreal nudge = m_locked ? 0.0 : 2.0 * qMax(m_hoverProgress, m_selectProgress);
    const QPixmap chevron = IconProvider::pixmap(trailingIcon, kChevronGlyph, chevronColor,
                                                 devicePixelRatioF());
    if (!chevron.isNull()) {
        painter.drawPixmap(QPointF(chevronBox.center().x() - kChevronGlyph / 2.0 + nudge,
                                   chevronBox.center().y() - kChevronGlyph / 2.0),
                           chevron);
    }

    // --- Title and description ----------------------------------------------
    const qreal textLeft = tile.right() + kTextGap;
    const qreal textRight = chevronBox.left() - 16.0;
    const qreal textWidth = qMax(10.0, textRight - textLeft);

    const QFont titleFont = font();
    const QFont subtitleFont = Theme::font(15);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics subtitleMetrics(subtitleFont);

    // Description wraps onto a second line, so measure the block it needs.
    const QRectF descBounds(textLeft, 0, textWidth, subtitleMetrics.height() * 2 + 2);
    const QRectF descNeeded = painter.boundingRect(descBounds,
                                                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                                   m_subtitle);
    const qreal blockHeight = titleMetrics.height() + 5 + descNeeded.height();
    const qreal blockTop = tagged ? tile.top() + 2.0
                                  : body.center().y() - blockHeight / 2.0;

    painter.setFont(titleFont);
    painter.setPen(Theme::mix(c.textPrimary, QColor(0xFF, 0xFF, 0xFF), accent * 0.5));
    painter.drawText(QRectF(textLeft, blockTop, textWidth, titleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, text());

    painter.setFont(subtitleFont);
    painter.setPen(Theme::mix(c.textSecondary, c.textPrimary, accent * 0.35));
    painter.drawText(QRectF(textLeft, blockTop + titleMetrics.height() + 5, textWidth,
                            subtitleMetrics.height() * 2 + 2),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_subtitle);

    // --- Capability chips ---------------------------------------------------
    if (tagged) {
        const QFont tagFont = Theme::font(12, QFont::DemiBold, 0.8);
        const QFontMetrics tagMetrics(tagFont);
        painter.setFont(tagFont);

        qreal x = body.left() + kPadding;
        const qreal y = body.bottom() - kPadding - kTagHeight;

        for (const QString &tag : m_tags) {
            const QString label = tag.toUpper();
            const qreal chipWidth = tagMetrics.horizontalAdvance(label) + kTagPaddingX * 2;
            const QRectF chip(x, y, chipWidth, kTagHeight);
            if (chip.right() > chevronBox.left() - 10.0)
                break; // Out of room - drop the remaining chips rather than overlap.

            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::alpha(c.primary, int(20 + 26 * accent)));
            painter.drawRoundedRect(chip, kTagHeight / 2.0, kTagHeight / 2.0);

            QPen chipBorder(Theme::mix(c.cardBorder, c.primaryBright, 0.25 + 0.5 * accent));
            chipBorder.setWidthF(1.0);
            painter.setPen(chipBorder);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(chip.adjusted(0.5, 0.5, -0.5, -0.5), kTagHeight / 2.0,
                                    kTagHeight / 2.0);

            painter.setPen(Theme::mix(c.textSecondary, c.primaryBright, 0.35 + 0.45 * accent));
            painter.drawText(chip, Qt::AlignCenter, label);

            x += chipWidth + kTagGap;
        }
    }
}

void MenuCard::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    if (isEnabled())
        animateTo(m_hoverAnimation, m_hoverProgress, 1.0, Theme::durations().hover);
}

void MenuCard::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    animateTo(m_hoverAnimation, m_hoverProgress, 0.0, Theme::durations().hover);
}

void MenuCard::mousePressEvent(QMouseEvent *event)
{
    QAbstractButton::mousePressEvent(event);
    if (isEnabled())
        animateTo(m_pressAnimation, m_pressProgress, 1.0, Theme::durations().press);
}

void MenuCard::mouseReleaseEvent(QMouseEvent *event)
{
    QAbstractButton::mouseReleaseEvent(event);
    animateTo(m_pressAnimation, m_pressProgress, 0.0, Theme::durations().press);
}

void MenuCard::focusInEvent(QFocusEvent *event)
{
    QAbstractButton::focusInEvent(event);
    update();
}

void MenuCard::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    update();
}
