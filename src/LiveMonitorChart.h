// LiveMonitorChart.h - Rolling multi-series area chart (the live monitor).
//
// Plots several 0..100 series against a seconds-ago time axis, each with its
// own colour, a soft gradient fill and a legend across the top right.
#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QVector>
#include <QWidget>

class LiveMonitorChart : public QWidget
{
    Q_OBJECT

public:
    explicit LiveMonitorChart(QWidget *parent = nullptr);

    /// Registers a series and returns its index.
    int addSeries(const QString &name, const QColor &color);

    /// Replaces the values of one series (oldest first, percentages).
    void setSeriesValues(int index, const QList<qreal> &values);

    /// Number of x-axis slots and how many seconds each one covers.
    void setTimeSpan(int slotCount, qreal secondsPerSlot);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Series
    {
        QString name;
        QColor color;
        QList<qreal> values;
    };

    QRectF plotArea() const;
    void paintLegend(QPainter &painter);

    QVector<Series> m_series;
    int m_slots = 40;
    qreal m_secondsPerSlot = 1.5;
};
