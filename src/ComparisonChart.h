// ComparisonChart.h - "Before / After (estimated)" line chart.
//
// Plots a measured baseline series and a projected series on a 0..100 scale,
// with gridlines, axis labels and an optional legend. The projection is always
// labelled as an estimate: it is a model, not a measurement.
#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class ComparisonChart : public QWidget
{
    Q_OBJECT

public:
    explicit ComparisonChart(QWidget *parent = nullptr);

    /// Both series are 0..100 values sampled left to right. An empty baseline
    /// hides that series (the Enhance page only shows the projection).
    void setSeries(const QList<qreal> &baseline, const QList<qreal> &projection);

    void setAxisLabels(const QString &left, const QString &right);
    void setLegendVisible(bool visible);
    void setLegendLabels(const QString &baseline, const QString &projection);
    /// Draws the dashed divider between the measured and projected halves.
    void setDividerVisible(bool visible);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF plotArea() const;

    QList<qreal> m_baseline;
    QList<qreal> m_projection;
    QString m_leftLabel;
    QString m_rightLabel;
    QString m_baselineLegend;
    QString m_projectionLegend;
    bool m_legendVisible = false;
    bool m_dividerVisible = true;
};
