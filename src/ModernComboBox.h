// ModernComboBox.h - Dark, rounded dropdown matching the input styling.
//
// Derives from QComboBox so the model, keyboard handling and popup all keep
// working; only the closed-state painting is taken over.
#pragma once

#include <QComboBox>
#include <QString>

class QPropertyAnimation;

class ModernComboBox : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit ModernComboBox(QWidget *parent = nullptr);
    ~ModernComboBox() override;

    /// Glyph drawn before the current text (e.g. a moon for the dark theme).
    void setLeadingIcon(const QString &iconName);
    /// Colour swatch drawn before the current text (for the accent picker).
    void setSwatchColor(const QColor &color);

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_leadingIcon;
    QColor m_swatch;
    qreal m_hoverProgress = 0.0;
    QPropertyAnimation *m_hoverAnimation = nullptr;
};
