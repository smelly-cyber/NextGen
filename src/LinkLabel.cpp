#include "LinkLabel.h"

#include "Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>

LinkLabel::LinkLabel(const QString &text, QWidget *parent)
    : QWidget(parent)
    , m_text(text)
{
    const Theme::Palette &c = Theme::colors();
    m_normalColor = c.link;
    m_hoverColor = c.primaryBright;

    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover, true);
    setFont(Theme::font(14));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_hoverAnimation->setDuration(Theme::durations().hover);
}

LinkLabel::~LinkLabel() = default;

void LinkLabel::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    updateGeometry();
    update();
}

void LinkLabel::setColors(const QColor &normal, const QColor &hover)
{
    m_normalColor = normal;
    m_hoverColor = hover;
    update();
}

void LinkLabel::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

QSize LinkLabel::sizeHint() const
{
    const QFontMetrics fm(font());
    return QSize(fm.horizontalAdvance(m_text) + 4, fm.height() + 8);
}

QSize LinkLabel::minimumSizeHint() const
{
    return sizeHint();
}

void LinkLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QColor color = Theme::mix(m_normalColor, m_hoverColor, m_hoverProgress);

    QFont f = font();
    painter.setFont(f);
    painter.setPen(color);
    painter.drawText(rect(), Qt::AlignRight | Qt::AlignVCenter, m_text);

    // Underline fades in with the hover animation.
    if (m_hoverProgress > 0.001) {
        const QFontMetrics fm(f);
        const int textWidth = fm.horizontalAdvance(m_text);
        const qreal baseline = rect().center().y() + fm.ascent() / 2.0 + 2.0;
        QColor line = color;
        line.setAlphaF(0.75 * m_hoverProgress);
        QPen pen(line);
        pen.setWidthF(1.1);
        painter.setPen(pen);
        painter.drawLine(QPointF(width() - textWidth - 2, baseline),
                         QPointF(width() - 2, baseline));
    }

    if (hasFocus()) {
        QPen ring(Theme::colors().focusRing);
        ring.setWidthF(1.1);
        painter.setPen(ring);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
    }
}

void LinkLabel::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
}

void LinkLabel::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
}

void LinkLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Accept the press so the matching release is delivered here.
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void LinkLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        emit clicked();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void LinkLabel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
        || event->key() == Qt::Key_Enter) {
        emit clicked();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void LinkLabel::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update();
}

void LinkLabel::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}
