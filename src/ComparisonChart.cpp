#include "ComparisonChart.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace {
/// Room on the left for the 0/25/50/75/100 scale.
constexpr int kAxisWidth = 30;
/// Room at the bottom for the Before / After labels.
constexpr int kAxisHeight = 22;
constexpr int kLegendHeight = 22;
constexpr int kTopPadding = 8;

const qreal kTicks[] = {0.0, 25.0, 50.0, 75.0, 100.0};
} // namespace

ComparisonChart::ComparisonChart(QWidget *parent)
    : QWidget(parent)
{
    m_leftLabel = tr("Before");
    m_rightLabel = tr("After");
    m_baselineLegend = tr("Before");
    m_projectionLegend = tr("After (Estimated)");

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void ComparisonChart::setSeries(const QList<qreal> &baseline, const QList<qreal> &projection)
{
    m_baseline = baseline;
    m_projection = projection;
    update();
}

void ComparisonChart::setAxisLabels(const QString &left, const QString &right)
{
    m_leftLabel = left;
    m_rightLabel = right;
    update();
}

void ComparisonChart::setLegendVisible(bool visible)
{
    m_legendVisible = visible;
    update();
}

void ComparisonChart::setLegendLabels(const QString &baseline, const QString &projection)
{
    m_baselineLegend = baseline;
    m_projectionLegend = projection;
    update();
}

void ComparisonChart::setDividerVisible(bool visible)
{
    m_dividerVisible = visible;
    update();
}

QSize ComparisonChart::sizeHint() const
{
    return QSize(340, 190);
}

QSize ComparisonChart::minimumSizeHint() const
{
    return QSize(220, 150);
}

QRectF ComparisonChart::plotArea() const
{
    const int bottom = kAxisHeight + (m_legendVisible ? kLegendHeight : 0);
    return QRectF(rect()).adjusted(kAxisWidth, kTopPadding, -6, -bottom);
}

void ComparisonChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF plot = plotArea();
    if (plot.width() < 20 || plot.height() < 20)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont scaleFont = Theme::font(11);
    const QFontMetrics scaleMetrics(scaleFont);
    painter.setFont(scaleFont);

    const auto yFor = [&](qreal value) {
        return plot.bottom() - qBound(0.0, value, 100.0) / 100.0 * plot.height();
    };

    // --- Gridlines and scale -------------------------------------------------
    for (qreal tick : kTicks) {
        const qreal y = yFor(tick);

        QPen grid(c.chartGrid);
        grid.setWidthF(1.0);
        if (tick > 0.0)
            grid.setStyle(Qt::DotLine);
        painter.setPen(grid);
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

        painter.setPen(c.textMuted);
        painter.drawText(QRectF(0, y - scaleMetrics.height() / 2.0, kAxisWidth - 8,
                                scaleMetrics.height()),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(qRound(tick)));
    }

    // --- Midpoint divider ----------------------------------------------------
    if (m_dividerVisible) {
        QPen divider(Theme::alpha(c.chartGrid, 200));
        divider.setStyle(Qt::DashLine);
        divider.setWidthF(1.0);
        painter.setPen(divider);
        painter.drawLine(QPointF(plot.center().x(), plot.top()),
                         QPointF(plot.center().x(), plot.bottom()));
    }

    // --- Series --------------------------------------------------------------
    const auto buildPath = [&](const QList<qreal> &values) {
        QPainterPath path;
        if (values.size() < 2)
            return path;
        const qreal step = plot.width() / qreal(values.size() - 1);
        for (int i = 0; i < values.size(); ++i) {
            const QPointF point(plot.left() + step * i, yFor(values.at(i)));
            if (i == 0)
                path.moveTo(point);
            else
                path.lineTo(point);
        }
        return path;
    };

    const auto drawSeries = [&](const QList<qreal> &values, const QColor &color, qreal width,
                                bool markers) {
        const QPainterPath path = buildPath(values);
        if (path.isEmpty())
            return;

        QPen stroke(color);
        stroke.setWidthF(width);
        stroke.setJoinStyle(Qt::RoundJoin);
        stroke.setCapStyle(Qt::RoundCap);
        painter.setPen(stroke);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        if (!markers)
            return;

        const qreal step = plot.width() / qreal(values.size() - 1);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        for (int i = 0; i < values.size(); ++i) {
            const QPointF point(plot.left() + step * i, yFor(values.at(i)));
            painter.drawEllipse(point, 2.8, 2.8);
        }
    };

    drawSeries(m_baseline, c.chartBaseline, 1.6, true);
    drawSeries(m_projection, c.chartProjection, 2.0, true);

    // --- Axis labels ---------------------------------------------------------
    painter.setFont(scaleFont);
    painter.setPen(c.textMuted);
    const qreal labelTop = plot.bottom() + 5;
    painter.drawText(QRectF(plot.left(), labelTop, plot.width() / 2.0, kAxisHeight - 6),
                     Qt::AlignLeft | Qt::AlignTop, m_leftLabel);
    painter.drawText(QRectF(plot.center().x(), labelTop, plot.width() / 2.0, kAxisHeight - 6),
                     Qt::AlignRight | Qt::AlignTop, m_rightLabel);

    // --- Legend --------------------------------------------------------------
    if (!m_legendVisible)
        return;

    const qreal legendY = rect().bottom() - kLegendHeight / 2.0 - 2;
    qreal x = plot.left();

    const auto drawLegendItem = [&](const QColor &color, const QString &label) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(x + 4, legendY), 4, 4);
        x += 14;

        painter.setPen(c.textMuted);
        const int textWidth = scaleMetrics.horizontalAdvance(label);
        painter.drawText(QRectF(x, legendY - scaleMetrics.height() / 2.0, textWidth + 2,
                                scaleMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, label);
        x += textWidth + 20;
    };

    if (!m_baseline.isEmpty())
        drawLegendItem(c.chartBaseline, m_baselineLegend);
    drawLegendItem(c.chartProjection, m_projectionLegend);
}
