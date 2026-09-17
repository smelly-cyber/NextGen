// MetricCard.h - Hardware tile on the Optimise page.
//
// Icon + name along the top, a donut gauge with the current load, two lines of
// detail and a history sparkline along the bottom.
#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class DonutGauge;
class Sparkline;

class MetricCard : public QWidget
{
    Q_OBJECT

public:
    MetricCard(const QString &iconName, const QString &title, QWidget *parent = nullptr);

    /// Percentage 0..100, or negative when the value cannot be measured.
    void setValue(qreal percent);
    void setDetail(const QString &primary, const QString &secondary);
    /// Replaces the gauge's centre label (used for non-percentage readings).
    void setGaugeText(const QString &text);
    void setHistory(const QList<qreal> &history);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QString m_iconName;
    QString m_title;
    QString m_detailPrimary;
    QString m_detailSecondary;

    DonutGauge *m_gauge = nullptr;
    Sparkline *m_sparkline = nullptr;
};
