// TweakEngine.h - Owns the tweak catalogue and runs jobs off the UI thread.
//
// Applying tweaks shells out to powercfg / sc / defrag and walks temp folders,
// so the work happens on a worker thread and reports progress back by signal.
// The UI never blocks and never writes to the machine itself.
#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

class QThread;
class Tweak;

/// One tweak plus what the engine knows about it right now.
struct TweakStatus
{
    QString id;
    QString title;
    QString description;
    bool requiresAdmin = false;
    bool requiresReboot = false;
    bool applied = false;   ///< Currently in effect on this machine.
    bool recorded = false;  ///< We applied it and hold undo data.
};

class TweakEngine : public QObject
{
    Q_OBJECT

public:
    explicit TweakEngine(QObject *parent = nullptr);
    ~TweakEngine() override;

    static TweakEngine *instance();

    /// Every tweak in the catalogue.
    QVector<Tweak *> tweaks() const { return m_tweaks; }
    Tweak *tweak(const QString &id) const;

    /// Fresh status for one tweak (reads the machine).
    TweakStatus status(const QString &id) const;

    /// Plain English preview of what applying \a ids would change.
    QStringList previewFor(const QStringList &ids) const;
    /// True when any of \a ids needs an elevated token.
    bool requiresAdmin(const QStringList &ids) const;
    /// True when any of \a ids only takes effect after a restart.
    bool requiresReboot(const QStringList &ids) const;

    bool isBusy() const { return m_busy; }

    /// Ids we have applied and still hold undo data for.
    QStringList revertableIds() const;

public slots:
    /// Applies \a ids in order. Creates a System Restore point first when any
    /// machine-level change is involved.
    void applyTweaks(const QStringList &ids);
    /// Puts back everything recorded for \a ids.
    void revertTweaks(const QStringList &ids);
    /// Cancels after the current step finishes.
    void cancel();

signals:
    void busyChanged(bool busy);
    void progress(int percent, const QString &step);
    /// One tweak finished. \a ok false means it was skipped with \a message.
    void tweakFinished(const QString &id, bool ok, const QString &message);
    /// The whole run finished. \a summary is shown to the user.
    void finished(bool allSucceeded, const QString &summary);

private:
    void runJob(const QStringList &ids, bool reverting);

    QVector<Tweak *> m_tweaks;
    QThread *m_thread = nullptr;
    bool m_busy = false;
    bool m_cancelRequested = false;
};
