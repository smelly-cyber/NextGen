// MainMenuPanel.h - Right hand side of the main window.
//
// Header ("Nextgen Tweaks" / "Welcome back, <user>"), the account avatar and
// the vertical list of section cards. Like LoginPanel it owns no business
// logic: picking a card simply emits sectionActivated().
#pragma once

#include "MenuCard.h"

#include <QList>
#include <QString>
#include <QWidget>

class AccountMenu;
class QButtonGroup;
class QLabel;
class UserCard;

class MainMenuPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MainMenuPanel(QWidget *parent = nullptr);

    /// Name shown after "Welcome back,".
    void setUserName(const QString &name);
    QString userName() const { return m_userName; }

    MenuSection currentSection() const { return m_currentSection; }

public slots:
    /// Marks \a section as the current one without emitting a navigation
    /// request (use it when routing is driven from elsewhere).
    void setCurrentSection(MenuSection section);

    /// Unchecks every card so no section is highlighted on the menu itself.
    void clearSelection();

    /// Locks the section cards behind a licence (dims them; a click asks to
    /// activate instead of navigating).
    void setLocked(bool locked);

signals:
    /// The user picked a section and wants to go there.
    void sectionActivated(MenuSection section);
    /// A locked card was clicked - prompt the user to activate a licence.
    void activateRequested();
    /// The avatar was clicked.
    void accountRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildUi();
    MenuCard *cardFor(MenuSection section) const;

    QLabel *m_eyebrow = nullptr;
    QLabel *m_heading = nullptr;
    QLabel *m_tagline = nullptr;
    QWidget *m_lockNote = nullptr;
    UserCard *m_userCard = nullptr;
    AccountMenu *m_accountMenu = nullptr;
    QButtonGroup *m_cardGroup = nullptr;
    QList<MenuCard *> m_cards;

    QString m_userName;
    bool m_locked = false;
    MenuSection m_currentSection = MenuSection::Optimize;
};
