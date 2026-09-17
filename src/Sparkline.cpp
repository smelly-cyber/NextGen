#include "Sparkline.h"

#include "Theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

Sparkline::Sparkline(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void Sparkline::setValues(const QList<qreal> &values)
{
    m_values = values;
    update();
}

QSize Sparkline::sizeHint() const
{
    return QSize(140, 34);
}

QSize Sparkline::minimumSizeHint() const
{
    return QSize(60, 24);
}

void Sparkline::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if (m_values.size() < 2)
        return;

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(1, 2, -1, -2);
    const int shown = m_values.size();

    // Whatever history exists is spread across the full width, so the trace is
    // readable from the very first samples instead of creeping in from the edge.
    const qreal step = body.width() / qreal(shown - 1);

    QPainterPath line;
    for (int i = 0; i < shown; ++i) {
        const qreal value = qBound(0.0, m_values.at(i), 100.0);
        const qreal x = body.left() + step * i;
        const qreal y = body.bottom() - (value / 100.0) * body.height();
        if (i == 0)
            line.moveTo(x, y);
        else
            line.lineTo(x, y);
    }

    // Soft fill under the curve.
    QPainterPath area = line;
    area.lineTo(body.right(), body.bottom());
    area.lineTo(body.left(), body.bottom());
    area.closeSubpath();

    QLinearGradient fade(body.topLeft(), body.bottomLeft());
    fade.setColorAt(0.0, Theme::alpha(c.primary, 64));
    fade.setColorAt(1.0, Qt::transparent);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fade);
    painter.drawPath(area);

    QPen stroke(c.primary);
    stroke.setWidthF(1.4);
    stroke.setJoinStyle(Qt::RoundJoin);
    painter.setPen(stroke);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(line);
}
