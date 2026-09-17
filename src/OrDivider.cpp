#include "OrDivider.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>

namespace {
/// Space between the label and the lines on either side.
constexpr int kLabelPadding = 18;
} // namespace

OrDivider::OrDivider(const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_label(label)
{
    setFont(Theme::font(13, QFont::Medium, 0.6));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void OrDivider::setLabel(const QString &label)
{
    if (m_label == label)
        return;
    m_label = label;
    updateGeometry();
    update();
}

QSize OrDivider::sizeHint() const
{
    const QFontMetrics fm(font());
    return QSize(240, qMax(fm.height() + 6, 22));
}

QSize OrDivider::minimumSizeHint() const
{
    const QFontMetrics fm(font());
    return QSize(fm.horizontalAdvance(m_label) + 80, qMax(fm.height() + 6, 22));
}

void OrDivider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QFontMetrics fm(font());
    const int labelWidth = fm.horizontalAdvance(m_label);
    const qreal centerY = height() / 2.0;
    const qreal labelLeft = (width() - labelWidth) / 2.0;
    const qreal labelRight = labelLeft + labelWidth;

    const qreal leftEnd = labelLeft - kLabelPadding;
    const qreal rightStart = labelRight + kLabelPadding;

    // Left line: fades in from the outer edge towards the label.
    if (leftEnd > 0) {
        QLinearGradient g(0, 0, leftEnd, 0);
        g.setColorAt(0.0, Theme::alpha(c.divider, 0));
        g.setColorAt(0.55, c.divider);
        g.setColorAt(1.0, Theme::alpha(c.divider, 190));
        QPen pen(QBrush(g), 1.0);
        painter.setPen(pen);
        painter.drawLine(QPointF(0, centerY), QPointF(leftEnd, centerY));
    }

    if (rightStart < width()) {
        QLinearGradient g(rightStart, 0, width(), 0);
        g.setColorAt(0.0, Theme::alpha(c.divider, 190));
        g.setColorAt(0.45, c.divider);
        g.setColorAt(1.0, Theme::alpha(c.divider, 0));
        QPen pen(QBrush(g), 1.0);
        painter.setPen(pen);
        painter.drawLine(QPointF(rightStart, centerY), QPointF(width(), centerY));
    }

    painter.setFont(font());
    painter.setPen(c.textMuted);
    painter.drawText(rect(), Qt::AlignCenter, m_label);
}
