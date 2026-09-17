// TweakBackupStore.h - Undo record for every change the app makes.
//
// Before a tweak writes anything it hands the previous value to this store.
// Nothing is ever changed without a recorded way back, and "Revert all" walks
// the store to put the machine back exactly as it was found.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

class QSettings;

class TweakBackupStore : public QObject
{
    Q_OBJECT

public:
    explicit TweakBackupStore(QObject *parent = nullptr);
    ~TweakBackupStore() override;

    static TweakBackupStore *instance();

    /// Records the value \a slot of \a tweakId had before we touched it.
    /// \a existed is false when the value was absent (revert deletes it again).
    void record(const QString &tweakId, const QString &slot, const QVariant &previous,
                bool existed);

    bool hasBackup(const QString &tweakId, const QString &slot) const;
    QVariant previousValue(const QString &tweakId, const QString &slot) const;
    /// False when the value did not exist before the tweak was applied.
    bool previousValueExisted(const QString &tweakId, const QString &slot) const;

    /// Tweak ids that currently have undo data (i.e. are applied by us).
    QStringList appliedTweaks() const;
    /// Slot names recorded for one tweak.
    QStringList recordedSlots(const QString &tweakId) const;
    bool isRecorded(const QString &tweakId) const;

    /// Drops the undo data for one tweak (called after a successful revert).
    void clearTweak(const QString &tweakId);
    void clearAll();

    /// Human-readable timestamp of when the tweak was applied.
    QString appliedAt(const QString &tweakId) const;

signals:
    void changed();

private:
    QString slotKey(const QString &tweakId, const QString &slot) const;

    QSettings *m_settings = nullptr;
};
