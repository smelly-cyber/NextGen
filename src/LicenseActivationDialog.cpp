#include "LicenseActivationDialog.h"

#include "IconProvider.h"
#include "LicenseClient.h"
#include "ModernButton.h"
#include "ModernLineEdit.h"
#include "Theme.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QShowEvent>
#include <QVBoxLayout>

LicenseActivationDialog::LicenseActivationDialog(QWidget *parent)
    : FramelessDialog(parent)
{
    const Theme::Palette &c = Theme::colors();

    setHeaderTitle(tr("Activate Licence"));
    setMovable(false); // The gate is stationary until a licence is entered.
    setFixedWidth(560);
    setContentMargins(30, 22, 30, 26);

    auto *icon = new QLabel;
    icon->setPixmap(IconProvider::pixmap(QStringLiteral("key"), 32, c.primaryBright,
                                         devicePixelRatioF()));

    auto *heading = new QLabel(tr("Activate your licence"));
    heading->setFont(Theme::font(23, QFont::Bold));
    heading->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    auto *headingRow = new QHBoxLayout;
    headingRow->setContentsMargins(0, 0, 0, 0);
    headingRow->setSpacing(12);
    headingRow->addWidget(icon, 0, Qt::AlignVCenter);
    headingRow->addWidget(heading, 0, Qt::AlignVCenter);
    headingRow->addStretch(1);

    auto *subtitle = new QLabel(
        tr("The features stay locked until you enter a valid licence key. Get one from "
           "the Nextgen Tweaks Discord, then paste it below."));
    subtitle->setFont(Theme::font(14));
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    m_license = new ModernLineEdit;
    m_license->setLeadingIcon(QStringLiteral("key"));
    m_license->setPlaceholderText(tr("Licence key"));
    m_license->lineEdit()->setMaxLength(220);

    m_status = new QLabel;
    m_status->setFont(Theme::font(13));
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(20);
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(c.danger.name()));

    m_signOut = new ModernButton(tr("Sign out"), ModernButton::Variant::Secondary);
    m_signOut->setIconName(QStringLiteral("logout"));
    m_signOut->setFont(Theme::font(14, QFont::Medium));
    m_signOut->setFixedHeight(52);

    m_activate = new ModernButton(tr("Activate"), ModernButton::Variant::Primary);
    m_activate->setIconName(QStringLiteral("check"));
    m_activate->setFont(Theme::font(15, QFont::DemiBold));
    m_activate->setFixedHeight(52);

    connect(m_signOut, &ModernButton::clicked, this, &LicenseActivationDialog::signOutRequested);
    connect(m_activate, &ModernButton::clicked, this, [this] {
        if (validate())
            emit submitted();
    });
    connect(m_license, &ModernLineEdit::returnPressed, this, [this] {
        if (validate())
            emit submitted();
    });

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    buttons->addWidget(m_signOut, 0);
    buttons->addStretch(1);
    buttons->addWidget(m_activate, 0);

    QVBoxLayout *layout = contentLayout();
    layout->addLayout(headingRow);
    layout->addSpacing(8);
    layout->addWidget(subtitle);
    layout->addSpacing(18);
    layout->addWidget(m_license);
    layout->addSpacing(6);
    layout->addWidget(m_status);
    layout->addSpacing(16);
    layout->addLayout(buttons);
}

QString LicenseActivationDialog::licenseKey() const
{
    return m_license->text().trimmed();
}

void LicenseActivationDialog::setBusy(bool busy)
{
    m_activate->setBusy(busy, tr("Activating..."));
    m_activate->setEnabled(!busy);
    m_signOut->setEnabled(!busy);
    m_license->setEnabled(!busy);
}

void LicenseActivationDialog::showError(const QString &message)
{
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::colors().danger.name()));
    m_status->setText(message);
}

bool LicenseActivationDialog::validate()
{
    if (!LicenseClient::isKeyWellFormed(licenseKey())) {
        showError(tr("Enter the licence key from your Discord (it starts with NGTL-)."));
        return false;
    }
    m_status->clear();
    return true;
}

void LicenseActivationDialog::onCloseRequested()
{
    // The header X / Alt+F4 quits the entire application rather than dismissing
    // the gate into a locked, unusable app.
    qApp->quit();
}

void LicenseActivationDialog::keyPressEvent(QKeyEvent *event)
{
    // Escape must not dismiss the gate; swallow it.
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        return;
    }
    FramelessDialog::keyPressEvent(event);
}

void LicenseActivationDialog::showEvent(QShowEvent *event)
{
    FramelessDialog::showEvent(event);
    m_license->setFocus();
}
