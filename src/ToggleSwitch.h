// ToggleSwitch.h - Animated on/off switch used by the option lists.
#pragma once

#include <QAbstractButton>

class QPropertyAnimation;

class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal slideProgress READ slideProgress WRITE setSlideProgress)
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    ~ToggleSwitch() override;

    qreal slideProgress() const { return m_slideProgress; }
    void setSlideProgress(qreal value);
    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    qreal m_slideProgress = 0.0;
    qreal m_hoverProgress = 0.0;
    QPropertyAnimation *m_slideAnimation = nullptr;
    QPropertyAnimation *m_hoverAnimation = nullptr;
};
