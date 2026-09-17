// PerformPage.h - "Perform" section.
//
// Live hardware tiles, the performance tweak list and a rolling multi-series
// monitor with the current score and projected improvement.
#pragma once

#include "SectionPage.h"

#include <QList>

class LiveMonitorChart;
class MetricCard;
class ModernButton;
class OptionRow;
class ScoreBox;
class StarRating;
class QLabel;
struct MetricSample;

class PerformPage : public SectionPage
{
    Q_OBJECT

public:
    explicit PerformPage(QWidget *parent = nullptr);

    void pageShown() override;

signals:
    /// The user asked for a full performance boost.
    void fullRunRequested();

protected:
    void runStateChanged(bool running) override;

private:
    void buildUi();
    void applySample(const MetricSample &sample);
    void refreshProjection();

    MetricCard *m_cpuCard = nullptr;
    MetricCard *m_gpuCard = nullptr;
    MetricCard *m_memoryCard = nullptr;
    MetricCard *m_networkCard = nullptr;

    /// Cached "Ethernet - 2.5 Gbps" style readout of the detected connection.
    QString m_connectionSummary;

    ModernButton *m_runButton = nullptr;
    QList<OptionRow *> m_options;

    LiveMonitorChart *m_monitor = nullptr;
    int m_cpuSeries = -1;
    int m_gpuSeries = -1;
    int m_memorySeries = -1;

    QLabel *m_scoreValue = nullptr;
    StarRating *m_stars = nullptr;
    ScoreBox *m_beforeScore = nullptr;
    ScoreBox *m_afterScore = nullptr;
};
