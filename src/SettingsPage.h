// SettingsPage.h - "Settings" section.
//
// General / Appearance / Notifications on the left, account and about panels on
// the right. Every control is bound to SettingsStore, so changes persist.
#pragma once

#include "SectionPage.h"

#include <QHash>
#include <QString>

class AvatarBadge;
class ModernButton;
class ColorSwatchButton;
class ModernComboBox;
class OptionRow;
class QLabel;

class SettingsPage : public SectionPage
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setUserName(const QString &name);
    /// Shows the current licence entitlement under the account name.
    void setEntitlement(const QString &text);

signals:
    void signOutRequested();
    void editProfileRequested();

private slots:
    void reloadFromStore();
    void confirmReset();
    void checkForUpdates();
    void showChangelog();
    void editProfile();
    void offerRestart(const QString &reason);

private:
    void buildUi();
    /// Creates a preference row bound to \a key.
    OptionRow *addSwitch(class SectionCard *card, const char *key, const QString &iconName,
                         const QString &title, const QString &description);

    QHash<QString, OptionRow *> m_switches;

    ModernComboBox *m_theme = nullptr;
    ColorSwatchButton *m_accent = nullptr;

    AvatarBadge *m_avatar = nullptr;
    QLabel *m_userName = nullptr;
    QLabel *m_accountType = nullptr;
    ModernButton *m_editProfile = nullptr;
    ModernButton *m_signOut = nullptr;
    ModernButton *m_checkUpdates = nullptr;
    ModernButton *m_changelog = nullptr;
    ModernButton *m_reset = nullptr;
    ModernButton *m_revertSystem = nullptr;
};
