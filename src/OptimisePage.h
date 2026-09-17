// OptimisePage.h - "Optimise" section.
//
// Live hardware tiles across the top, the tweak list on the left and the
// projected performance comparison on the right.
#pragma once

#include "SectionPage.h"

#include <QList>

class ComparisonChart;
class MetricCard;
class ModernButton;
class OptionRow;
class ScoreBox;
struct MetricSample;

class OptimisePage : public SectionPage
{
    Q_OBJECT

public:
    explicit OptimisePage(QWidget *parent = nullptr);

    void pageShown() override;
    void pageHidden() override;

signals:
    /// The user asked for a full optimisation run.
    void fullRunRequested();

protected:
    void runStateChanged(bool running) override;

private:
    void buildUi();
    void applySample(const MetricSample &sample);
    void refreshProjection();

    MetricCard *m_cpuCard = nullptr;
    MetricCard *m_memoryCard = nullptr;
    MetricCard *m_storageCard = nullptr;
    MetricCard *m_gpuCard = nullptr;

    ModernButton *m_runButton = nullptr;
    QList<OptionRow *> m_options;

    ComparisonChart *m_chart = nullptr;
    ScoreBox *m_beforeScore = nullptr;
    ScoreBox *m_afterScore = nullptr;
};
