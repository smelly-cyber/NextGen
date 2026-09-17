// SectionCard.h - Titled panel used to group content on a section page.
//
// Draws the glass card background, an optional icon, a title and a subtitle,
// and exposes a layout for whatever goes underneath.
#pragma once

#include <QString>
#include <QWidget>

class QVBoxLayout;

class SectionCard : public QWidget
{
    Q_OBJECT

public:
    explicit SectionCard(const QString &title, const QString &subtitle = QString(),
                         const QString &iconName = QString(), QWidget *parent = nullptr);

    /// Add your content to this layout.
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);
    /// Tints the title in the accent colour (used by "Optimisation Options").
    void setTitleAccented(bool accented);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QString m_subtitle;
    QString m_iconName;
    bool m_titleAccented = false;
    QVBoxLayout *m_contentLayout = nullptr;
};
