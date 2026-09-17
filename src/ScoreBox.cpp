#include "ScoreBox.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>

namespace {
constexpr int kPadding = 14;
constexpr int kShieldSize = 26;
constexpr int kShieldGap = 12;
constexpr int kBoxHeight = 74;
} // namespace

ScoreBox::ScoreBox(const QString &caption, Tone tone, QWidget *parent)
    : QWidget(parent)
    , m_caption(caption)
    , m_tone(tone)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kBoxHeight);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void ScoreBox::setScore(qreal score)
{
    m_score = score;
    update();
}

QSize ScoreBox::sizeHint() const
{
    return QSize(190, kBoxHeight);
}

QSize ScoreBox::minimumSizeHint() const
{
    return QSize(140, kBoxHeight);
}

void ScoreBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const qreal radius = 10.0;
    const bool projected = m_tone == Tone::Projected;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QLinearGradient fill(body.topLeft(), body.bottomRight());
    fill.setColorAt(0.0, projected ? Theme::mix(c.cardTop, c.primary.darker(230), 0.55)
                                   : c.cardTop);
    fill.setColorAt(1.0, c.cardBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(body, radius, radius);

    QPen border(projected ? Theme::mix(c.cardBorder, c.primary, 0.55) : c.cardBorder);
    border.setWidthF(1.0);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, radius, radius);

    // --- Shield --------------------------------------------------------------
    qreal x = kPadding;
    const QPixmap shield = IconProvider::pixmap(QStringLiteral("shield_check"), kShieldSize,
                                                projected ? c.cyan : c.primaryBright,
                                                devicePixelRatioF());
    if (!shield.isNull()) {
        painter.drawPixmap(QPointF(x, body.center().y() - kShieldSize / 2.0), shield);
        x += kShieldSize + kShieldGap;
    }

    // --- Caption -------------------------------------------------------------
    const QFont captionFont = Theme::font(12, QFont::Medium);
    const QFont scoreFont = Theme::font(24, QFont::Bold);
    const QFont outOfFont = Theme::font(13);
    const QFontMetrics captionMetrics(captionFont);
    const QFontMetrics scoreMetrics(scoreFont);
    const QFontMetrics outOfMetrics(outOfFont);

    const qreal textWidth = qMax(10.0, width() - x - kPadding);
    const qreal blockHeight = captionMetrics.height() + 2 + scoreMetrics.height();
    const qreal blockTop = body.center().y() - blockHeight / 2.0;

    painter.setFont(captionFont);
    painter.setPen(c.textSecondary);
    painter.drawText(QRectF(x, blockTop, textWidth, captionMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     captionMetrics.elidedText(m_caption, Qt::ElideRight, qRound(textWidth)));

    // --- Score ---------------------------------------------------------------
    const QString number = m_score < 0.0 ? QStringLiteral("—")
                                         : QString::number(qRound(m_score));
    const qreal scoreTop = blockTop + captionMetrics.height() + 2;

    painter.setFont(scoreFont);
    painter.setPen(projected ? c.primaryBright : c.textPrimary);
    painter.drawText(QRectF(x, scoreTop, textWidth, scoreMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, number);

    if (m_score >= 0.0) {
        const qreal outOfLeft = x + scoreMetrics.horizontalAdvance(number) + 2;
        painter.setFont(outOfFont);
        painter.setPen(c.textMuted);
        painter.drawText(QRectF(outOfLeft, scoreTop, qMax(10.0, width() - outOfLeft - kPadding),
                                scoreMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("/100"));
        Q_UNUSED(outOfMetrics)
    }
}
