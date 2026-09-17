#include "EnhancePage.h"

#include "ComparisonChart.h"
#include "IconProvider.h"
#include "ModernButton.h"
#include "OptionRow.h"
#include "PageHeader.h"
#include "ScoreBox.h"
#include "SectionCard.h"
#include "StatTile.h"
#include "SystemMetricsService.h"
#include "TweakCatalogue.h"
#include "TweakEngine.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

struct TweakSpec
{
    const char *tweakId;
    const char *iconName;
    const char *title;
    const char *description;
    OptionRow::Emphasis emphasis;
    bool onByDefault;
    qreal weight;
};

const TweakSpec kTweaks[] = {
    {TweakId::BoostResponsiveness, "rocket", QT_TRANSLATE_NOOP("EnhancePage", "Boost System Responsiveness"),
     QT_TRANSLATE_NOOP("EnhancePage", "Prioritise interactive work over background tasks."),
     OptionRow::Emphasis::Recommended, true, 1.2},
    {TweakId::VisualEffects, "sparkles", QT_TRANSLATE_NOOP("EnhancePage", "Optimise Visual Effects"),
     QT_TRANSLATE_NOOP("EnhancePage", "Trim animations, shadows and thumbnails that cost frame time."),
     OptionRow::Emphasis::Recommended, true, 1.0},
    {TweakId::AnimationLatency, "clock", QT_TRANSLATE_NOOP("EnhancePage", "Reduce Interface Latency"),
     QT_TRANSLATE_NOOP("EnhancePage", "Cut menu, hover and window transition delays to near zero."),
     OptionRow::Emphasis::Neutral, true, 0.9},
    {TweakId::ForegroundPriority, "window", QT_TRANSLATE_NOOP("EnhancePage", "CPU & Memory Scheduling"),
     QT_TRANSLATE_NOOP("EnhancePage", "Weight CPU time to the active app and keep the kernel in RAM."),
     OptionRow::Emphasis::Recommended, true, 1.0},
    {TweakId::Transparency, "diamond", QT_TRANSLATE_NOOP("EnhancePage", "Disable Transparency & Blur"),
     QT_TRANSLATE_NOOP("EnhancePage", "Turn off acrylic blur and lock screen slideshows to save GPU work."),
     OptionRow::Emphasis::Optional, false, 0.7},
    {TweakId::AppTimeouts, "gauge",
     QT_TRANSLATE_NOOP("EnhancePage", "Faster App & Shutdown Response"),
     QT_TRANSLATE_NOOP("EnhancePage", "Stop Windows waiting on frozen apps and slow services."),
     OptionRow::Emphasis::Neutral, true, 0.9},
    {TweakId::MouseInputLag, "pulse", QT_TRANSLATE_NOOP("EnhancePage", "Reduce Input Lag"),
     QT_TRANSLATE_NOOP("EnhancePage", "1:1 mouse movement and the fastest keyboard repeat rate."),
     OptionRow::Emphasis::Optional, false, 0.8},
};

constexpr qreal kHeadroomPerWeight = 0.085;

} // namespace

EnhancePage::EnhancePage(QWidget *parent)
    : SectionPage(parent)
{
    buildUi();
    setStatus(tr("Ready to Enhance"), Theme::colors().link);
}

