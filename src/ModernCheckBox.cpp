#include "ModernCheckBox.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPropertyAnimation>

namespace {
/// Gap between the indicator and the label.
constexpr int kLabelGap = 12;
/// Room kept around the indicator so neither its border stroke nor the keyboard
/// focus ring is clipped by the widget edge.
constexpr int kIndicatorInset = 4;
} // namespace

ModernCheckBox::ModernCheckBox(const QString &text, QWidget *parent)
    : QCheckBox(text, parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::StrongFocus);
    setFont(Theme::font(14));

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_checkAnimation = new QPropertyAnimation(this, "checkProgress", this);
    m_checkAnimation->setEasingCurve(QEasingCurve::OutBack);

    connect(this, &QCheckBox::toggled, this, [this](bool checked) {
        animateTo(m_checkAnimation, m_checkProgress, checked ? 1.0 : 0.0,
                  Theme::durations().state);
    });
}

ModernCheckBox::~ModernCheckBox() = default;

void ModernCheckBox::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

void ModernCheckBox::setCheckProgress(qreal value)
{
    if (qFuzzyCompare(m_checkProgress, value))
        return;
    m_checkProgress = value;
    update();
}

QSize ModernCheckBox::sizeHint() const
{
    const QFontMetrics fm(font());
    const int box = Theme::metrics().checkBoxSize;
    return QSize(kIndicatorInset + box + kLabelGap + fm.horizontalAdvance(text()) + 2,
                 qMax(box + 2 * kIndicatorInset, fm.height() + 8));
}

QSize ModernCheckBox::minimumSizeHint() const
{
    return sizeHint();
}

void ModernCheckBox::animateTo(QPropertyAnimation *animation, qreal current, qreal target,
                               int duration)
{
    animation->stop();
    animation->setDuration(duration);
    animation->setStartValue(current);
    animation->setEndValue(target);
    animation->start();
}

void ModernCheckBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const int box = Theme::metrics().checkBoxSize;
    const qreal radius = 6.0;
    const qreal progress = qBound(0.0, m_checkProgress, 1.0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF indicator(kIndicatorInset + 0.5, (height() - box) / 2.0 + 0.5, box - 1.0,
                           box - 1.0);

    // --- Unchecked shell ---------------------------------------------------
    QColor shellFill = Theme::mix(c.fieldBackground, c.fieldBackgroundHover, m_hoverProgress);
    painter.setPen(Qt::NoPen);
    painter.setBrush(shellFill);
    painter.drawRoundedRect(indicator, radius, radius);

    QPen shellBorder(Theme::mix(c.fieldBorder, c.fieldBorderHover, m_hoverProgress));
    shellBorder.setWidthF(1.2);
    painter.setPen(shellBorder);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(indicator, radius, radius);

    // --- Checked fill (scales in) -----------------------------------------
    if (progress > 0.001) {
        painter.save();
        painter.setOpacity(qBound(0.0, progress * 1.4, 1.0));
        painter.translate(indicator.center());
        const qreal scale = 0.6 + 0.4 * qBound(0.0, progress, 1.0);
        painter.scale(scale, scale);
        painter.translate(-indicator.center());

        QLinearGradient fill(indicator.topLeft(), indicator.bottomRight());
        fill.setColorAt(0.0, c.primaryBright);
        fill.setColorAt(1.0, c.primaryDeep);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(indicator, radius, radius);
        painter.restore();

        const int glyphSize = box - 6;
        const QPixmap check = IconProvider::pixmap(QStringLiteral("check"), glyphSize,
                                                   c.textOnPrimary, devicePixelRatioF());
        if (!check.isNull()) {
            painter.save();
            painter.setOpacity(qBound(0.0, (progress - 0.25) / 0.75, 1.0));
            painter.drawPixmap(QPointF(indicator.center().x() - glyphSize / 2.0,
                                       indicator.center().y() - glyphSize / 2.0),
                               check);
            painter.restore();
        }
    }

    // --- Hover / focus ring ------------------------------------------------
    if (hasFocus()) {
        QPen ring(Theme::colors().focusRing);
        ring.setWidthF(1.2);
        painter.setPen(ring);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(indicator.adjusted(-3, -3, 3, 3), radius + 3, radius + 3);
    }

    // --- Label -------------------------------------------------------------
    painter.setPen(Theme::mix(c.textSecondary, c.textPrimary, qMax(m_hoverProgress, progress)));
    painter.setFont(font());
    const qreal labelLeft = indicator.right() + kLabelGap;
    const QRectF labelRect(labelLeft, 0, width() - labelLeft, height());
    painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, text());
}

void ModernCheckBox::enterEvent(QEnterEvent *event)
{
    QCheckBox::enterEvent(event);
    animateTo(m_hoverAnimation, m_hoverProgress, 1.0, Theme::durations().hover);
}

void ModernCheckBox::leaveEvent(QEvent *event)
{
    QCheckBox::leaveEvent(event);
    animateTo(m_hoverAnimation, m_hoverProgress, 0.0, Theme::durations().hover);
}

void ModernCheckBox::focusInEvent(QFocusEvent *event)
{
    QCheckBox::focusInEvent(event);
    update();
}

void ModernCheckBox::focusOutEvent(QFocusEvent *event)
{
    QCheckBox::focusOutEvent(event);
    update();
}
