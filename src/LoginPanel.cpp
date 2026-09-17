#include "LoginPanel.h"

#include "AuthService.h"
#include "LinkLabel.h"
#include "ModernButton.h"
#include "ModernCheckBox.h"
#include "ModernLineEdit.h"
#include "OrDivider.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QVBoxLayout>

namespace {
/// Height reserved for the status line so showing a message never re-lays out
/// the form (which would fight the shake animation).
constexpr int kStatusHeight = 26;
} // namespace

LoginPanel::LoginPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void LoginPanel::buildUi()
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    // --- Headings ----------------------------------------------------------
    m_heading = new QLabel(this);
    m_heading->setTextFormat(Qt::RichText);
    m_heading->setFont(Theme::font(42, QFont::Bold, -0.5));
    m_heading->setText(QStringLiteral("<span style=\"color:%1;\">%2 </span>"
                                      "<span style=\"color:%3;\">%4</span>")
                           .arg(c.textPrimary.name(), tr("Welcome"), c.link.name(), tr("Back")));

    m_subHeading = new QLabel(tr("Sign in to your Nextgen Tweaks account"), this);
    m_subHeading->setFont(Theme::font(15));
    m_subHeading->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    // --- Fields ------------------------------------------------------------
    m_identifierField = new ModernLineEdit(this);
    m_identifierField->setLeadingIcon(QStringLiteral("user"));
    m_identifierField->setPlaceholderText(tr("Username or Email"));
    m_identifierField->lineEdit()->setMaxLength(254);
    m_identifierField->lineEdit()->setAccessibleName(tr("Username or email"));

    m_passwordField = new ModernLineEdit(this);
    m_passwordField->setLeadingIcon(QStringLiteral("lock"));
    m_passwordField->setPlaceholderText(tr("Password"));
    m_passwordField->setEchoMode(QLineEdit::Password);
    m_passwordField->setPasswordToggleEnabled(true);
    m_passwordField->lineEdit()->setMaxLength(128);
    m_passwordField->lineEdit()->setAccessibleName(tr("Password"));

    // --- Remember me / forgot password ------------------------------------
    m_rememberMe = new ModernCheckBox(tr("Remember me"), this);
    m_rememberMe->setChecked(true);

    m_forgotPassword = new LinkLabel(tr("Forgot password?"), this);

    auto *optionsRow = new QHBoxLayout;
    optionsRow->setContentsMargins(1, 0, 5, 0);
    optionsRow->setSpacing(12);
    optionsRow->addWidget(m_rememberMe, 0, Qt::AlignVCenter);
    optionsRow->addStretch(1);
    optionsRow->addWidget(m_forgotPassword, 0, Qt::AlignVCenter);

    // --- Status line -------------------------------------------------------
    m_status = new QLabel(this);
    m_status->setFont(Theme::font(13));
    m_status->setFixedHeight(kStatusHeight);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_status->setContentsMargins(5, 0, 5, 0);
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(c.danger.name()));

    // --- Buttons -----------------------------------------------------------
    m_loginButton = new ModernButton(tr("Log In"), ModernButton::Variant::Primary, this);
    m_loginButton->setIconName(QStringLiteral("login"));
    m_loginButton->setMinimumHeight(m.buttonHeight + 12);
    m_loginButton->setFont(Theme::font(17, QFont::DemiBold));

    m_divider = new OrDivider(tr("OR"), this);

    m_createAccountButton = new ModernButton(tr("Create Account"),
                                             ModernButton::Variant::Secondary, this);
    m_createAccountButton->setIconName(QStringLiteral("user_plus"));
    m_createAccountButton->setMinimumHeight(m.buttonHeight + 12);
    m_createAccountButton->setFont(Theme::font(16, QFont::Medium));

    // --- Form column -------------------------------------------------------
    auto *form = new QWidget(this);
    form->setMaximumWidth(m.formMaxWidth);
    form->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(0);
    formLayout->addWidget(m_heading);
    formLayout->addSpacing(10);
    formLayout->addWidget(m_subHeading);
    formLayout->addSpacing(30);
    formLayout->addWidget(m_identifierField);
    formLayout->addSpacing(6);
    formLayout->addWidget(m_passwordField);
    formLayout->addSpacing(10);
    formLayout->addLayout(optionsRow);
    formLayout->addSpacing(4);
    formLayout->addWidget(m_status);
    formLayout->addSpacing(2);
    formLayout->addWidget(m_loginButton);
    formLayout->addSpacing(12);
    formLayout->addWidget(m_divider);
    formLayout->addSpacing(12);
    formLayout->addWidget(m_createAccountButton);

    auto *centerRow = new QHBoxLayout;
    centerRow->setContentsMargins(0, 0, 0, 0);
    centerRow->addStretch(1);
    centerRow->addWidget(form, 8);
    centerRow->addStretch(1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(m.panelPadding, m.panelPadding + m.titleBarHeight, m.panelPadding,
                              m.panelPadding);
    outer->setSpacing(0);
    outer->addStretch(1);
    outer->addLayout(centerRow);
    outer->addStretch(1);

    // --- Behaviour ---------------------------------------------------------
    connect(m_identifierField, &ModernLineEdit::returnPressed, this, &LoginPanel::submit);
    connect(m_passwordField, &ModernLineEdit::returnPressed, this, &LoginPanel::submit);
    connect(m_loginButton, &ModernButton::clicked, this, &LoginPanel::submit);
    connect(m_createAccountButton, &ModernButton::clicked, this,
            &LoginPanel::createAccountRequested);
    connect(m_forgotPassword, &LinkLabel::clicked, this, [this] {
        emit forgotPasswordRequested(m_identifierField->text().trimmed());
    });
    connect(m_identifierField, &ModernLineEdit::textChanged, this, [this] { clearStatus(); });
    connect(m_passwordField, &ModernLineEdit::textChanged, this, [this] { clearStatus(); });

    // --- Keyboard navigation ----------------------------------------------
    setTabOrder(m_identifierField, m_passwordField);
    setTabOrder(m_passwordField, m_rememberMe);
    setTabOrder(m_rememberMe, m_forgotPassword);
    setTabOrder(m_forgotPassword, m_loginButton);
    setTabOrder(m_loginButton, m_createAccountButton);
}

