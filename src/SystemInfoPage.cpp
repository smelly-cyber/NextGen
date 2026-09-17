#include "SystemInfoPage.h"

#include "DetailRow.h"
#include "HardwareSerials.h"
#include "ModernButton.h"
#include "PageHeader.h"
#include "SectionCard.h"
#include "SummaryCard.h"
#include "SystemMetricsService.h"
#include "SystemTweaks.h"
#include "Theme.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QSysInfo>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

SystemInfoPage::SystemInfoPage(QWidget *parent)
    : SectionPage(parent)
{
    buildUi();
    setStatus(tr("Healthy"), Theme::colors().success);
}

DetailRow *SystemInfoPage::addDetail(SectionCard *card, const QString &label,
                                     const QString &iconName)
{
    auto *row = new DetailRow(label, iconName, card);
    card->contentLayout()->addWidget(row);
    return row;
}

void SystemInfoPage::buildUi()
{
    const Theme::Metrics &m = Theme::metrics();

    auto *header = new PageHeader(tr("System Info"),
                                  tr("View detailed information about your hardware and software."),
                                  this);

    // --- Summary tiles -------------------------------------------------------
    m_cpuSummary = new SummaryCard(QStringLiteral("cpu"), tr("CPU"), this);
    m_gpuSummary = new SummaryCard(QStringLiteral("gpu"), tr("GPU"), this);
    m_memorySummary = new SummaryCard(QStringLiteral("memory"), tr("RAM"), this);
    m_storageSummary = new SummaryCard(QStringLiteral("disk"), tr("Storage"), this);

    auto *summaryRow = new QHBoxLayout;
    summaryRow->setContentsMargins(0, 0, 0, 0);
    summaryRow->setSpacing(14);
    for (SummaryCard *card : {m_cpuSummary, m_gpuSummary, m_memorySummary, m_storageSummary})
        summaryRow->addWidget(card, 1);

    // --- Hardware ------------------------------------------------------------
    auto *hardwareCard = new SectionCard(tr("Hardware Details"), QString(),
                                         QStringLiteral("cpu"), this);

    m_processor = addDetail(hardwareCard, tr("Processor:"), QStringLiteral("cpu"));
    m_motherboard = addDetail(hardwareCard, tr("Motherboard:"), QStringLiteral("board"));
    m_memory = addDetail(hardwareCard, tr("Memory:"), QStringLiteral("memory"));
    m_graphics = addDetail(hardwareCard, tr("Graphics:"), QStringLiteral("gpu"));
    m_storage = addDetail(hardwareCard, tr("Storage:"), QStringLiteral("disk"));
    m_network = addDetail(hardwareCard, tr("Network:"), QStringLiteral("wifi"));
    m_network->setSeparatorVisible(false);

    // --- Component serials ---------------------------------------------------
    // Fills the space under the hardware card with the identifiers stamped into
    // the machine itself (firmware + storage driver).
    auto *serialsCard = new SectionCard(tr("Component Serials"), QString(),
                                        QStringLiteral("key"), this);

    const QVector<HardwareSerial> serials = HardwareSerials::collect();
    for (int i = 0; i < serials.size(); ++i) {
        const HardwareSerial &entry = serials.at(i);
        auto *row = new DetailRow(entry.label + QLatin1Char(':'), entry.icon, serialsCard);
        row->setValue(entry.value);
        if (i == serials.size() - 1)
            row->setSeparatorVisible(false);
        serialsCard->contentLayout()->addWidget(row);
    }
    serialsCard->contentLayout()->addStretch(1);

    auto *hardwareColumn = new QVBoxLayout;
    hardwareColumn->setContentsMargins(0, 0, 0, 0);
    hardwareColumn->setSpacing(10);
    hardwareColumn->addWidget(hardwareCard, 0);
    hardwareColumn->addWidget(serialsCard, 1);

    // --- Software ------------------------------------------------------------
    auto *softwareCard = new SectionCard(tr("Software & System"), QString(),
                                         QStringLiteral("monitor"), this);

    m_operatingSystem = addDetail(softwareCard, tr("Operating System:"), QString());
    m_osBuild = addDetail(softwareCard, tr("OS Build:"), QString());
    m_systemType = addDetail(softwareCard, tr("System Type:"), QString());
    m_bios = addDetail(softwareCard, tr("BIOS Version:"), QString());
    m_directX = addDetail(softwareCard, tr("DirectX Version:"), QString());
    m_appVersion = addDetail(softwareCard, tr("Nextgen Tweaks Version:"), QString());
    m_computerName = addDetail(softwareCard, tr("Computer Name:"), QString());
    m_display = addDetail(softwareCard, tr("Primary Display:"), QString());
    m_powerPlan = addDetail(softwareCard, tr("Power Plan:"), QString());
    m_gpuScheduling = addDetail(softwareCard, tr("GPU Scheduling:"), QString());
    m_gameMode = addDetail(softwareCard, tr("Game Mode:"), QString());
    m_secureBoot = addDetail(softwareCard, tr("Secure Boot:"), QString());
    m_uptime = addDetail(softwareCard, tr("System Uptime:"), QString());
    m_uptime->setSeparatorVisible(false);
    softwareCard->contentLayout()->addStretch(1);

    // --- Actions -------------------------------------------------------------
    m_exportButton = new ModernButton(tr("Export Report"), ModernButton::Variant::Secondary,
                                      this);
    m_exportButton->setIconName(QStringLiteral("download"));
    m_exportButton->setFont(Theme::font(14, QFont::Medium));
    m_exportButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_exportButton->setMinimumWidth(200);
    m_exportButton->setFixedHeight(50);

    m_refreshButton = new ModernButton(tr("Refresh"), ModernButton::Variant::Secondary, this);
    m_refreshButton->setIconName(QStringLiteral("refresh"));
    m_refreshButton->setFont(Theme::font(14, QFont::Medium));
    m_refreshButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_refreshButton->setMinimumWidth(180);
    m_refreshButton->setFixedHeight(50);

    connect(m_exportButton, &ModernButton::clicked, this, &SystemInfoPage::exportReport);
    connect(m_refreshButton, &ModernButton::clicked, this, &SystemInfoPage::refresh);

    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(12);
    actionRow->addStretch(1);
    actionRow->addWidget(m_exportButton, 0);
    actionRow->addWidget(m_refreshButton, 0);

    auto *softwareColumn = new QVBoxLayout;
    softwareColumn->setContentsMargins(0, 0, 0, 0);
    softwareColumn->setSpacing(14);
    softwareColumn->addWidget(softwareCard, 1);
    softwareColumn->addLayout(actionRow);

    auto *panelRow = new QHBoxLayout;
    panelRow->setContentsMargins(0, 0, 0, 0);
    panelRow->setSpacing(14);
    panelRow->addLayout(hardwareColumn, 50);
    panelRow->addLayout(softwareColumn, 50);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(m.sectionPadding, m.titleBarHeight - 8, m.sectionPadding,
                              m.sectionPadding - 8);
    outer->setSpacing(0);
    outer->addWidget(header);
    outer->addSpacing(14);
    outer->addLayout(summaryRow);
    outer->addSpacing(12);
    outer->addLayout(panelRow, 1);
}

