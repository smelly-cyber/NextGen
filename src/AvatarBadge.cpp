#include "AvatarBadge.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRadialGradient>

namespace {
/// Space around the circle reserved for its glow.
constexpr int kGlowPadding = 5;
} // namespace

AvatarBadge::AvatarBadge(QWidget *parent)
    : QAbstractButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover, true);
    setToolTip(tr("Account"));
    setAccessibleName(tr("Account"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_hoverAnimation->setDuration(Theme::durations().hover);
}

AvatarBadge::~AvatarBadge() = default;

void AvatarBadge::setOnline(bool online)
{
    if (m_online == online)
        return;
    m_online = online;
    update();
}

void AvatarBadge::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

QSize AvatarBadge::sizeHint() const
{
    const int side = Theme::metrics().avatarSize + 2 * kGlowPadding;
    return QSize(side, side);
}

void AvatarBadge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF circle = QRectF(rect()).adjusted(kGlowPadding, kGlowPadding,
                                                  -kGlowPadding, -kGlowPadding);

    // --- Glow --------------------------------------------------------------
    QRadialGradient glow(circle.center(), circle.width() / 2.0 + kGlowPadding);
    glow.setColorAt(0.65, Qt::transparent);
    glow.setColorAt(0.86, Theme::alpha(c.primary, int(60 + 50 * m_hoverProgress)));
    glow.setColorAt(1.0, Qt::transparent);
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawEllipse(QRectF(rect()));

    // --- Disc --------------------------------------------------------------
    QRadialGradient disc(circle.center() - QPointF(0, circle.height() * 0.15),
                         circle.width() * 0.8);
    disc.setColorAt(0.0, Theme::mix(c.iconTileTop, c.primary.darker(170), 0.5));
    disc.setColorAt(1.0, c.iconTileBottom);
    painter.setBrush(disc);
    painter.drawEllipse(circle);

    QPen ring(Theme::mix(c.primary, c.primaryBright, m_hoverProgress));
    ring.setWidthF(1.6);
    painter.setPen(ring);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(circle.adjusted(0.8, 0.8, -0.8, -0.8));

    // --- Silhouette --------------------------------------------------------
    const int glyphSize = qRound(circle.width() * 0.62);
    const QPixmap glyph = IconProvider::pixmap(QStringLiteral("avatar"), glyphSize,
                                               Theme::mix(c.primaryBright, c.cyan,
                                                          m_hoverProgress * 0.5),
                                               devicePixelRatioF());
    if (!glyph.isNull()) {
        painter.drawPixmap(QPointF(circle.center().x() - glyphSize / 2.0,
                                   circle.center().y() - glyphSize / 2.0 + 1),
                           glyph);
    }

    // --- Presence dot ------------------------------------------------------
    if (m_online) {
        const qreal dot = circle.width() * 0.2;
        const QPointF centre(circle.right() - dot * 0.55, circle.bottom() - dot * 0.75);
        const QRectF dotRect(centre.x() - dot / 2.0, centre.y() - dot / 2.0, dot, dot);

        painter.setPen(QPen(c.windowBottom, 2.2));
        painter.setBrush(c.online);
        painter.drawEllipse(dotRect);
    }

    if (hasFocus()) {
        QPen focus(c.focusRing);
        focus.setWidthF(1.2);
        painter.setPen(focus);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRectF(rect()).adjusted(1, 1, -1, -1));
    }
}

void AvatarBadge::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
}

void AvatarBadge::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
}

void AvatarBadge::focusInEvent(QFocusEvent *event)
{
    QAbstractButton::focusInEvent(event);
    update();
}

void AvatarBadge::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    update();
}
