#include "LiveMonitorChart.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace {
/// Room on the left for the percentage scale.
constexpr int kAxisWidth = 44;
/// Room at the bottom for the time labels.
constexpr int kAxisHeight = 24;
/// Room at the top for the legend.
constexpr int kLegendHeight = 22;

const qreal kTicks[] = {0.0, 25.0, 50.0, 75.0, 100.0};
} // namespace

LiveMonitorChart::LiveMonitorChart(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

int LiveMonitorChart::addSeries(const QString &name, const QColor &color)
{
    Series series;
    series.name = name;
    series.color = color;
    m_series.append(series);
    update();
    return m_series.size() - 1;
}

void LiveMonitorChart::setSeriesValues(int index, const QList<qreal> &values)
{
    if (index < 0 || index >= m_series.size())
        return;
    m_series[index].values = values;
    update();
}

void LiveMonitorChart::setTimeSpan(int slotCount, qreal secondsPerSlot)
{
    m_slots = qMax(2, slotCount);
    m_secondsPerSlot = secondsPerSlot;
    update();
}

QSize LiveMonitorChart::sizeHint() const
{
    return QSize(420, 230);
}

QSize LiveMonitorChart::minimumSizeHint() const
{
    return QSize(260, 170);
}

QRectF LiveMonitorChart::plotArea() const
{
    return QRectF(rect()).adjusted(kAxisWidth, kLegendHeight, -6, -kAxisHeight);
}

void LiveMonitorChart::paintLegend(QPainter &painter)
{
    if (m_series.isEmpty())
        return;

    const Theme::Palette &c = Theme::colors();
    const QFont legendFont = Theme::font(12, QFont::Medium);
    const QFontMetrics metrics(legendFont);
    painter.setFont(legendFont);

    // Measure first so the whole block can be right aligned.
    qreal total = 0;
    for (const Series &series : m_series)
        total += 8 + 8 + metrics.horizontalAdvance(series.name) + 18;

    qreal x = qMax(qreal(kAxisWidth), rect().right() - 6 - total);
    const qreal y = kLegendHeight / 2.0;

    for (const Series &series : m_series) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(series.color);
        painter.drawEllipse(QPointF(x + 4, y), 4, 4);
        x += 14;

        const int width = metrics.horizontalAdvance(series.name);
        painter.setPen(c.textSecondary);
        painter.drawText(QRectF(x, y - metrics.height() / 2.0, width + 2, metrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, series.name);
        x += width + 18;
    }
}

void LiveMonitorChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF plot = plotArea();
    if (plot.width() < 20 || plot.height() < 20)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    paintLegend(painter);

    const QFont scaleFont = Theme::font(11);
    const QFontMetrics scaleMetrics(scaleFont);
    painter.setFont(scaleFont);

    const auto yFor = [&](qreal value) {
        return plot.bottom() - qBound(0.0, value, 100.0) / 100.0 * plot.height();
    };

    // --- Plot backdrop -------------------------------------------------------
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::alpha(c.windowBottom, 150));
    painter.drawRoundedRect(plot.adjusted(-4, -4, 4, 4), 8, 8);

    // --- Gridlines and scale -------------------------------------------------
    for (qreal tick : kTicks) {
        const qreal y = yFor(tick);

        QPen grid(c.chartGrid);
        grid.setWidthF(1.0);
        if (tick > 0.0 && tick < 100.0)
            grid.setStyle(Qt::DotLine);
        painter.setPen(grid);
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

        painter.setPen(c.textMuted);
        painter.drawText(QRectF(0, y - scaleMetrics.height() / 2.0, kAxisWidth - 8,
                                scaleMetrics.height()),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1%").arg(qRound(tick)));
    }

    // --- Series --------------------------------------------------------------
    const qreal step = plot.width() / qreal(m_slots - 1);

    for (const Series &series : m_series) {
        if (series.values.size() < 2)
            continue;

        const int shown = qMin(series.values.size(), m_slots);
        // Newest sample sits at "Now" on the right hand edge.
        const qreal firstX = plot.right() - step * (shown - 1);

        QPainterPath line;
        for (int i = 0; i < shown; ++i) {
            const qreal value = series.values.at(series.values.size() - shown + i);
            const QPointF point(firstX + step * i, yFor(value));
            if (i == 0)
                line.moveTo(point);
            else
                line.lineTo(point);
        }

        QPainterPath area = line;
        area.lineTo(plot.right(), plot.bottom());
        area.lineTo(firstX, plot.bottom());
        area.closeSubpath();

        QLinearGradient fade(plot.topLeft(), plot.bottomLeft());
        fade.setColorAt(0.0, Theme::alpha(series.color, 90));
        fade.setColorAt(1.0, Theme::alpha(series.color, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(fade);
        painter.drawPath(area);

        QPen stroke(series.color);
        stroke.setWidthF(1.6);
        stroke.setJoinStyle(Qt::RoundJoin);
        painter.setPen(stroke);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(line);
    }

    // --- Time axis -----------------------------------------------------------
    const qreal span = (m_slots - 1) * m_secondsPerSlot;
    const int labelCount = 5;

    painter.setFont(scaleFont);
    painter.setPen(c.textMuted);

    for (int i = 0; i < labelCount; ++i) {
        const qreal t = qreal(i) / (labelCount - 1);
        const qreal x = plot.left() + plot.width() * t;
        const int secondsAgo = qRound(span * (1.0 - t));

        const QString label = secondsAgo == 0 ? tr("Now")
                                              : QStringLiteral("-%1s").arg(secondsAgo);
        const int width = scaleMetrics.horizontalAdvance(label);

        qreal left = x - width / 2.0;
        if (i == 0)
            left = x;
        else if (i == labelCount - 1)
            left = x - width;

        painter.drawText(QRectF(left, plot.bottom() + 6, width + 2, kAxisHeight - 8),
                         Qt::AlignLeft | Qt::AlignTop, label);
    }
}
