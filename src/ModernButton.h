// ModernButton.h - Primary (gradient) and secondary (outlined) push button.
//
// Both variants share the same painting code; only the brushes differ. The
// button can also enter a "busy" state which replaces the icon with a spinner
// without ever blocking the event loop.
#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QString>

class QPropertyAnimation;
class QVariantAnimation;

class ModernButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)
    Q_PROPERTY(qreal busyProgress READ busyProgress WRITE setBusyProgress)

public:
    enum class Variant {
        Primary,   ///< Bright blue gradient fill with a soft glow.
        Secondary, ///< Transparent fill with a thin blue outline.
        Danger     ///< Outlined in red, for destructive actions.
    };
    Q_ENUM(Variant)

    explicit ModernButton(const QString &text, Variant variant = Variant::Primary,
                          QWidget *parent = nullptr);
    ~ModernButton() override;

    void setVariant(Variant variant);
    Variant variant() const { return m_variant; }

    /// Name of the SVG glyph drawn to the left of the label.
    void setIconName(const QString &iconName);
    QString iconName() const { return m_iconName; }
    void setIconPixelSize(int size);

    /// Optional glyph pinned to the right hand edge (e.g. a chevron).
    void setTrailingIconName(const QString &iconName);
    QString trailingIconName() const { return m_trailingIconName; }

    void setCornerRadius(int radius);

    /// Shows a spinner and the busy label; the button is not clickable meanwhile.
    void setBusy(bool busy, const QString &busyText = QString());
    bool isBusy() const { return m_busy; }

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);
    qreal pressProgress() const { return m_pressProgress; }
    void setPressProgress(qreal value);
    qreal busyProgress() const { return m_busyProgress; }
    void setBusyProgress(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void paintPrimary(QPainter &painter, const QRectF &body, qreal radius);
    void paintSecondary(QPainter &painter, const QRectF &body, qreal radius);
    void paintContent(QPainter &painter, const QRectF &body);
    void paintSpinner(QPainter &painter, const QRectF &body);
    void animateTo(QPropertyAnimation *animation, qreal current, qreal target, int duration);

    Variant m_variant = Variant::Primary;
    QString m_iconName;
    QString m_trailingIconName;
    QString m_busyText;
    QString m_restingText;
    int m_iconPixelSize = 20;
    int m_cornerRadius = 12;
    bool m_busy = false;

    qreal m_hoverProgress = 0.0;
    qreal m_pressProgress = 0.0;
    qreal m_busyProgress = 0.0;
    qreal m_spinnerAngle = 0.0;

    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_pressAnimation = nullptr;
    QPropertyAnimation *m_busyAnimation = nullptr;
    QVariantAnimation *m_spinnerAnimation = nullptr;
};
