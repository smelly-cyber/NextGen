// LicenseActivationDialog.h - In-app "enter your licence" prompt.
//
// A frameless, non-movable modal gate shown after an unlicensed sign-in. It uses
// the shared custom header (no Windows title bar). It cannot be dragged and it
// stays put until a valid key is verified, at which point the caller accept()s
// it. Its header close button (and Alt+F4) quit the whole application, because
// the features are unusable without a licence. LicenseClient does the actual
// redemption, so the busy/error handling lives with the caller.
#pragma once

#include "FramelessDialog.h"

#include <QString>

class ModernButton;
class ModernLineEdit;
class QLabel;

class LicenseActivationDialog : public FramelessDialog
{
    Q_OBJECT

public:
    explicit LicenseActivationDialog(QWidget *parent = nullptr);

    QString licenseKey() const;

    void setBusy(bool busy);
    void showError(const QString &message);

signals:
    /// The user submitted a well-formed key to activate.
    void submitted();

    /// The user chose to sign out and use a different account.
    void signOutRequested();

protected:
    void onCloseRequested() override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    bool validate();

    ModernLineEdit *m_license = nullptr;
    QLabel *m_status = nullptr;
    ModernButton *m_signOut = nullptr;
    ModernButton *m_activate = nullptr;
};
