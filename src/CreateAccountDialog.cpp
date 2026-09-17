#include "CreateAccountDialog.h"

#include "ModernButton.h"
#include "ModernLineEdit.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QVBoxLayout>

CreateAccountDialog::CreateAccountDialog(QWidget *parent)
    : FramelessDialog(parent)
{
    const Theme::Palette &c = Theme::colors();

    setHeaderTitle(tr("Create Account"));
    setMinimumWidth(520);

    auto *heading = new QLabel(tr("Create your account"), this);
    heading->setFont(Theme::font(26, QFont::Bold));
    heading->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    auto *subtitle = new QLabel(
        tr("Create your account, then activate it with a licence inside the app."), this);
    subtitle->setFont(Theme::font(14));
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    m_username = new ModernLineEdit(this);
    m_username->setLeadingIcon(QStringLiteral("user"));
    m_username->setPlaceholderText(tr("Username"));
    m_username->lineEdit()->setMaxLength(32);

    m_email = new ModernLineEdit(this);
    m_email->setLeadingIcon(QStringLiteral("user"));
    m_email->setPlaceholderText(tr("Email  (optional)"));
    m_email->lineEdit()->setMaxLength(254);

    m_password = new ModernLineEdit(this);
    m_password->setLeadingIcon(QStringLiteral("lock"));
    m_password->setPlaceholderText(tr("Password"));
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPasswordToggleEnabled(true);

    m_confirm = new ModernLineEdit(this);
    m_confirm->setLeadingIcon(QStringLiteral("lock"));
    m_confirm->setPlaceholderText(tr("Confirm password"));
    m_confirm->setEchoMode(QLineEdit::Password);

    m_status = new QLabel(this);
    m_status->setFont(Theme::font(13));
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(20);
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(c.danger.name()));

    m_cancel = new ModernButton(tr("Cancel"), ModernButton::Variant::Secondary, this);
    m_cancel->setFont(Theme::font(14, QFont::Medium));
    m_cancel->setFixedHeight(52);

    m_create = new ModernButton(tr("Create Account"), ModernButton::Variant::Primary, this);
    m_create->setIconName(QStringLiteral("user_plus"));
    m_create->setFont(Theme::font(15, QFont::DemiBold));
    m_create->setFixedHeight(52);

    connect(m_cancel, &ModernButton::clicked, this, &QDialog::reject);
    connect(m_create, &ModernButton::clicked, this, [this] {
        if (validate())
            emit submitted();
    });
    connect(m_confirm, &ModernLineEdit::returnPressed, this, [this] {
        if (validate())
            emit submitted();
    });

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    buttons->addWidget(m_cancel, 0);
    buttons->addStretch(1);
    buttons->addWidget(m_create, 0);

    setContentMargins(28, 22, 28, 24);
    QVBoxLayout *layout = contentLayout();
    layout->addWidget(heading);
    layout->addSpacing(6);
    layout->addWidget(subtitle);
    layout->addSpacing(20);
    layout->addWidget(m_username);
    layout->addSpacing(6);
    layout->addWidget(m_email);
    layout->addSpacing(6);
    layout->addWidget(m_password);
    layout->addSpacing(6);
    layout->addWidget(m_confirm);
    layout->addSpacing(6);
    layout->addWidget(m_status);
    layout->addSpacing(12);
    layout->addLayout(buttons);
}

QString CreateAccountDialog::username() const { return m_username->text().trimmed(); }
QString CreateAccountDialog::email() const { return m_email->text().trimmed(); }
QString CreateAccountDialog::password() const { return m_password->text(); }

void CreateAccountDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_create->setBusy(busy, tr("Creating..."));
    m_create->setEnabled(!busy);
    m_cancel->setEnabled(!busy);
    for (ModernLineEdit *field : {m_username, m_email, m_password, m_confirm})
        field->setEnabled(!busy);
}

void CreateAccountDialog::showError(const QString &message)
{
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::colors().danger.name()));
    m_status->setText(message);
}

bool CreateAccountDialog::validate()
{
    if (username().size() < 3) {
        showError(tr("Choose a username of at least 3 characters."));
        return false;
    }
    static const QRegularExpression emailRe(
        QStringLiteral(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)"));
    if (!email().isEmpty() && !emailRe.match(email()).hasMatch()) {
        showError(tr("That email address does not look right."));
        return false;
    }
    if (password().size() < 6) {
        showError(tr("Choose a password of at least 6 characters."));
        return false;
    }
    if (password() != m_confirm->text()) {
        showError(tr("The passwords do not match."));
        return false;
    }
    m_status->clear();
    return true;
}
