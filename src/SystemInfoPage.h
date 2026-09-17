// SystemInfoPage.h - "System Info" section.
//
// Summary tiles across the top, then hardware and software detail panels, with
// actions to refresh the report or export it to a text file.
#pragma once

#include "SectionPage.h"

#include <QHash>
#include <QString>

class DetailRow;
class ModernButton;
class SummaryCard;

class SystemInfoPage : public SectionPage
{
    Q_OBJECT

public:
    explicit SystemInfoPage(QWidget *parent = nullptr);

    void pageShown() override;

private slots:
    void refresh();
    void exportReport();

private:
    void buildUi();
    DetailRow *addDetail(class SectionCard *card, const QString &label, const QString &iconName);

    SummaryCard *m_cpuSummary = nullptr;
    SummaryCard *m_gpuSummary = nullptr;
    SummaryCard *m_memorySummary = nullptr;
    SummaryCard *m_storageSummary = nullptr;

    DetailRow *m_processor = nullptr;
    DetailRow *m_motherboard = nullptr;
    DetailRow *m_memory = nullptr;
    DetailRow *m_graphics = nullptr;
    DetailRow *m_storage = nullptr;
    DetailRow *m_network = nullptr;

    DetailRow *m_operatingSystem = nullptr;
    DetailRow *m_osBuild = nullptr;
    DetailRow *m_systemType = nullptr;
    DetailRow *m_bios = nullptr;
    DetailRow *m_directX = nullptr;
    DetailRow *m_appVersion = nullptr;
    // Live state of the settings Nextgen Tweaks cares about, so the card shows
    // at a glance what the optimisations have (or have not) changed.
    DetailRow *m_computerName = nullptr;
    DetailRow *m_display = nullptr;
    DetailRow *m_powerPlan = nullptr;
    DetailRow *m_gpuScheduling = nullptr;
    DetailRow *m_gameMode = nullptr;
    DetailRow *m_secureBoot = nullptr;
    DetailRow *m_uptime = nullptr;

    ModernButton *m_exportButton = nullptr;
    ModernButton *m_refreshButton = nullptr;
};
