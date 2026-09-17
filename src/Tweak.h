// Tweak.h - One reversible change to the machine.
//
// Every tweak must be able to report what it will change *before* it runs, read
// back whether it is currently applied, and undo itself from the backup store.
// Anything that cannot honour that contract does not belong in the catalogue.
#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

class TweakBackupStore;

class Tweak
{
public:
    /// Where the change lands - Machine implies an elevated token is required.
    enum class Scope {
        CurrentUser, ///< HKCU / per-user state. No administrator needed.
        Machine      ///< HKLM, services, disk tools. Administrator required.
    };

    Tweak(QString id, QString title, QString description, Scope scope);
    virtual ~Tweak();

    QString id() const { return m_id; }
    QString title() const { return m_title; }
    QString description() const { return m_description; }
    Scope scope() const { return m_scope; }
    bool requiresAdmin() const { return m_scope == Scope::Machine; }

    bool requiresReboot() const { return m_requiresReboot; }
    void setRequiresReboot(bool value) { m_requiresReboot = value; }

    /// Whether this tweak can do anything on this machine at all - e.g. an
    /// NVIDIA-only tweak on a PC with an AMD card. Unavailable tweaks are shown
    /// greyed out and switched off rather than hidden, so the list is the same on
    /// every PC.
    virtual bool isAvailable() const { return true; }
    /// Short reason shown when isAvailable() is false.
    virtual QString unavailableReason() const { return QString(); }

    /// Plain English list of exactly what apply() will do, shown to the user in
    /// the confirmation dialog before anything happens.
    virtual QStringList plannedChanges() const = 0;

    /// Reads the machine and reports whether the tweak is currently in effect.
    virtual bool isApplied() const = 0;

    /// Applies the change, recording undo data first. Returns false on failure.
    virtual bool apply(TweakBackupStore &store, QString *error) = 0;

    /// Restores the previous state from the backup store.
    virtual bool revert(TweakBackupStore &store, QString *error) = 0;

private:
    QString m_id;
    QString m_title;
    QString m_description;
    Scope m_scope;
    bool m_requiresReboot = false;
};

// ---------------------------------------------------------- RegistryTweak --

/// One registry value the tweak owns.
struct RegistryEntry
{
    QString path;        ///< Full native key, e.g. "HKEY_CURRENT_USER\\Control Panel\\Desktop".
    QString valueName;   ///< Value inside that key.
    QVariant optimised;  ///< What we write.
    QVariant windowsDefault; ///< Used when the value did not exist before.
};

/// Writes one or more registry values, remembering what was there first.
class RegistryTweak : public Tweak
{
public:
    RegistryTweak(QString id, QString title, QString description, Scope scope,
                  QVector<RegistryEntry> entries);

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

private:
    QVector<RegistryEntry> m_entries;
};

// --------------------------------------------------------- CompositeTweak --

/// One toggle that owns several underlying changes.
///
/// Two reasons this exists rather than one row per change. The obvious one is
/// space: the option lists have to fit a fixed-size window, and a window that
/// grows to fit its content is worse than a list that groups related settings.
/// The other is that several of these changes are meaningless on their own -
/// switching to the High performance power plan while leaving power throttling
/// on, for instance.
///
/// The grouping is presentational only. The confirmation dialog still lists
/// every individual value, each part still records its own undo data, and
/// revert still puts every one of them back.
///
/// Parts must be constructed with the SAME id as the composite, so their backup
/// records land under the id the engine knows about.
class CompositeTweak : public Tweak
{
public:
    CompositeTweak(QString id, QString title, QString description, Scope scope,
                   QVector<Tweak *> parts);
    ~CompositeTweak() override;

    QStringList plannedChanges() const override;
    bool isApplied() const override;
    bool apply(TweakBackupStore &store, QString *error) override;
    bool revert(TweakBackupStore &store, QString *error) override;

private:
    QVector<Tweak *> m_parts;
};
