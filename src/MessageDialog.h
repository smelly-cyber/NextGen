// MessageDialog.h - Themed replacement for QMessageBox / QInputDialog.
//
// A frameless, brand-styled popup for confirmations, information and simple text
// input, so the app never falls back to the native Windows message box. Built on
// FramelessDialog, so it shares the custom header and window controls.
#pragma once

#include "FramelessDialog.h"
#include "ModernButton.h"

#include <QList>
#include <QString>

class ModernLineEdit;
class QLabel;

class MessageDialog : public FramelessDialog
{
    Q_OBJECT

public:
    /// Visual accent for the leading glyph.
    enum class Tone { Info, Question, Warning, Success, Danger };

    MessageDialog(QWidget *parent, const QString &title);

    void setTone(Tone tone);
    void setHeading(const QString &heading);
    void setBody(const QString &richText);

    /// Adds a single-line text field (turns the dialog into an input prompt).
    void enableInput(const QString &placeholder, const QString &initial);
    QString inputText() const;

    /// Adds a button; clicking it ends the dialog returning \a resultCode.
    void addButton(const QString &text, ModernButton::Variant variant, int resultCode,
                   bool isDefault = false);

    /// Shows modally and returns the clicked button's result code (-1 if closed).
    int run();

    // --- Convenience helpers ------------------------------------------------

    static void information(QWidget *parent, const QString &title, const QString &heading,
                            const QString &body, Tone tone = Tone::Info);

    /// Two-button confirm. Returns true when the confirm button is chosen.
    static bool confirm(QWidget *parent, const QString &title, const QString &heading,
                        const QString &body, const QString &confirmText, Tone tone,
                        ModernButton::Variant confirmVariant);

    /// Single-field prompt. Sets \a ok and returns the trimmed text.
    static QString getText(QWidget *parent, const QString &title, const QString &heading,
                           const QString &placeholder, const QString &initial, bool *ok);

protected:
    void onCloseRequested() override;

private:
    QLabel *m_icon = nullptr;
    QLabel *m_heading = nullptr;
    QLabel *m_body = nullptr;
    ModernLineEdit *m_input = nullptr;
    class QHBoxLayout *m_buttonRow = nullptr;
    int m_result = -1;
};
