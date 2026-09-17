#include "OptimisePage.h"

#include "ComparisonChart.h"
#include "IconProvider.h"
#include "MetricCard.h"
#include "ModernButton.h"
#include "OptionRow.h"
#include "PageHeader.h"
#include "ScoreBox.h"
#include "SectionCard.h"
#include "SystemMetricsService.h"
#include "TweakCatalogue.h"
#include "TweakEngine.h"
#include "Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

/// Declarative tweak list; `weight` is how much of the remaining headroom the
/// tweak is modelled to recover.
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
    {TweakId::CleanTempFiles, "trash", QT_TRANSLATE_NOOP("OptimisePage", "Clean Temporary Files"),
     QT_TRANSLATE_NOOP("OptimisePage", "Remove temp files, cache, and unnecessary system junk."),
     OptionRow::Emphasis::Recommended, true, 1.2},
    {TweakId::StartupPrograms, "rocket", QT_TRANSLATE_NOOP("OptimisePage", "Optimise Startup Programs"),
     QT_TRANSLATE_NOOP("OptimisePage", "Reduce startup items for faster boot times."),
     OptionRow::Emphasis::Recommended, true, 1.0},
    {TweakId::OptimiseDrives, "disk", QT_TRANSLATE_NOOP("OptimisePage", "Defragment & Optimise Drives"),
     QT_TRANSLATE_NOOP("OptimisePage", "Improve drive performance and file access speed."),
     OptionRow::Emphasis::Recommended, true, 0.9},
    {TweakId::ClearBrowserCache, "globe",
     QT_TRANSLATE_NOOP("OptimisePage", "Reduce Search & Indexing Load"),
     QT_TRANSLATE_NOOP("OptimisePage", "Stop search reaching out to the cloud and indexing what it finds."),
     OptionRow::Emphasis::Optional, false, 0.7},
    {TweakId::DisableServices, "settings", QT_TRANSLATE_NOOP("OptimisePage", "Disable Unnecessary Services"),
     QT_TRANSLATE_NOOP("OptimisePage", "Stop non-essential services to free up resources."),
     OptionRow::Emphasis::Optional, false, 0.8},
    {TweakId::MemoryTrim, "memory", QT_TRANSLATE_NOOP("OptimisePage", "Memory Optimisation"),
     QT_TRANSLATE_NOOP("OptimisePage", "Free up RAM and optimise memory usage."),
     OptionRow::Emphasis::Recommended, true, 1.1},
    {TweakId::WebSearch, "sparkles",
     QT_TRANSLATE_NOOP("OptimisePage", "Declutter Start Menu & Taskbar"),
     QT_TRANSLATE_NOOP("OptimisePage", "Remove web results, widgets and silently installed suggested apps."),
     OptionRow::Emphasis::Recommended, true, 0.9},
};

/// Fraction of the remaining headroom one unit of weight is modelled to recover.
constexpr qreal kHeadroomPerWeight = 0.09;

} // namespace

OptimisePage::OptimisePage(QWidget *parent)
    : SectionPage(parent)
{
    buildUi();
    setStatus(tr("Ready to Optimise"), Theme::colors().link);
}

void OptimisePage::buildUi()
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    // --- Header row ----------------------------------------------------------
    auto *header = new PageHeader(tr("Optimise"),
                                  tr("Analyse and optimise your system for peak performance."),
                                  this);

    m_runButton = new ModernButton(tr("Start Full Optimisation"), ModernButton::Variant::Primary,
                                   this);
    m_runButton->setIconName(QStringLiteral("rocket"));
    m_runButton->setTrailingIconName(QStringLiteral("chevron_right"));
    m_runButton->setFont(Theme::font(16, QFont::DemiBold));
    m_runButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_runButton->setMinimumWidth(330);

    connect(m_runButton, &ModernButton::clicked, this, [this] {
        emit fullRunRequested();
        runSelectedTweaks(tr("Run Full Optimisation"),
                          tr("These are the exact changes Nextgen Tweaks will make to "
                             "Windows. Every setting is recorded first so it can be "
                             "put back."));
    });

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(24);
    headerRow->addWidget(header, 1, Qt::AlignVCenter);
    headerRow->addWidget(m_runButton, 0, Qt::AlignVCenter);

    // --- Hardware tiles ------------------------------------------------------
    m_cpuCard = new MetricCard(QStringLiteral("cpu"), tr("CPU Usage"), this);
    m_memoryCard = new MetricCard(QStringLiteral("memory"), tr("RAM Usage"), this);
    m_storageCard = new MetricCard(QStringLiteral("disk"), tr("Disk Usage"), this);
    m_gpuCard = new MetricCard(QStringLiteral("gpu"), tr("GPU Usage"), this);

    auto *metricsRow = new QHBoxLayout;
    metricsRow->setContentsMargins(0, 0, 0, 0);
    metricsRow->setSpacing(14);
    for (MetricCard *card : {m_cpuCard, m_memoryCard, m_storageCard, m_gpuCard})
        metricsRow->addWidget(card, 1);

    // --- Tweak list ----------------------------------------------------------
    auto *optionsCard = new SectionCard(tr("Optimisation Options"),
                                        tr("Select the tweaks to apply during optimisation."),
                                        QString(), this);
    optionsCard->setTitleAccented(true);

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
    auto *previewCard = new SectionCard(tr("Performance Preview"),
                                        tr("Real-time comparison of system performance."),
                                        QStringLiteral("pulse"), this);
    previewCard->setTitleAccented(true);

    auto *chartCaption = new QLabel(tr("System Performance Score"), previewCard);
    chartCaption->setFont(Theme::font(14, QFont::Medium));
    chartCaption->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    m_chart = new ComparisonChart(previewCard);
    m_chart->setAxisLabels(tr("Before"), tr("After"));

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

    previewCard->contentLayout()->addWidget(chartCaption);
    previewCard->contentLayout()->addSpacing(6);
    previewCard->contentLayout()->addWidget(m_chart, 1);
    previewCard->contentLayout()->addSpacing(12);
    previewCard->contentLayout()->addLayout(scoreRow);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(14);
    bottomRow->addWidget(optionsCard, 57);
    bottomRow->addWidget(previewCard, 43);

    // --- Page ----------------------------------------------------------------
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(m.sectionPadding, m.titleBarHeight - 8, m.sectionPadding,
                              m.sectionPadding - 8);
    outer->setSpacing(0);
    outer->addLayout(headerRow);
    outer->addSpacing(18);
    outer->addLayout(metricsRow);
    outer->addSpacing(16);
    outer->addLayout(bottomRow, 1);

    // --- Live data -----------------------------------------------------------
    connect(SystemMetricsService::instance(), &SystemMetricsService::sampled, this,
            &OptimisePage::applySample);
}

