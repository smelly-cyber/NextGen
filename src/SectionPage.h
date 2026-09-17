// SectionPage.h - Shared base for the pages hosted inside MainWindow.
//
// Provides the panel background, the status text the window's bottom strip
// shows, and the one path through which a page is allowed to change the
// machine: bind option rows to catalogue tweaks, then call runSelectedTweaks(),
// which always goes through the confirmation dialog and TweakEngine.
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

class OptionRow;

class SectionPage : public QWidget
{
    Q_OBJECT

public:
    explicit SectionPage(QWidget *parent = nullptr);

    /// Text and colour for the window status strip while this page is current.
    QString statusText() const { return m_statusText; }
    QColor statusColor() const { return m_statusColor; }

    /// Called when the page becomes / stops being the visible one.
    virtual void pageShown();
    virtual void pageHidden() {}

signals:
    void statusChanged(const QString &text, const QColor &color);
    /// The user asked to restart the app with administrator rights.
    void elevationRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

    void setStatus(const QString &text, const QColor &color);

    /// Associates an option row with a tweak in the catalogue.
    void bindOption(OptionRow *row, const QString &tweakId);

    /// Tweak ids for every bound row that is currently switched on.
    QStringList selectedTweakIds() const;

    /// Reads the machine and sets each bound row to match reality.
    void syncOptionsFromMachine();

    /// Greys out and switches off rows whose tweak cannot run on this PC.
    void applyAvailability();

    /// Confirmation dialog, then a real run on the engine. Returns immediately.
    void runSelectedTweaks(const QString &actionTitle, const QString &subtitle);

    /// Reverts everything this app has applied and recorded.
    void revertAllTweaks();

    bool isRunning() const { return m_running; }

    /// Hook for subclasses to move their button into / out of the busy state.
    virtual void runStateChanged(bool running) { Q_UNUSED(running) }

private:
    void connectEngine();
    void handleBusyChanged(bool busy);

    QHash<OptionRow *, QString> m_boundOptions;
    QList<OptionRow *> m_optionOrder;

    QString m_statusText;
    QColor m_statusColor;
    QString m_idleStatus;
    QColor m_idleColor;

    bool m_running = false;
    /// Failure lines gathered during the current run, shown in the result dialog.
    QStringList m_runErrors;
    /// The human title of the action being run, for the result dialog.
    QString m_runActionTitle;
    /// True while a Revert is running (so the result wording differs).
    bool m_runIsRevert = false;
    /// True when the run included a tweak that needs a restart to take effect.
    bool m_runNeedsReboot = false;
    /// True while the async applied-state check is running.
    bool m_syncing = false;
    /// Set by the page that started a run, so only it shows the result dialog
    /// (every page listens to the shared engine). Cleared when the result shows.
    bool m_awaitingResult = false;

    /// Shows the post-run result (success tick / error logs).
    void showResultDialog(bool allSucceeded);
    /// After the result, if a reboot is needed, offer Reboot Now / Reboot Later.
    void maybePromptReboot();
    static void rebootNow();
    bool m_engineConnected = false;
};
