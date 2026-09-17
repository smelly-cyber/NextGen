#include "PerformPage.h"

#include "IconProvider.h"
#include "LiveMonitorChart.h"
#include "MetricCard.h"
#include "ModernButton.h"
#include "OptionRow.h"
#include "NetworkAdapterInfo.h"
#include "PageHeader.h"
#include "ScoreBox.h"
#include "SectionCard.h"
#include "StarRating.h"
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
    OptionRow::Emphasis emphasis;
    bool onByDefault;
    qreal weight;
};

// Seven toggles, six of them groups, rather than twelve single settings. Each one still applies
// everything it used to - see CompositeTweak in the catalogue - and the
// confirmation dialog lists every individual value before anything is written.
// Grouping them is what lets the page fit the window without being clipped.
const TweakSpec kTweaks[] = {
    {TweakId::HighPerformanceMode, "bolt",
     QT_TRANSLATE_NOOP("PerformPage", "Maximum Performance Profile"),
     OptionRow::Emphasis::Recommended, true, 1.4},
    {TweakId::CpuPriority, "cpu", QT_TRANSLATE_NOOP("PerformPage", "CPU & GPU Scheduling"),
     OptionRow::Emphasis::Recommended, true, 1.3},
    {TweakId::GamePriority, "rocket", QT_TRANSLATE_NOOP("PerformPage", "Gaming Optimisation"),
     OptionRow::Emphasis::Recommended, true, 1.2},
    {TweakId::BackgroundApps, "window",
     QT_TRANSLATE_NOOP("PerformPage", "Limit Background Activity"),
     OptionRow::Emphasis::Recommended, true, 1.1},
    {TweakId::NetworkThrottling, "globe",
     QT_TRANSLATE_NOOP("PerformPage", "Network Latency Tuning"),
     OptionRow::Emphasis::Recommended, true, 1.0},
    {TweakId::DisableTelemetry, "shield_check",
     QT_TRANSLATE_NOOP("PerformPage", "Disable Telemetry & Tracking"),
     OptionRow::Emphasis::Recommended, true, 0.9},
    // Greyed out automatically on PCs without an NVIDIA card.
    {TweakId::NvidiaProfile, "gpu", QT_TRANSLATE_NOOP("PerformPage", "NVIDIA Profile Import"),
     OptionRow::Emphasis::Optional, false, 0.8},
};

constexpr qreal kHeadroomPerWeight = 0.08;

} // namespace

PerformPage::PerformPage(QWidget *parent)
    : SectionPage(parent)
{
    buildUi();
    setStatus(tr("Monitoring"), Theme::colors().link);
}

