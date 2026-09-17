// OptionRow.h - One tweak in an option list: icon, title, description, an
// optional badge and a toggle switch.
//
// Shared by the Optimise and Enhance pages.
#pragma once

#include "Badge.h"

#include <QString>
#include <QWidget>

class ToggleSwitch;

class OptionRow : public QWidget
{
    Q_OBJECT

public:
    /// How prominently the tweak is suggested.
    enum class Emphasis {
        Recommended, ///< Blue dot + "Recommended".
        Optional,    ///< Outlined "Optional" pill.
        Neutral      ///< No badge.
    };

    OptionRow(const QString &iconName, const QString &title, const QString &description,
              Emphasis emphasis, bool enabledByDefault, QWidget *parent = nullptr);

    bool isOptionEnabled() const;
    void setOptionEnabled(bool enabled);

    /// Relative contribution of this tweak to the estimated improvement.
    void setWeight(qreal weight) { m_weight = weight; }
    qreal weight() const { return m_weight; }

    QString title() const { return m_title; }

    /// Draws each row inside its own rounded box (the Perform page style).
    void setFramed(bool framed);
    /// Places the badge directly after the title rather than right aligned.
    void setBadgeInline(bool inlineBadge);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void optionToggled(bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateHoverState();

    QString m_iconName;
    QString m_title;
    QString m_description;
    Badge *m_badge = nullptr;
    ToggleSwitch *m_toggle = nullptr;
    qreal m_weight = 1.0;
    bool m_hovered = false;
    bool m_framed = false;
    bool m_badgeInline = false;
};
