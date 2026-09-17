#include "StarRating.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QPainter>

namespace {
constexpr int kStars = 5;
constexpr int kStarSize = 18;
constexpr int kStarGap = 6;
} // namespace

StarRating::StarRating(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void StarRating::setScore(qreal score)
{
    m_score = score;
    update();
}

QSize StarRating::sizeHint() const
{
    return QSize(kStars * kStarSize + (kStars - 1) * kStarGap, kStarSize + 4);
}

QSize StarRating::minimumSizeHint() const
{
    return sizeHint();
}

void StarRating::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if (m_score < 0.0)
        return;

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Each star is worth 20 points; half a star is rounded to the nearest whole.
    const int filled = qBound(0, qRound(m_score / 100.0 * kStars), kStars);
    const qreal y = (height() - kStarSize) / 2.0;

    for (int i = 0; i < kStars; ++i) {
        const bool on = i < filled;
        const QPixmap glyph = IconProvider::pixmap(
            on ? QStringLiteral("star") : QStringLiteral("star_outline"), kStarSize,
            on ? c.primary : c.gaugeTrack, devicePixelRatioF());
        if (!glyph.isNull())
            painter.drawPixmap(QPointF(i * (kStarSize + kStarGap), y), glyph);
    }
}
