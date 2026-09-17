#include "TweakBackupStore.h"

#include <QDateTime>
#include <QSettings>

namespace {
/// Everything lives under this group so it never collides with preferences.
const QLatin1String kRoot("tweakBackups");
} // namespace

TweakBackupStore::TweakBackupStore(QObject *parent)
    : QObject(parent)
{
    m_settings = new QSettings(this);
}

TweakBackupStore::~TweakBackupStore() = default;

TweakBackupStore *TweakBackupStore::instance()
{
    static TweakBackupStore store;
    return &store;
}

QString TweakBackupStore::slotKey(const QString &tweakId, const QString &slot) const
{
    // Slots are registry paths and similar, so "/" has to be escaped or QSettings
    // would turn each segment into a nested group.
    QString safeSlot = slot;
    safeSlot.replace(QLatin1Char('/'), QLatin1Char('|'));
    safeSlot.replace(QLatin1Char('\\'), QLatin1Char('|'));
    return QStringLiteral("%1/%2/slots/%3").arg(kRoot, tweakId, safeSlot);
}

void TweakBackupStore::record(const QString &tweakId, const QString &slot,
                              const QVariant &previous, bool existed)
{
    const QString key = slotKey(tweakId, slot);
    m_settings->setValue(key + QStringLiteral("/value"), previous);
    m_settings->setValue(key + QStringLiteral("/existed"), existed);

    const QString stamp = QStringLiteral("%1/%2/appliedAt").arg(kRoot, tweakId);
    if (!m_settings->contains(stamp)) {
        m_settings->setValue(stamp,
                             QDateTime::currentDateTime().toString(Qt::ISODate));
    }

    m_settings->sync();
    emit changed();
}

bool TweakBackupStore::hasBackup(const QString &tweakId, const QString &slot) const
{
    return m_settings->contains(slotKey(tweakId, slot) + QStringLiteral("/value"));
}

QVariant TweakBackupStore::previousValue(const QString &tweakId, const QString &slot) const
{
    return m_settings->value(slotKey(tweakId, slot) + QStringLiteral("/value"));
}

bool TweakBackupStore::previousValueExisted(const QString &tweakId, const QString &slot) const
{
    return m_settings->value(slotKey(tweakId, slot) + QStringLiteral("/existed"), true).toBool();
}

QStringList TweakBackupStore::appliedTweaks() const
{
    m_settings->beginGroup(kRoot);
    const QStringList ids = m_settings->childGroups();
    m_settings->endGroup();
    return ids;
}

QStringList TweakBackupStore::recordedSlots(const QString &tweakId) const
{
    // "slots" is a Qt keyword macro, so the local cannot be named that.
    m_settings->beginGroup(QStringLiteral("%1/%2/slots").arg(kRoot, tweakId));
    const QStringList recorded = m_settings->childGroups();
    m_settings->endGroup();
    return recorded;
}

bool TweakBackupStore::isRecorded(const QString &tweakId) const
{
    return appliedTweaks().contains(tweakId);
}

void TweakBackupStore::clearTweak(const QString &tweakId)
{
    m_settings->beginGroup(QStringLiteral("%1/%2").arg(kRoot, tweakId));
    m_settings->remove(QString());
    m_settings->endGroup();
    m_settings->sync();
    emit changed();
}

void TweakBackupStore::clearAll()
{
    m_settings->beginGroup(kRoot);
    m_settings->remove(QString());
    m_settings->endGroup();
    m_settings->sync();
    emit changed();
}

QString TweakBackupStore::appliedAt(const QString &tweakId) const
{
    return m_settings->value(QStringLiteral("%1/%2/appliedAt").arg(kRoot, tweakId)).toString();
}
