// NavList.h - The sidebar navigation shown down the left of the workspace.
//
// Home plus one entry per section. The active entry is drawn as a filled pill
// with an accent bar, so the rail always shows where you are. Only the main
// window turns this on; the sign-in window keeps a plain branding panel.
#pragma once

#include "MenuCard.h" // MenuSection

#include <QAbstractButton>
#include <QList>
#include <QWidget>

class QVariantAnimation;

/// One row in the rail: icon + label, with hover and active states.
class NavItem : public QAbstractButton
{
public:
    NavItem(const QString &iconName, const QString &label, QWidget *parent = nullptr);

    void setActive(bool active);
    bool isActive() const { return m_active; }

    /// Dims the row and shows a padlock instead of its icon.
    void setLocked(bool locked);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateHover(qreal to);

    QString m_iconName;
    bool m_active = false;
    bool m_locked = false;
    qreal m_hover = 0.0;
    qreal m_activeProgress = 0.0;
    QVariantAnimation *m_hoverAnimation = nullptr;
    QVariantAnimation *m_activeAnimation = nullptr;
};

class NavList : public QWidget
{
    Q_OBJECT

public:
    explicit NavList(QWidget *parent = nullptr);

    /// Highlights the row for \a section.
    void setActiveSection(MenuSection section);
    /// Highlights the Home row (no section open).
    void setHomeActive();

    /// Locks the section rows behind a licence (Home stays available).
    void setLocked(bool locked);

    /// Shows the owner-only Admin row. Hidden (and absent) for everyone else.
    void setAdminVisible(bool visible);

signals:
    void homeRequested();
    void sectionRequested(MenuSection section);
    /// A locked row was clicked - the window should offer activation.
    void activateRequested();

private:
    void clearActive();

    NavItem *m_home = nullptr;
    NavItem *m_admin = nullptr;
    QList<QPair<MenuSection, NavItem *>> m_sections;
    bool m_locked = false;
};
