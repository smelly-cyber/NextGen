#include "UserCard.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QVariantAnimation>

namespace {
constexpr int kHeight = 78;
constexpr int kPadding = 14;
constexpr int kAvatarSize = 46;
constexpr int kAvatarGap = 15;
constexpr int kChevronSize = 16;
constexpr int kChevronBox = 30;
constexpr int kRadius = 14;
} // namespace

UserCard::UserCard(QWidget *parent)
    : QWidget(parent)
{
    m_userName = tr("User");
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedHeight(kHeight);

    m_hoverAnimation = new QVariantAnimation(this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { m_hover = v.toReal(); update(); });
}

void UserCard::setUserName(const QString &name)
{
    m_userName = name.trimmed().isEmpty() ? tr("User") : name.trimmed();
    setAccessibleName(m_userName);
    updateGeometry();
    update();
}

QSize UserCard::sizeHint() const
{
    const QFontMetrics nameMetrics(Theme::font(18, QFont::DemiBold));
    const QFontMetrics statusMetrics(Theme::font(13));
    const int textWidth = qMax(nameMetrics.horizontalAdvance(m_userName),
                               statusMetrics.horizontalAdvance(tr("Online")) + 16);
    return QSize(kPadding * 2 + kAvatarSize + kAvatarGap + textWidth + 26 + kChevronBox, kHeight);
}

QSize UserCard::minimumSizeHint() const
{
    return sizeHint();
}

void UserCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // --- Panel -------------------------------------------------------------
    QLinearGradient fill(body.topLeft(), body.bottomRight());
    fill.setColorAt(0.0, Theme::mix(c.cardTop, c.cardHoverTop, m_hover));
    fill.setColorAt(1.0, Theme::mix(c.cardBottom, c.cardHoverBottom, m_hover));
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(body, kRadius, kRadius);

    QPen border(Theme::mix(c.cardBorder, c.cardBorderSelected, m_hover * 0.8));
    border.setWidthF(1.0);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, kRadius, kRadius);

    // --- Avatar ------------------------------------------------------------
    const QRectF avatar(body.left() + kPadding, body.center().y() - kAvatarSize / 2.0,
                        kAvatarSize, kAvatarSize);
    QLinearGradient ring(avatar.topLeft(), avatar.bottomRight());
    ring.setColorAt(0.0, c.primaryBright);
    ring.setColorAt(1.0, c.primaryDeep);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ring);
    painter.drawEllipse(avatar);

    const int personSize = 24;
    const QPixmap person = IconProvider::pixmap(QStringLiteral("user"), personSize,
                                                c.textOnPrimary, devicePixelRatioF());
    if (!person.isNull()) {
        painter.drawPixmap(QPointF(avatar.center().x() - personSize / 2.0,
                                   avatar.center().y() - personSize / 2.0),
                           person);
    }

    // --- Name + presence ---------------------------------------------------
    const qreal textLeft = avatar.right() + kAvatarGap;

    const QFont nameFont = Theme::font(18, QFont::DemiBold);
    const QFont statusFont = Theme::font(13);
    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics statusMetrics(statusFont);

    const qreal blockHeight = nameMetrics.height() + 3 + statusMetrics.height();
    const qreal blockTop = body.center().y() - blockHeight / 2.0;

    painter.setFont(nameFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(textLeft, blockTop, body.width(), nameMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_userName);

    // Presence dot with a soft halo.
    const qreal dotY = blockTop + nameMetrics.height() + 3 + statusMetrics.height() / 2.0;
    const QPointF dot(textLeft + 4.0, dotY);
    QRadialGradient halo(dot, 7.0);
    halo.setColorAt(0.0, Theme::alpha(c.online, 120));
    halo.setColorAt(1.0, Qt::transparent);
    painter.setPen(Qt::NoPen);
    painter.setBrush(halo);
    painter.drawEllipse(dot, 7.0, 7.0);
    painter.setBrush(c.online);
    painter.drawEllipse(dot, 3.4, 3.4);

    painter.setFont(statusFont);
    painter.setPen(c.textSecondary);
    painter.drawText(QRectF(textLeft + 16.0, blockTop + nameMetrics.height() + 3, body.width(),
                            statusMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, tr("Online"));

    // --- Chevron -----------------------------------------------------------
    const QRectF chevronBox(body.right() - kPadding - kChevronBox,
                            body.center().y() - kChevronBox / 2.0, kChevronBox, kChevronBox);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::alpha(c.primary, int(30 + 40 * m_hover)));
    painter.drawEllipse(chevronBox);

    const QPixmap chevron = IconProvider::pixmap(
        QStringLiteral("chevron_down"), kChevronSize,
        Theme::mix(c.textSecondary, c.primaryBright, m_hover), devicePixelRatioF());
    if (!chevron.isNull()) {
        painter.drawPixmap(QPointF(chevronBox.center().x() - kChevronSize / 2.0,
                                   chevronBox.center().y() - kChevronSize / 2.0),
                           chevron);
    }
}

void UserCard::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setDuration(Theme::durations().hover);
    m_hoverAnimation->setStartValue(m_hover);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
}

void UserCard::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setDuration(Theme::durations().hover);
    m_hoverAnimation->setStartValue(m_hover);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
}

void UserCard::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit clicked();
}
