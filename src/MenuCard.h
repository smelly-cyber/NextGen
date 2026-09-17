// MenuCard.h - One tile of the home screen grid.
//
// A dark glass card carrying an accent bar along its top edge, a rounded icon
// tile, a title, a two-line description, a row of capability tags and a circular
// chevron. A large, very faint glyph is drawn into the right-hand side as
// watermark artwork. Cards are checkable so exactly one can be the current
// section; hover, press and selection are all animated.
#pragma once

#include <QAbstractButton>
#include <QString>
#include <QStringList>

class QPropertyAnimation;

/// The destinations the main menu can navigate to.
enum class MenuSection {
    Optimize,
    Enhance,
    Perform,
    SystemInfo,
    Settings,
    Admin
};

class MenuCard : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)
    Q_PROPERTY(qreal selectProgress READ selectProgress WRITE setSelectProgress)

public:
    MenuCard(MenuSection section, const QString &iconName, const QString &title,
             const QString &subtitle, QWidget *parent = nullptr);
    ~MenuCard() override;

    MenuSection section() const { return m_section; }
    QString subtitle() const { return m_subtitle; }
    void setSubtitle(const QString &subtitle);

    /// Dims the card and shows a padlock (the features are licence-gated).
    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

    /// Short capability chips shown under the description, e.g. CLEAN / BOOST.
    /// Passing an empty list gives the shorter, chip-less layout.
    void setTags(const QStringList &tags);

    /// Large faint glyph drawn into the right of the card as artwork.
    void setWatermarkIcon(const QString &iconName);

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);
    qreal pressProgress() const { return m_pressProgress; }
    void setPressProgress(qreal value);
    qreal selectProgress() const { return m_selectProgress; }
    void setSelectProgress(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void animateTo(QPropertyAnimation *animation, qreal current, qreal target, int duration);

    MenuSection m_section;
    QString m_iconName;
    QString m_subtitle;
    QStringList m_tags;
    QString m_watermarkIcon;

    qreal m_hoverProgress = 0.0;
    qreal m_pressProgress = 0.0;
    qreal m_selectProgress = 0.0;
    bool m_locked = false;

    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_pressAnimation = nullptr;
    QPropertyAnimation *m_selectAnimation = nullptr;
};