void EnhancePage::buildUi()
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    auto *header = new PageHeader(tr("Enhance"),
                                  tr("Improve system responsiveness and efficiency."), this);

    // --- Tiles + action ------------------------------------------------------
    m_responsivenessTile = new StatTile(QStringLiteral("cpu"), tr("Responsiveness"),
                                        StatTile::Mode::Gauge, this);
    m_efficiencyTile = new StatTile(QStringLiteral("gauge"), tr("Efficiency"),
                                    StatTile::Mode::Gauge, this);

    m_bootTile = new StatTile(QStringLiteral("clock"), tr("System Uptime"),
                              StatTile::Mode::Value, this);

    m_processesTile = new StatTile(QStringLiteral("nodes"), tr("Background Processes"),
                                   StatTile::Mode::Value, this);
    m_processesTile->setUnit(QString());

    m_runButton = new ModernButton(tr("Start Full Enhancement"), ModernButton::Variant::Primary,
                                   this);
    m_runButton->setIconName(QStringLiteral("rocket"));
    m_runButton->setFont(Theme::font(16, QFont::DemiBold));
    m_runButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_runButton->setMinimumWidth(300);

    connect(m_runButton, &ModernButton::clicked, this, [this] {
        emit fullRunRequested();
        runSelectedTweaks(tr("Run Full Enhancement"),
                          tr("These are the exact changes Nextgen Tweaks will make to "
                             "Windows. Every setting is recorded first so it can be "
                             "put back."));
    });

    auto *tileRow = new QHBoxLayout;
    tileRow->setContentsMargins(0, 0, 0, 0);
    tileRow->setSpacing(12);
    for (StatTile *tile : {m_responsivenessTile, m_efficiencyTile, m_bootTile, m_processesTile})
        tileRow->addWidget(tile, 1);

    // --- Enhancement list ----------------------------------------------------
    auto *optionsCard = new SectionCard(tr("Enhancement Options"), QString(), QString(), this);

    for (const TweakSpec &spec : kTweaks) {
        auto *row = new OptionRow(QString::fromLatin1(spec.iconName), tr(spec.title),
                                  tr(spec.description), spec.emphasis, spec.onByDefault,
                                  optionsCard);
        row->setWeight(spec.weight);
        m_options.append(row);
        bindOption(row, QString::fromLatin1(spec.tweakId));
        optionsCard->contentLayout()->addWidget(row);

        connect(row, &OptionRow::optionToggled, this, [this] { refreshProjection(); });
    }
    optionsCard->contentLayout()->addStretch(1);

    // --- Projection ----------------------------------------------------------
    auto *previewCard = new SectionCard(tr("Enhancement Preview"),
                                        tr("Real-time before/after performance comparison."),
                                        QString(), this);

    m_chart = new ComparisonChart(previewCard);
    m_chart->setAxisLabels(tr("Before"), tr("After"));
    m_chart->setLegendVisible(true);
    m_chart->setLegendLabels(tr("Before"), tr("After (Estimated)"));
    m_chart->setDividerVisible(false);

    m_beforeScore = new ScoreBox(tr("Before"), ScoreBox::Tone::Measured, previewCard);
    m_afterScore = new ScoreBox(tr("After (Estimated)"), ScoreBox::Tone::Projected, previewCard);

    auto *arrow = new QLabel(previewCard);
    arrow->setPixmap(IconProvider::pixmap(QStringLiteral("arrow_right"), 22, c.primaryBright,
                                          devicePixelRatioF()));
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setFixedWidth(34);

    auto *scoreRow = new QHBoxLayout;
    scoreRow->setContentsMargins(0, 0, 0, 0);
    scoreRow->setSpacing(6);
    scoreRow->addWidget(m_beforeScore, 1);
    scoreRow->addWidget(arrow, 0, Qt::AlignVCenter);
    scoreRow->addWidget(m_afterScore, 1);

    auto *scoreCaption = new QLabel(tr("System Responsiveness Score"), previewCard);
    scoreCaption->setFont(Theme::font(14, QFont::Medium));
    scoreCaption->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    previewCard->contentLayout()->addWidget(m_chart, 1);
    previewCard->contentLayout()->addSpacing(10);
    previewCard->contentLayout()->addWidget(scoreCaption);
    previewCard->contentLayout()->addSpacing(8);
    previewCard->contentLayout()->addLayout(scoreRow);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(14);
    bottomRow->addWidget(optionsCard, 45);
    bottomRow->addWidget(previewCard, 55);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(m.sectionPadding, m.titleBarHeight - 8, m.sectionPadding,
                              m.sectionPadding - 8);
    outer->setSpacing(0);
    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(24);
    headerRow->addWidget(header, 1, Qt::AlignVCenter);
    headerRow->addWidget(m_runButton, 0, Qt::AlignVCenter);
    outer->addLayout(headerRow);
    outer->addSpacing(16);
    outer->addLayout(tileRow);
    outer->addSpacing(16);
    outer->addLayout(bottomRow, 1);

    connect(SystemMetricsService::instance(), &SystemMetricsService::sampled, this,
            &EnhancePage::applySample);
}

void EnhancePage::pageShown()
{
    SectionPage::pageShown();
    SystemMetricsService::instance()->start();
    applySample(SystemMetricsService::instance()->latest());
}

void EnhancePage::applySample(const MetricSample &sample)
{
    // Responsiveness is the CPU headroom; efficiency the memory headroom. Both
    // are derived from real readings rather than invented.
    m_responsivenessTile->setValue(sample.cpuPercent < 0.0 ? -1.0 : 100.0 - sample.cpuPercent);
    m_efficiencyTile->setValue(sample.memoryPercent < 0.0 ? -1.0 : 100.0 - sample.memoryPercent);

    // Uptime is read live from the system tick count; the process count is a
    // live enumeration each poll.
    const QString uptime = SystemMetricsService::instance()->uptimeText();
    m_bootTile->setText(uptime);
    m_bootTile->setValue(uptime.isEmpty() ? -1.0 : 0.0);
    m_processesTile->setValue(sample.processCount > 0 ? qreal(sample.processCount) : -1.0);

    refreshProjection();
}

void EnhancePage::refreshProjection()
{
    const MetricSample &sample = SystemMetricsService::instance()->latest();

    if (sample.cpuPercent < 0.0 && sample.memoryPercent < 0.0) {
        m_beforeScore->setScore(-1.0);
        m_afterScore->setScore(-1.0);
        m_chart->setSeries({}, {});
        return;
    }

    // Responsiveness score leans on CPU headroom, with memory as a modifier.
    qreal current = 0.0;
    qreal weightSum = 0.0;
    if (sample.cpuPercent >= 0.0) {
        current += (100.0 - sample.cpuPercent) * 0.65;
        weightSum += 0.65;
    }
    if (sample.memoryPercent >= 0.0) {
        current += (100.0 - sample.memoryPercent) * 0.35;
        weightSum += 0.35;
    }
    current = weightSum > 0.0 ? qBound(0.0, current / weightSum, 100.0) : -1.0;

    qreal weight = 0.0;
    for (const OptionRow *row : m_options) {
        if (row->isOptionEnabled())
            weight += row->weight();
    }

    const qreal headroom = 100.0 - current;
    const qreal projected =
        qBound(0.0, current + headroom * qMin(0.85, weight * kHeadroomPerWeight), 100.0);

    m_beforeScore->setScore(current);
    m_afterScore->setScore(projected);

    // Single rising curve from the measured value to the projection.
    QList<qreal> projection;
    constexpr int kPoints = 11;
    for (int i = 0; i < kPoints; ++i) {
        const qreal t = qreal(i) / (kPoints - 1);
        projection.append(current + (projected - current) * t * t);
    }
    m_chart->setSeries({}, projection);
}

void EnhancePage::runStateChanged(bool running)
{
    m_runButton->setBusy(running, tr("Enhancing..."));
    m_runButton->setEnabled(!running);
    for (OptionRow *row : m_options)
        row->setEnabled(!running);
}
