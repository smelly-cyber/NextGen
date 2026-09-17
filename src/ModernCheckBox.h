// ModernCheckBox.h - Custom painted check box replacing the native Qt indicator.
//
// It derives from QCheckBox so that keyboard handling, the checked state and
// accessibility keep working; only the painting is taken over.
#pragma once

#include <QCheckBox>

class QPropertyAnimation;

class ModernCheckBox : public QCheckBox
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal checkProgress READ checkProgress WRITE setCheckProgress)

public:
    explicit ModernCheckBox(const QString &text, QWidget *parent = nullptr);
    ~ModernCheckBox() override;

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);
    qreal checkProgress() const { return m_checkProgress; }
    void setCheckProgress(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void animateTo(QPropertyAnimation *animation, qreal current, qreal target, int duration);

    qreal m_hoverProgress = 0.0;
    qreal m_checkProgress = 0.0;

    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_checkAnimation = nullptr;
};
