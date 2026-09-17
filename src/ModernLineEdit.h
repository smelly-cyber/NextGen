// ModernLineEdit.h - Rounded, dark, glowing text input.
//
// The widget paints its own background, border, focus glow and leading icon;
// a frameless QLineEdit is embedded for the actual text editing so that all of
// Qt's input handling (selection, IME, undo, validators) keeps working.
#pragma once

#include <QColor>
#include <QLineEdit>
#include <QString>
#include <QWidget>

class IconButton;
class QPropertyAnimation;
class QSequentialAnimationGroup;

class ModernLineEdit : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)
    Q_PROPERTY(qreal focusProgress READ focusProgress WRITE setFocusProgress)
    Q_PROPERTY(qreal errorProgress READ errorProgress WRITE setErrorProgress)

public:
    explicit ModernLineEdit(QWidget *parent = nullptr);
    ~ModernLineEdit() override;

    /// Name of the SVG glyph drawn on the left hand side of the field.
    void setLeadingIcon(const QString &iconName);

    /// Shorter field with a smaller font, for dense panels like the admin
    /// console. Off by default, so every existing screen is untouched.
    void setCompact(bool compact);

    void setPlaceholderText(const QString &text);
    QString placeholderText() const;

    QString text() const;
    void setText(const QString &text);

    void setEchoMode(QLineEdit::EchoMode mode);
    QLineEdit::EchoMode echoMode() const;

    /// Adds the eye / eye-off button that toggles password visibility.
    void setPasswordToggleEnabled(bool enabled);
    bool isPasswordToggleEnabled() const { return m_passwordToggle != nullptr; }
    bool isPasswordVisible() const { return m_passwordVisible; }

    /// Red border + glow used for validation feedback.
    void setErrorState(bool error);
    bool hasErrorState() const { return m_errorState; }

    /// Short horizontal shake, played when the field fails validation.
    void shake();

    /// Direct access for advanced configuration (validators, max length, ...).
    QLineEdit *lineEdit() const { return m_lineEdit; }

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);
    qreal focusProgress() const { return m_focusProgress; }
    void setFocusProgress(qreal value);
    qreal errorProgress() const { return m_errorProgress; }
    void setErrorProgress(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void textChanged(const QString &text);
    void returnPressed();
    void passwordVisibilityChanged(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void togglePasswordVisibility();
    void updateHoverState();
    void animateTo(QPropertyAnimation *animation, qreal current, qreal target, int duration);
    void applyInnerPalette();

    /// Keeps the text's left inset in step with the icon and compact state.
    void updateTextInset();

    bool m_compact = false;
    QLineEdit *m_lineEdit = nullptr;
    IconButton *m_passwordToggle = nullptr;

    QString m_leadingIcon;
    bool m_passwordVisible = false;
    bool m_errorState = false;

    qreal m_hoverProgress = 0.0;
    qreal m_focusProgress = 0.0;
    qreal m_errorProgress = 0.0;

    QPropertyAnimation *m_hoverAnimation = nullptr;
    QPropertyAnimation *m_focusAnimation = nullptr;
    QPropertyAnimation *m_errorAnimation = nullptr;
    QSequentialAnimationGroup *m_shakeAnimation = nullptr;
};
