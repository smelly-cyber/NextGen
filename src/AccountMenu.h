// AccountMenu.h - Small popover shown when the account avatar is clicked.
//
// Instead of signing out on the very first click, the avatar opens this little
// themed popover with a "Sign out" button. It behaves like a menu: clicking
// outside dismisses it.
#pragma once

#include <QWidget>

class ModernButton;

class AccountMenu : public QWidget
{
    Q_OBJECT

public:
    explicit AccountMenu(QWidget *parent = nullptr);

    /// Shows the popover just beneath \a anchor, right-aligned to it.
    void popupUnder(QWidget *anchor);

signals:
    /// The "Sign out" button was pressed.
    void signOutClicked();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    ModernButton *m_signOut = nullptr;
};
