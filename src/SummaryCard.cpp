#include "SummaryCard.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>

namespace {
constexpr int kPadding = 16;
constexpr int kIconSize = 30;
constexpr int kIconGap = 14;
constexpr int kCardHeight = 84;
} // namespace

SummaryCard::SummaryCard(const QString &iconName, const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_iconName(iconName)
    , m_title(title)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kCardHeight);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void SummaryCard::setValue(const QString &value)
{
    m_value = value;
    update();
}

QSize SummaryCard::sizeHint() const
{
    return QSize(220, kCardHeight);
}

QSize SummaryCard::minimumSizeHint() const
{
    return QSize(150, kCardHeight);
}

void SummaryCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const qreal radius = Theme::metrics().cardRadius;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QLinearGradient fill(body.topLeft(), body.bottomRight());
    fill.setColorAt(0.0, c.cardTop);
    fill.setColorAt(1.0, c.cardBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(body, radius, radius);

    QPen border(c.cardBorder);
    border.setWidthF(1.0);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, radius, radius);

    qreal x = kPadding;
    const QPixmap glyph = IconProvider::pixmap(m_iconName, kIconSize, c.primaryBright,
                                               devicePixelRatioF());
    if (!glyph.isNull()) {
        painter.drawPixmap(QPointF(x, body.center().y() - kIconSize / 2.0), glyph);
        x += kIconSize + kIconGap;
    }

    const QFont titleFont = Theme::font(15, QFont::DemiBold);
    const QFont valueFont = Theme::font(12);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics valueMetrics(valueFont);

    const qreal textWidth = qMax(10.0, width() - x - kPadding);
    const qreal blockHeight = titleMetrics.height() + 3 + valueMetrics.height() * 2;
    const qreal blockTop = body.center().y() - blockHeight / 2.0;

    painter.setFont(titleFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(x, blockTop, textWidth, titleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_title);

    painter.setFont(valueFont);
    painter.setPen(c.textSecondary);
    painter.drawText(QRectF(x, blockTop + titleMetrics.height() + 3, textWidth,
                            valueMetrics.height() * 2),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     m_value.isEmpty() ? QStringLiteral("—") : m_value);
}
