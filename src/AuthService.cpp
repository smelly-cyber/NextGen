#include "AuthService.h"

#include <QRegularExpression>
#include <QTimer>

namespace Validation {

bool isEmail(const QString &value)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)"));
    return re.match(value.trimmed()).hasMatch();
}

bool isUserName(const QString &value)
{
    static const QRegularExpression re(QStringLiteral(R"(^[A-Za-z0-9._\-]{3,32}$)"));
    return re.match(value.trimmed()).hasMatch();
}

bool isUserNameOrEmail(const QString &value)
{
    return isEmail(value) || isUserName(value);
}

int minimumPasswordLength()
{
    return 6;
}

QString displayNameFor(const QString &identifier)
{
    QString name = identifier.trimmed();
    const int at = name.indexOf(QLatin1Char('@'));
    if (at > 0)
        name = name.left(at);
    if (name.isEmpty())
        return QObject::tr("User");
    name[0] = name.at(0).toUpper();
    return name;
}

} // namespace Validation

namespace {
/// How long the stub pretends to talk to a server.
constexpr int kSimulatedLatencyMs = 1500;
} // namespace

AuthService::AuthService(QObject *parent)
    : QObject(parent)
{
    m_requestTimer = new QTimer(this);
    m_requestTimer->setSingleShot(true);
    m_requestTimer->setInterval(kSimulatedLatencyMs);

    connect(m_requestTimer, &QTimer::timeout, this, [this] {
        const QString identifier = m_pendingIdentifier;
        m_pendingIdentifier.clear();
        setBusy(false);

        // TODO: replace with a real network round trip against the Nextgen
        // Tweaks API. Until that exists the service runs in offline mode and
        // accepts any well formed credentials so the rest of the app is usable.
        emit loginSucceeded(identifier, Validation::displayNameFor(identifier));
    });
}

AuthService::~AuthService() = default;

void AuthService::login(const QString &identifier, const QString &password, bool rememberMe)
{
    Q_UNUSED(rememberMe)

    if (m_busy)
        return;

    if (!Validation::isUserNameOrEmail(identifier)) {
        emit loginFailed(tr("Enter a valid username or email address."));
        return;
    }

    if (password.length() < Validation::minimumPasswordLength()) {
        emit loginFailed(tr("Your password must be at least %1 characters long.")
                             .arg(Validation::minimumPasswordLength()));
        return;
    }

    m_pendingIdentifier = identifier;
    setBusy(true);
    emit loginStarted();
    m_requestTimer->start();
}

void AuthService::cancel()
{
    if (!m_busy)
        return;
    m_requestTimer->stop();
    m_pendingIdentifier.clear();
    setBusy(false);
}

void AuthService::requestRegistration()
{
    emit registrationRequested();
}

void AuthService::requestPasswordReset(const QString &identifier)
{
    emit passwordResetRequested(identifier);
}

void AuthService::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(m_busy);
}
