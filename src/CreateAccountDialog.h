// CreateAccountDialog.h - Registration form shown from the sign-in screen.
//
// Collects the details a new account needs. No licence is entered here: a fresh
// account is created without one and must be activated from inside the app. The
// dialog does no networking itself - on accept it hands the values back and
// LoginWindow drives the LicenseClient, so the busy / error states live in one
// place.
#pragma once

#include "FramelessDialog.h"

#include <QString>

class ModernButton;
class ModernLineEdit;
class QLabel;

class CreateAccountDialog : public FramelessDialog
{
    Q_OBJECT

public:
    explicit CreateAccountDialog(QWidget *parent = nullptr);

    QString username() const;
    QString email() const;
    QString password() const;

    void setBusy(bool busy);
    void showError(const QString &message);

signals:
    /// Emitted when the form passes local validation and wants to be submitted.
    void submitted();

private:
    bool validate();

    ModernLineEdit *m_username = nullptr;
    ModernLineEdit *m_email = nullptr;
    ModernLineEdit *m_password = nullptr;
    ModernLineEdit *m_confirm = nullptr;
    QLabel *m_status = nullptr;
    ModernButton *m_create = nullptr;
    ModernButton *m_cancel = nullptr;
    bool m_busy = false;
};
