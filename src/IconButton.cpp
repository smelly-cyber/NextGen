#include "IconButton.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

IconButton::IconButton(const QString &iconName, QWidget *parent)
    : QAbstractButton(parent)
    , m_iconName(iconName)
{
    const Theme::Palette &c = Theme::colors();

    m_normalColor     = c.textSecondary;
    m_hoverColor      = c.textPrimary;
    m_hoverBackground = c.controlHover;
    m_cornerRadius    = Theme::metrics().windowButtonRadius;

    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover, true);

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_pressAnimation = new QPropertyAnimation(this, "pressProgress", this);
    m_pressAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

IconButton::~IconButton() = default;

void IconButton::setIconName(const QString &iconName)
{
    if (m_iconName == iconName)
        return;
    m_iconName = iconName;
    update();
}

void IconButton::setIconSize(int size)
{
    m_iconPixelSize = size;
    updateGeometry();
    update();
}

void IconButton::setCornerRadius(int radius)
{
    m_cornerRadius = radius;
    update();
}

void IconButton::setColors(const QColor &normal, const QColor &hover)
{
    m_normalColor = normal;
    m_hoverColor = hover;
    update();
}

void IconButton::setHoverBackground(const QColor &color)
{
    m_hoverBackground = color;
    update();
}

void IconButton::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

void IconButton::setPressProgress(qreal value)
{
    if (qFuzzyCompare(m_pressProgress, value))
        return;
    m_pressProgress = value;
    update();
}

QSize IconButton::sizeHint() const
{
    const int side = Theme::metrics().windowButtonSize;
    return QSize(side, side);
}

void IconButton::animate(QPropertyAnimation *animation, qreal to, int duration)
{
    animation->stop();
    animation->setDuration(duration);
    animation->setStartValue(animation == m_hoverAnimation ? m_hoverProgress : m_pressProgress);
    animation->setEndValue(to);
    animation->start();
}

void IconButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal wash = qBound(0.0, m_hoverProgress + m_pressProgress * 0.4, 1.0);

    if (wash > 0.001) {
        QColor bg = m_hoverBackground;
        bg.setAlphaF(bg.alphaF() * wash);
        painter.setPen(Qt::NoPen);
        painter.setBrush(bg);
        painter.drawRoundedRect(box, m_cornerRadius, m_cornerRadius);
    }

    if (hasFocus()) {
        QPen ring(Theme::colors().focusRing);
        ring.setWidthF(1.2);
        painter.setPen(ring);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box, m_cornerRadius, m_cornerRadius);
    }

    const QColor glyphColor = Theme::mix(m_normalColor, m_hoverColor, m_hoverProgress);
    const QPixmap glyph = IconProvider::pixmap(m_iconName, m_iconPixelSize, glyphColor,
                                               devicePixelRatioF());
    if (!glyph.isNull()) {
        const qreal scale = 1.0 - 0.06 * m_pressProgress;
        painter.save();
        painter.translate(width() / 2.0, height() / 2.0);
        painter.scale(scale, scale);
        painter.drawPixmap(QPointF(-m_iconPixelSize / 2.0, -m_iconPixelSize / 2.0), glyph);
        painter.restore();
    }
}

void IconButton::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    animate(m_hoverAnimation, 1.0, Theme::durations().hover);
}

void IconButton::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    animate(m_hoverAnimation, 0.0, Theme::durations().hover);
}

void IconButton::mousePressEvent(QMouseEvent *event)
{
    QAbstractButton::mousePressEvent(event);
    animate(m_pressAnimation, 1.0, Theme::durations().press);
}

void IconButton::mouseReleaseEvent(QMouseEvent *event)
{
    QAbstractButton::mouseReleaseEvent(event);
    animate(m_pressAnimation, 0.0, Theme::durations().press);
}

void IconButton::focusInEvent(QFocusEvent *event)
{
    QAbstractButton::focusInEvent(event);
    update();
}

void IconButton::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    update();
}
