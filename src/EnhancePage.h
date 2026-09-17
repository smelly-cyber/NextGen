// EnhancePage.h - "Enhance" section.
//
// Responsiveness tiles across the top, the enhancement list on the left and the
// projected responsiveness comparison on the right.
#pragma once

#include "SectionPage.h"

#include <QList>

class ComparisonChart;
class ModernButton;
class OptionRow;
class ScoreBox;
class StatTile;
struct MetricSample;

class EnhancePage : public SectionPage
{
    Q_OBJECT

public:
    explicit EnhancePage(QWidget *parent = nullptr);

    void pageShown() override;

signals:
    /// The user asked for a full enhancement run.
    void fullRunRequested();

protected:
    void runStateChanged(bool running) override;

private:
    void buildUi();
    void applySample(const MetricSample &sample);
    void refreshProjection();

    StatTile *m_responsivenessTile = nullptr;
    StatTile *m_efficiencyTile = nullptr;
    StatTile *m_bootTile = nullptr;
    StatTile *m_processesTile = nullptr;

    ModernButton *m_runButton = nullptr;
    QList<OptionRow *> m_options;

    ComparisonChart *m_chart = nullptr;
    ScoreBox *m_beforeScore = nullptr;
    ScoreBox *m_afterScore = nullptr;
};
