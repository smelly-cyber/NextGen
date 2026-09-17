// IconButton.h - A flat, square, icon-only button with animated hover states.
//
// Used for the custom window controls (minimize / maximize / restore / close)
// and for the password visibility toggle inside ModernLineEdit.
#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QString>

class QPropertyAnimation;

class IconButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)

public:
    explicit IconButton(const QString &iconName, QWidget *parent = nullptr);
    ~IconButton() override;

    /// Swaps the rendered glyph (e.g. eye <-> eye_off).
    void setIconName(const QString &iconName);
    QString iconName() const { return m_iconName; }

    void setIconSize(int size);
    void setCornerRadius(int radius);

    /// Idle / hover glyph colours.
    void setColors(const QColor &normal, const QColor &hover);
    /// Background wash painted underneath the glyph while hovered.
    void setHoverBackground(const QColor &color);

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);
    qreal pressProgress() const { return m_pressProgress; }
    void setPressProgress(qreal value);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void animate(QPropertyAnimation *animation, qreal to, int duration);

    QString m_iconName;
    int m_iconPixelSize = 18;
    int m_cornerRadius = 8;

    QColor m_normalColor;
    QColor m_hoverColor;
    QColor m_hoverBackground;

    qreal m_hoverProgress = 0.0;
    qreal m_pressProgress = 0.0;

    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_pressAnimation = nullptr;
};