void SystemInfoPage::pageShown()
{
    SectionPage::pageShown();
    SystemMetricsService::instance()->start();
    refresh();
}

void SystemInfoPage::refresh()
{
    SystemMetricsService *service = SystemMetricsService::instance();
    service->refreshReport();
    const SystemReport &report = service->report();

    m_cpuSummary->setValue(report.cpuSummary);
    m_gpuSummary->setValue(report.gpuSummary);
    m_memorySummary->setValue(report.memorySummary);
    m_storageSummary->setValue(report.storageSummary);

    m_processor->setValue(report.processor);
    m_motherboard->setValue(report.motherboard);
    m_memory->setValue(report.memory);
    m_graphics->setValue(report.graphics);
    m_storage->setValue(report.storage);
    m_network->setValue(report.network);

    m_operatingSystem->setValue(report.operatingSystem);
    m_osBuild->setValue(report.osBuild);
    m_systemType->setValue(report.systemType);
    m_bios->setValue(report.biosVersion);
    m_directX->setValue(report.directXVersion);
    m_appVersion->setValue(report.applicationVersion);

    m_computerName->setValue(QSysInfo::machineHostName());

    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QSize pixels = screen->size() * screen->devicePixelRatio();
        m_display->setValue(tr("%1 x %2  @  %3 Hz")
                                .arg(pixels.width())
                                .arg(pixels.height())
                                .arg(qRound(screen->refreshRate())));
    }

    const QString scheme = PowerPlanTweak::activeSchemeGuid();
    m_powerPlan->setValue(scheme.isEmpty() ? tr("Unknown") : PowerPlanTweak::schemeName(scheme));

    const QSettings graphics(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers"),
        QSettings::NativeFormat);
    m_gpuScheduling->setValue(graphics.value(QStringLiteral("HwSchMode")).toInt() == 2
                                  ? tr("Hardware-accelerated")
                                  : tr("Standard"));

    // Game Mode is on unless it has been explicitly switched off.
    const QSettings gameBar(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\GameBar"),
                            QSettings::NativeFormat);
    m_gameMode->setValue(gameBar.value(QStringLiteral("AutoGameModeEnabled"), 1).toInt() == 1
                             ? tr("On")
                             : tr("Off"));

    const QSettings secureBoot(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State"),
        QSettings::NativeFormat);
    const QVariant secureBootState = secureBoot.value(QStringLiteral("UEFISecureBootEnabled"));
    m_secureBoot->setValue(!secureBootState.isValid()     ? tr("Not supported")
                           : secureBootState.toInt() == 1 ? tr("Enabled")
                                                          : tr("Disabled"));

    m_uptime->setValue(SystemMetricsService::instance()->uptimeText());

    setStatus(tr("Healthy"), Theme::colors().success);
}

void SystemInfoPage::exportReport()
{
    const QString suggested =
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .filePath(QStringLiteral("nextgen-tweaks-system-report-%1.txt")
                          .arg(QDateTime::currentDateTime().toString(
                              QStringLiteral("yyyyMMdd-HHmmss"))));

    const QString path = QFileDialog::getSaveFileName(this, tr("Export System Report"), suggested,
                                                      tr("Text files (*.txt)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus(tr("Could not write the report"), Theme::colors().danger);
        return;
    }

    QTextStream stream(&file);
    stream << SystemMetricsService::instance()->report().toPlainText();
    stream << QStringLiteral("\nGenerated %1\n")
                  .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    file.close();

    setStatus(tr("Report exported"), Theme::colors().success);
}