void OptimisePage::pageShown()
{
    SectionPage::pageShown();
    SystemMetricsService::instance()->start();
    applySample(SystemMetricsService::instance()->latest());
}

void OptimisePage::pageHidden()
{
    // The poller is shared, so it is left running for whichever page is next.
}

void OptimisePage::applySample(const MetricSample &sample)
{
    SystemMetricsService *service = SystemMetricsService::instance();
    const HardwareInfo &hardware = service->hardware();

    m_cpuCard->setValue(sample.cpuPercent);
    m_cpuCard->setDetail(hardware.cpuName, hardware.cpuClock);
    m_cpuCard->setHistory(service->cpuHistory());

    m_memoryCard->setValue(sample.memoryPercent);
    m_memoryCard->setDetail(
        sample.memoryTotalBytes > 0
            ? tr("%1 / %2").arg(SystemMetricsService::formatBytes(sample.memoryUsedBytes),
                                SystemMetricsService::formatBytes(sample.memoryTotalBytes))
            : tr("Unavailable"),
        tr("Used"));
    m_memoryCard->setHistory(service->memoryHistory());

    m_storageCard->setValue(sample.storagePercent);
    m_storageCard->setDetail(
        sample.storageTotalBytes > 0
            ? tr("%1 / %2").arg(SystemMetricsService::formatBytes(sample.storageUsedBytes),
                                SystemMetricsService::formatBytes(sample.storageTotalBytes))
            : tr("Unavailable"),
        tr("Used"));
    m_storageCard->setHistory(service->storageHistory());

    // GPU load is read live from the Windows GPU-engine performance counters
    // (the same source Task Manager uses); it primes for one poll before a
    // real number is available.
    m_gpuCard->setValue(sample.gpuPercent);
    m_gpuCard->setDetail(hardware.gpuName,
                         sample.gpuPercent >= 0.0 ? tr("3D engine load") : tr("Measuring…"));
    m_gpuCard->setHistory(service->gpuHistory());

    refreshProjection();
}

void OptimisePage::refreshProjection()
{
    const qreal current = SystemMetricsService::instance()->performanceScore();

    if (current < 0.0) {
        m_beforeScore->setScore(-1.0);
        m_afterScore->setScore(-1.0);
        m_chart->setSeries({}, {});
        return;
    }

    qreal weight = 0.0;
    for (const OptionRow *row : m_options) {
        if (row->isOptionEnabled())
            weight += row->weight();
    }

    // Model: each unit of weight recovers a slice of the remaining headroom, so
    // a already-healthy machine is never promised a huge gain.
    const qreal headroom = 100.0 - current;
    const qreal projected = qBound(0.0, current + headroom * qMin(0.85, weight * kHeadroomPerWeight),
                                   100.0);

    m_beforeScore->setScore(current);
    m_afterScore->setScore(projected);

    // Baseline: the recent measured score history, flattened to the left half.
    QList<qreal> baseline;
    QList<qreal> projection;
    constexpr int kPoints = 9;
    for (int i = 0; i < kPoints; ++i) {
        const qreal t = qreal(i) / (kPoints - 1);
        baseline.append(current);
        projection.append(current + (projected - current) * t * t);
    }

    m_chart->setSeries(baseline, projection);
}

void OptimisePage::runStateChanged(bool running)
{
    m_runButton->setBusy(running, tr("Optimising..."));
    m_runButton->setEnabled(!running);
    for (OptionRow *row : m_options)
        row->setEnabled(!running);
}
