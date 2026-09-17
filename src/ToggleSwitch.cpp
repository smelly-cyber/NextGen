#include "ToggleSwitch.h"

#include "Theme.h"

#include <QEvent>
#include <QPainter>
#include <QPropertyAnimation>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_slideAnimation = new QPropertyAnimation(this, "slideProgress", this);
    m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_slideAnimation->setDuration(Theme::durations().state);

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_hoverAnimation->setDuration(Theme::durations().hover);

    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        m_slideAnimation->stop();
        m_slideAnimation->setStartValue(m_slideProgress);
        m_slideAnimation->setEndValue(on ? 1.0 : 0.0);
        m_slideAnimation->start();
    });
}

ToggleSwitch::~ToggleSwitch() = default;

void ToggleSwitch::setSlideProgress(qreal value)
{
    if (qFuzzyCompare(m_slideProgress, value))
        return;
    m_slideProgress = value;
    update();
}

void ToggleSwitch::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

QSize ToggleSwitch::sizeHint() const
{
    const Theme::Metrics &m = Theme::metrics();
    return QSize(m.toggleWidth + 6, m.toggleHeight + 6);
}

void ToggleSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track((width() - m.toggleWidth) / 2.0, (height() - m.toggleHeight) / 2.0,
                       m.toggleWidth, m.toggleHeight);
    const qreal radius = track.height() / 2.0;

    if (!isEnabled())
        painter.setOpacity(0.45);

    // --- Glow while on -----------------------------------------------------
    if (m_slideProgress > 0.001) {
        QColor glow = c.primary;
        glow.setAlphaF(0.22 * m_slideProgress);
        QPen pen(glow);
        pen.setWidthF(2.4);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(-2, -2, 2, 2), radius + 2, radius + 2);
    }

    // --- Track -------------------------------------------------------------
    QColor trackColor = Theme::mix(c.toggleTrackOff, c.toggleTrackOn, m_slideProgress);
    trackColor = trackColor.lighter(100 + int(8 * m_hoverProgress));
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, radius, radius);

    if (hasFocus()) {
        QPen ring(c.focusRing);
        ring.setWidthF(1.2);
        painter.setPen(ring);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(-2.5, -2.5, 2.5, 2.5), radius + 2.5, radius + 2.5);
    }

    // --- Knob --------------------------------------------------------------
    const qreal inset = 3.0;
    const qreal knobDiameter = track.height() - 2 * inset;
    const qreal travel = track.width() - 2 * inset - knobDiameter;
    const QRectF knob(track.left() + inset + travel * m_slideProgress, track.top() + inset,
                      knobDiameter, knobDiameter);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 60));
    painter.drawEllipse(knob.translated(0, 0.8));
    painter.setBrush(c.toggleKnob);
    painter.drawEllipse(knob);
}

void ToggleSwitch::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
}

void ToggleSwitch::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
}

void ToggleSwitch::focusInEvent(QFocusEvent *event)
{
    QAbstractButton::focusInEvent(event);
    update();
}

void ToggleSwitch::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    update();
}
