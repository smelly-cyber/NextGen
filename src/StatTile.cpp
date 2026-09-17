#include "StatTile.h"

#include "DonutGauge.h"
#include "IconProvider.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>

namespace {
constexpr int kPadding = 14;
constexpr int kIconSize = 22;
constexpr int kGaugeSize = 70;
constexpr int kTileHeight = 164;
} // namespace

StatTile::StatTile(const QString &iconName, const QString &caption, Mode mode, QWidget *parent)
    : QWidget(parent)
    , m_iconName(iconName)
    , m_caption(caption)
    , m_mode(mode)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kTileHeight);

    if (m_mode == Mode::Gauge) {
        m_gauge = new DonutGauge(this);
        m_gauge->setFixedSize(kGaugeSize, kGaugeSize);
        m_gauge->setRingThickness(6.5);
        m_gauge->setValueFontSize(20);
    }
}

void StatTile::setValue(qreal value)
{
    m_value = value;
    if (m_gauge)
        m_gauge->setValue(value);
    update();
}

void StatTile::setText(const QString &text)
{
    m_text = text;
    update();
}

void StatTile::setUnit(const QString &unit)
{
    m_unit = unit;
    if (m_gauge) {
        m_gauge->setUnit(unit);
        m_gauge->setShowUnit(!unit.isEmpty());
    }
    update();
}

QSize StatTile::sizeHint() const
{
    return QSize(160, kTileHeight);
}

QSize StatTile::minimumSizeHint() const
{
    return QSize(118, kTileHeight);
}

void StatTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_gauge)
        m_gauge->move((width() - kGaugeSize) / 2, kPadding + kIconSize + 6);
}

void StatTile::paintEvent(QPaintEvent *event)
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

    // --- Icon ---------------------------------------------------------------
    const QPixmap glyph = IconProvider::pixmap(m_iconName, kIconSize, c.primaryBright,
                                               devicePixelRatioF());
    if (!glyph.isNull())
        painter.drawPixmap(QPointF((width() - kIconSize) / 2.0, kPadding), glyph);

    // --- Value (plain mode) --------------------------------------------------
    if (m_mode == Mode::Value) {
        // A pre-formatted string (e.g. "2h 14m") uses a smaller font so it fits
        // the narrow tile; a bare number keeps the large display.
        const bool useText = !m_text.isEmpty();
        const QFont valueFont = Theme::font(useText ? 23 : 30, QFont::Bold);
        const QFont unitFont = Theme::font(17, QFont::Medium);
        const QFontMetrics valueMetrics(valueFont);
        const QFontMetrics unitMetrics(unitFont);

        const QString number = useText ? m_text
                                       : (m_value < 0.0 ? QStringLiteral("—")
                                                        : QString::number(qRound(m_value)));
        const QString unit = (useText || m_value < 0.0) ? QString() : m_unit;

        const int numberWidth = valueMetrics.horizontalAdvance(number);
        const int unitWidth = unit.isEmpty() ? 0 : unitMetrics.horizontalAdvance(unit) + 1;

        qreal x = width() / 2.0 - (numberWidth + unitWidth) / 2.0;
        const qreal centreY = kPadding + kIconSize + 6 + kGaugeSize / 2.0;

        painter.setFont(valueFont);
        painter.setPen((!useText && m_value < 0.0) ? c.textMuted : c.textPrimary);
        painter.drawText(QRectF(x, centreY - valueMetrics.height() / 2.0, numberWidth + 2,
                                valueMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, number);

        if (!unit.isEmpty()) {
            x += numberWidth + 1;
            painter.setFont(unitFont);
            painter.setPen(c.textSecondary);
            painter.drawText(QRectF(x, centreY - valueMetrics.height() / 2.0, unitWidth + 2,
                                    valueMetrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, unit);
        }
    }

    // --- Caption -------------------------------------------------------------
    const QFont captionFont = Theme::font(12, QFont::Medium);
    const QFontMetrics captionMetrics(captionFont);
    painter.setFont(captionFont);
    painter.setPen(c.textSecondary);

    const QRectF captionRect(8, height() - kPadding - captionMetrics.height() * 2 - 2,
                             width() - 16, captionMetrics.height() * 2 + 2);
    painter.drawText(captionRect, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap,
                     m_caption);
}
