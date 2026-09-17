#include "SectionCard.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QVBoxLayout>

namespace {
constexpr int kPaddingX = 20;
constexpr int kPaddingTop = 14;
constexpr int kPaddingBottom = 14;
/// Gap between the card's header text and the start of its content.
constexpr int kHeaderGap = 10;
constexpr int kIconSize = 20;
constexpr int kIconGap = 10;
} // namespace

SectionCard::SectionCard(const QString &title, const QString &subtitle, const QString &iconName,
                         QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_subtitle(subtitle)
    , m_iconName(iconName)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QFontMetrics titleMetrics(Theme::font(16, QFont::DemiBold));
    const QFontMetrics subtitleMetrics(Theme::font(12));

    int headerHeight = titleMetrics.height();
    if (!m_subtitle.isEmpty())
        headerHeight += 3 + subtitleMetrics.height();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(kPaddingX, kPaddingTop + headerHeight + kHeaderGap, kPaddingX,
                              kPaddingBottom);
    outer->setSpacing(0);

    m_contentLayout = new QVBoxLayout;
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    outer->addLayout(m_contentLayout);
}

void SectionCard::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void SectionCard::setSubtitle(const QString &subtitle)
{
    m_subtitle = subtitle;
    update();
}

void SectionCard::setTitleAccented(bool accented)
{
    m_titleAccented = accented;
    update();
}

void SectionCard::paintEvent(QPaintEvent *event)
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

    // --- Header -------------------------------------------------------------
    qreal x = kPaddingX;
    const QFont titleFont = Theme::font(16, QFont::DemiBold);
    const QFontMetrics titleMetrics(titleFont);
    const qreal titleTop = kPaddingTop;

    if (!m_iconName.isEmpty()) {
        const QPixmap glyph = IconProvider::pixmap(m_iconName, kIconSize, c.primaryBright,
                                                   devicePixelRatioF());
        if (!glyph.isNull()) {
            painter.drawPixmap(
                QPointF(x, titleTop + titleMetrics.height() / 2.0 - kIconSize / 2.0), glyph);
            x += kIconSize + kIconGap;
        }
    }

    painter.setFont(titleFont);
    painter.setPen(m_titleAccented ? c.link : c.textPrimary);
    painter.drawText(QRectF(x, titleTop, width() - x - kPaddingX, titleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_title);

    if (!m_subtitle.isEmpty()) {
        const QFont subtitleFont = Theme::font(12);
        const QFontMetrics subtitleMetrics(subtitleFont);
        painter.setFont(subtitleFont);
        painter.setPen(c.textMuted);
        painter.drawText(QRectF(kPaddingX, titleTop + titleMetrics.height() + 3,
                                width() - 2 * kPaddingX, subtitleMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_subtitle);
    }
}
