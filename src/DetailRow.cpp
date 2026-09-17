#include "DetailRow.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>

namespace {
// 34 rather than 40: System Info stacks fourteen of these in one column, and at
// 40 the column was taller than the window could ever be.
constexpr int kRowHeight = 34;
constexpr int kIconSize = 18;
constexpr int kIconGap = 12;
/// Fraction of the row given to the label before the value starts.
constexpr qreal kLabelFraction = 0.42;
} // namespace

DetailRow::DetailRow(const QString &label, const QString &iconName, QWidget *parent)
    : QWidget(parent)
    , m_label(label)
    , m_iconName(iconName)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kRowHeight);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void DetailRow::setValue(const QString &value)
{
    m_value = value;
    setToolTip(value);
    update();
}

void DetailRow::setSeparatorVisible(bool visible)
{
    m_separator = visible;
    update();
}

QSize DetailRow::sizeHint() const
{
    return QSize(320, kRowHeight);
}

QSize DetailRow::minimumSizeHint() const
{
    return QSize(220, kRowHeight);
}

void DetailRow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    qreal x = 0;
    if (!m_iconName.isEmpty()) {
        const QPixmap glyph = IconProvider::pixmap(m_iconName, kIconSize, c.primary,
                                                   devicePixelRatioF());
        if (!glyph.isNull()) {
            painter.drawPixmap(QPointF(x, height() / 2.0 - kIconSize / 2.0), glyph);
            x += kIconSize + kIconGap;
        }
    }

    const QFont labelFont = Theme::font(13);
    const QFont valueFont = Theme::font(13, QFont::Medium);
    const QFontMetrics labelMetrics(labelFont);
    const QFontMetrics valueMetrics(valueFont);

    const qreal labelWidth = qMax(60.0, (width() - x) * kLabelFraction);

    painter.setFont(labelFont);
    painter.setPen(c.textMuted);
    painter.drawText(QRectF(x, 0, labelWidth, height()), Qt::AlignLeft | Qt::AlignVCenter,
                     labelMetrics.elidedText(m_label, Qt::ElideRight, qRound(labelWidth)));

    const qreal valueLeft = x + labelWidth + 8;
    const qreal valueWidth = qMax(20.0, width() - valueLeft);

    painter.setFont(valueFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(valueLeft, 0, valueWidth, height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     valueMetrics.elidedText(m_value.isEmpty() ? QStringLiteral("—") : m_value,
                                             Qt::ElideRight, qRound(valueWidth)));

    if (!m_separator)
        return;

    QLinearGradient hairline(0, 0, width(), 0);
    hairline.setColorAt(0.0, Theme::alpha(c.cardBorder, 200));
    hairline.setColorAt(0.85, Theme::alpha(c.cardBorder, 90));
    hairline.setColorAt(1.0, Theme::alpha(c.cardBorder, 0));
    painter.setPen(QPen(QBrush(hairline), 1.0));
    painter.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
}
