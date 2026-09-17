// AdminPage.h - The owner-only admin console.
//
// Headline stats, remote client management, push updates, licence management
// with a live table, backend service status, a recent-activity feed and a
// security tools column. Every panel is wired to the server through
// AdminClient; nothing here is a mock.
//
// The page is only added to the workspace for the owner account (see
// MainWindow::setOwner), so it needs no gate of its own.
#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

class AdminClient;
class ModernComboBox;
class ModernLineEdit;
class QJsonArray;
class QJsonObject;
class QLabel;
class QPlainTextEdit;
class QTimer;
class QVBoxLayout;

class StatCard;
class LicenseTableRow;

class AdminPage : public QWidget
{
    Q_OBJECT

public:
    explicit AdminPage(QWidget *parent = nullptr);

    /// Reloads every panel from the server. Called whenever the page is shown.
    void refresh();

    // Called by the table rows.
    void showRow(const QJsonObject &data);
    void revokeRow(const QString &keyId);

signals:
    /// Status strip text for this page.
    void statusChanged(const QString &text, const QColor &color);
    /// "Application Settings" was chosen in Security & Tools.
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QWidget *buildHeader();
    QWidget *buildStatsRow();
    QWidget *buildRemoteManagement();
    QWidget *buildPushUpdates();
    QWidget *buildLicenseManagement();
    QWidget *buildSystemStatus();
    QWidget *buildUserActivity();
    QWidget *buildSecurityTools();

    void loadOverview();
    void loadLicenses();
    void applyLicenseRows(const QJsonArray &licenses);
    void addLicense();
    /// Enables "Revoke Selected" only when rows are ticked.
    void updateSelectionState();
    void runTool(int which);
    void sendCommand(const QString &kind, const QString &title, const QString &body,
                     bool destructive);
    void toast(const QString &text, bool ok = true);

    AdminClient *m_admin = nullptr;

    // Stats.
    StatCard *m_totalUsers = nullptr;
    StatCard *m_activeLicenses = nullptr;
    StatCard *m_onlineNow = nullptr;
    StatCard *m_appVersion = nullptr;

    // Push updates.
    QLabel *m_updateVersionLabel = nullptr;
    ModernLineEdit *m_updateVersion = nullptr;
    ModernLineEdit *m_updateUrl = nullptr;
    QPlainTextEdit *m_updateNotes = nullptr;
    ModernComboBox *m_updateTarget = nullptr;

    // Licence management.
    ModernLineEdit *m_licenseSearch = nullptr;
    ModernComboBox *m_statusFilter = nullptr;
    QVBoxLayout *m_licenseRows = nullptr;
    QLabel *m_licenseCount = nullptr;
    QWidget *m_pager = nullptr;
    class ModernButton *m_revokeSelected = nullptr;
    class ModernCheckBox *m_selectAll = nullptr;
    int m_page = 1;
    int m_totalLicenses = 0;
    QList<LicenseTableRow *> m_rows;

    // System status + activity.
    class AdminActionRow *m_nukeRow = nullptr;
    bool m_nukeActive = false;
    void applyNukeState(bool active);

    QVBoxLayout *m_serviceRows = nullptr;
    QLabel *m_allOperational = nullptr;
    QVBoxLayout *m_activityRows = nullptr;

    QTimer *m_clock = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QTimer *m_searchDebounce = nullptr;
    QLabel *m_dateTime = nullptr;
};