void PerformPage::buildUi()
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    // --- Header row ----------------------------------------------------------
    auto *header = new PageHeader(tr("Perform"),
                                  tr("Boost performance and monitor key system metrics."), this);

    m_runButton = new ModernButton(tr("Start Performance Boost"), ModernButton::Variant::Primary,
                                   this);
    m_runButton->setIconName(QStringLiteral("chart"));
    m_runButton->setFont(Theme::font(16, QFont::DemiBold));
    m_runButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_runButton->setMinimumWidth(310);

    connect(m_runButton, &ModernButton::clicked, this, [this] {
        emit fullRunRequested();
        runSelectedTweaks(tr("Start Performance Boost"),
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
    m_cpuCard = new MetricCard(QStringLiteral("cpu"), tr("CPU"), this);
    m_gpuCard = new MetricCard(QStringLiteral("gpu"), tr("GPU"), this);
    m_memoryCard = new MetricCard(QStringLiteral("memory"), tr("RAM"), this);
    m_networkCard = new MetricCard(QStringLiteral("wifi"), tr("Network"), this);

    auto *metricsRow = new QHBoxLayout;
    metricsRow->setContentsMargins(0, 0, 0, 0);
    metricsRow->setSpacing(14);
    for (MetricCard *card : {m_cpuCard, m_gpuCard, m_memoryCard, m_networkCard})
        metricsRow->addWidget(card, 1);

    // --- Tweak list ----------------------------------------------------------
    auto *tweaksCard = new SectionCard(tr("Performance Tweaks"), QString(),
                                       QStringLiteral("settings"), this);

    for (const TweakSpec &spec : kTweaks) {
        auto *row = new OptionRow(QString::fromLatin1(spec.iconName), tr(spec.title), QString(),
                                  spec.emphasis, spec.onByDefault, tweaksCard);
        row->setWeight(spec.weight);
        bindOption(row, QString::fromLatin1(spec.tweakId));
        row->setFramed(true);
        row->setBadgeInline(true);
        m_options.append(row);
        tweaksCard->contentLayout()->addWidget(row);

        connect(row, &OptionRow::optionToggled, this, [this] { refreshProjection(); });
    }
    tweaksCard->contentLayout()->addStretch(1);

    // --- Live monitor --------------------------------------------------------
    auto *monitorCard = new SectionCard(tr("Live Performance Monitor"), QString(),
                                        QStringLiteral("pulse"), this);

    m_monitor = new LiveMonitorChart(monitorCard);
    m_cpuSeries = m_monitor->addSeries(tr("CPU"), c.primary);
    m_gpuSeries = m_monitor->addSeries(tr("GPU"), c.cyan);
    m_memorySeries = m_monitor->addSeries(tr("RAM"), QColor(0xA8, 0x6C, 0xF0));
    m_monitor->setTimeSpan(SystemMetricsService::historyLength(),
                           SystemMetricsService::sampleIntervalSeconds());
    monitorCard->contentLayout()->addWidget(m_monitor, 1);

    // --- Score panels --------------------------------------------------------
    auto *scoreCard = new SectionCard(tr("Current Performance Score:"), QString(), QString(),
                                      this);

    m_scoreValue = new QLabel(QStringLiteral("—"), scoreCard);
    m_scoreValue->setFont(Theme::font(38, QFont::Bold));
    m_scoreValue->setStyleSheet(QStringLiteral("color: %1;").arg(c.primaryBright.name()));

    auto *outOf = new QLabel(tr("/100"), scoreCard);
    outOf->setFont(Theme::font(14));
    outOf->setStyleSheet(QStringLiteral("color: %1;").arg(c.textMuted.name()));

    auto *scoreValueRow = new QHBoxLayout;
    scoreValueRow->setContentsMargins(0, 0, 0, 0);
    scoreValueRow->setSpacing(6);
    scoreValueRow->addWidget(m_scoreValue, 0, Qt::AlignBottom);
    scoreValueRow->addWidget(outOf, 0, Qt::AlignBottom);
    scoreValueRow->addStretch(1);

    m_stars = new StarRating(scoreCard);

    scoreCard->contentLayout()->addLayout(scoreValueRow);
    scoreCard->contentLayout()->addSpacing(8);
    scoreCard->contentLayout()->addWidget(m_stars, 0, Qt::AlignLeft);
    scoreCard->contentLayout()->addStretch(1);

    auto *improvementCard = new SectionCard(tr("Estimated Improvement"), QString(), QString(),
                                            this);

    m_beforeScore = new ScoreBox(tr("Before"), ScoreBox::Tone::Measured, improvementCard);
    m_afterScore = new ScoreBox(tr("After"), ScoreBox::Tone::Projected, improvementCard);

    auto *arrow = new QLabel(improvementCard);
    arrow->setPixmap(IconProvider::pixmap(QStringLiteral("arrow_right"), 22, c.primaryBright,
                                          devicePixelRatioF()));
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setFixedWidth(34);

    auto *improvementRow = new QHBoxLayout;
    improvementRow->setContentsMargins(0, 0, 0, 0);
    improvementRow->setSpacing(6);
    improvementRow->addWidget(m_beforeScore, 1);
    improvementRow->addWidget(arrow, 0, Qt::AlignVCenter);
    improvementRow->addWidget(m_afterScore, 1);

    improvementCard->contentLayout()->addLayout(improvementRow);
    improvementCard->contentLayout()->addStretch(1);

    auto *scoreRow = new QHBoxLayout;
    scoreRow->setContentsMargins(0, 0, 0, 0);
    scoreRow->setSpacing(14);
    scoreRow->addWidget(scoreCard, 40);
    scoreRow->addWidget(improvementCard, 60);

    auto *rightColumn = new QVBoxLayout;
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->setSpacing(14);
    rightColumn->addWidget(monitorCard, 1);
    rightColumn->addLayout(scoreRow);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(14);
    bottomRow->addWidget(tweaksCard, 45);
    bottomRow->addLayout(rightColumn, 55);

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

    connect(SystemMetricsService::instance(), &SystemMetricsService::sampled, this,
            &PerformPage::applySample);
}

void PerformPage::pageShown()
{
    SectionPage::pageShown();
    SystemMetricsService::instance()->start();
    applySample(SystemMetricsService::instance()->latest());
}

void PerformPage::applySample(const MetricSample &sample)
{
    SystemMetricsService *service = SystemMetricsService::instance();
    const HardwareInfo &hardware = service->hardware();

    m_cpuCard->setValue(sample.cpuPercent);
    m_cpuCard->setDetail(tr("CPU"), hardware.cpuName);
    m_cpuCard->setHistory(service->cpuHistory());

    m_gpuCard->setValue(sample.gpuPercent);
    m_gpuCard->setDetail(tr("GPU"), hardware.gpuName);
    m_gpuCard->setHistory(service->gpuHistory());

    m_memoryCard->setValue(sample.memoryPercent);
    m_memoryCard->setDetail(sample.memoryPercent >= 0.0
                                ? tr("%1%").arg(qRound(sample.memoryPercent))
                                : tr("Unavailable"),
                            tr("Used"));
    m_memoryCard->setHistory(service->memoryHistory());

    // Throughput is not a percentage, so the ring shows the reading as text and
    // fills relative to a 100 Mbps reference.
    m_networkCard->setValue(sample.networkDownMbps < 0.0
                                ? -1.0
                                : qBound(0.0, sample.networkDownMbps, 100.0));
    m_networkCard->setGaugeText(sample.networkDownMbps < 0.0
                                    ? QString()
                                    : tr("%1 Mbps").arg(sample.networkDownMbps, 0, 'f',
                                                        sample.networkDownMbps < 10.0 ? 1 : 0));
    // Show what we are actually connected through, which is also what the
    // "Optimise Network Adapter" tweak will tune. Detection is cached: walking
    // the interface table on every 1.5s poll would be wasteful.
    if (m_connectionSummary.isEmpty())
        m_connectionSummary = NetworkAdapterInfo::activeAdapter().summary();

    m_networkCard->setDetail(sample.networkDownMbps < 0.0
                                 ? tr("Unavailable")
                                 : tr("%1 Mbps").arg(sample.networkDownMbps, 0, 'f',
                                                     sample.networkDownMbps < 10.0 ? 1 : 0),
                             m_connectionSummary);
    m_networkCard->setHistory(service->networkHistory());

    m_monitor->setSeriesValues(m_cpuSeries, service->cpuHistory());
    m_monitor->setSeriesValues(m_gpuSeries, service->gpuHistory());
    m_monitor->setSeriesValues(m_memorySeries, service->memoryHistory());

    refreshProjection();
}

void PerformPage::refreshProjection()
{
    const qreal current = SystemMetricsService::instance()->performanceScore();

    if (current < 0.0) {
        m_scoreValue->setText(QStringLiteral("—"));
        m_stars->setScore(-1.0);
        m_beforeScore->setScore(-1.0);
        m_afterScore->setScore(-1.0);
        return;
    }

    qreal weight = 0.0;
    for (const OptionRow *row : m_options) {
        if (row->isOptionEnabled())
            weight += row->weight();
    }

    const qreal headroom = 100.0 - current;
    const qreal projected =
        qBound(0.0, current + headroom * qMin(0.85, weight * kHeadroomPerWeight), 100.0);

    m_scoreValue->setText(QString::number(qRound(current)));
    m_stars->setScore(current);
    m_beforeScore->setScore(current);
    m_afterScore->setScore(projected);
}

void PerformPage::runStateChanged(bool running)
{
    m_runButton->setBusy(running, tr("Boosting..."));
    m_runButton->setEnabled(!running);
    for (OptionRow *row : m_options)
        row->setEnabled(!running);
}