QString LoginPanel::identifier() const
{
    return m_identifierField->text().trimmed();
}

QString LoginPanel::password() const
{
    return m_passwordField->text();
}

bool LoginPanel::rememberMe() const
{
    return m_rememberMe->isChecked();
}

bool LoginPanel::validateInputs()
{
    const QString user = identifier();
    const QString pass = password();

    bool ok = true;

    if (user.isEmpty()) {
        m_identifierField->setErrorState(true);
        m_identifierField->shake();
        ok = false;
    }

    if (pass.isEmpty()) {
        m_passwordField->setErrorState(true);
        m_passwordField->shake();
        ok = false;
    }

    if (!ok) {
        showError(tr("Please fill in both fields to continue."));
        if (user.isEmpty())
            m_identifierField->setFocus();
        else
            m_passwordField->setFocus();
        return false;
    }

    if (!Validation::isUserNameOrEmail(user)) {
        m_identifierField->setErrorState(true);
        m_identifierField->shake();
        m_identifierField->setFocus();
        showError(tr("Enter a valid username or email address."));
        return false;
    }

    return true;
}

void LoginPanel::submit()
{
    if (m_busy)
        return;

    clearStatus();
    if (!validateInputs())
        return;

    emit loginRequested(identifier(), password(), rememberMe());
}

void LoginPanel::focusFirstField()
{
    m_identifierField->setFocus();
}

void LoginPanel::setBusy(bool busy)
{
    if (m_busy == busy)
        return;

    m_busy = busy;
    m_loginButton->setBusy(busy, tr("Signing in..."));
    m_loginButton->setEnabled(!busy);
    m_createAccountButton->setEnabled(!busy);
    m_identifierField->setEnabled(!busy);
    m_passwordField->setEnabled(!busy);
    m_rememberMe->setEnabled(!busy);
    m_forgotPassword->setEnabled(!busy);
}

void LoginPanel::showError(const QString &message)
{
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::colors().danger.name()));
    m_status->setText(message);
}

void LoginPanel::showInfo(const QString &message)
{
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::colors().link.name()));
    m_status->setText(message);
}

void LoginPanel::clearStatus()
{
    m_status->clear();
    m_identifierField->setErrorState(false);
    m_passwordField->setErrorState(false);
}

void LoginPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF body(rect());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient base(body.topLeft(), body.bottomRight());
    base.setColorAt(0.0, c.formPanelTop);
    base.setColorAt(1.0, c.formPanelBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(body);

    // A faint blue bloom in the upper right keeps the panel from looking flat.
    QRadialGradient bloom(QPointF(body.width() * 0.92, body.height() * 0.06),
                          qMax(body.width(), body.height()) * 0.8);
    bloom.setColorAt(0.0, Theme::alpha(c.primary, 26));
    bloom.setColorAt(1.0, Qt::transparent);
    painter.setBrush(bloom);
    painter.drawRect(body);
}
