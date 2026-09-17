#include "MetricCard.h"

#include "DonutGauge.h"
#include "IconProvider.h"
#include "Sparkline.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>

namespace {
constexpr int kPadding = 16;
constexpr int kIconSize = 20;
constexpr int kIconGap = 10;
constexpr int kHeaderHeight = 26;
constexpr int kGaugeSize = 72;
constexpr int kSparklineHeight = 30;
} // namespace

MetricCard::MetricCard(const QString &iconName, const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_iconName(iconName)
    , m_title(title)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(Theme::metrics().metricCardHeight);

    m_gauge = new DonutGauge(this);
    m_gauge->setFixedSize(kGaugeSize, kGaugeSize);
    m_gauge->setRingThickness(7.0);
    m_gauge->setValueFontSize(19);

    m_sparkline = new Sparkline(this);
}

void MetricCard::setValue(qreal percent)
{
    m_gauge->setValue(percent);
}

void MetricCard::setDetail(const QString &primary, const QString &secondary)
{
    m_detailPrimary = primary;
    m_detailSecondary = secondary;
    update();
}

void MetricCard::setGaugeText(const QString &text)
{
    m_gauge->setCentreText(text);
}

void MetricCard::setHistory(const QList<qreal> &history)
{
    m_sparkline->setValues(history);
}

QSize MetricCard::sizeHint() const
{
    return QSize(230, Theme::metrics().metricCardHeight);
}

QSize MetricCard::minimumSizeHint() const
{
    return QSize(170, Theme::metrics().metricCardHeight);
}

void MetricCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    const int gaugeTop = kPadding + kHeaderHeight + 12;
    m_gauge->move(kPadding, gaugeTop);

    m_sparkline->setGeometry(kPadding, height() - kPadding - kSparklineHeight,
                             qMax(10, width() - 2 * kPadding), kSparklineHeight);
}

void MetricCard::paintEvent(QPaintEvent *event)
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

    // --- Header --------------------------------------------------------------
    const QFont titleFont = Theme::font(15, QFont::DemiBold);
    const QFontMetrics titleMetrics(titleFont);

    qreal x = kPadding;
    const qreal headerCentre = kPadding + kHeaderHeight / 2.0;

    const QPixmap glyph = IconProvider::pixmap(m_iconName, kIconSize, c.primaryBright,
                                               devicePixelRatioF());
    if (!glyph.isNull()) {
        painter.drawPixmap(QPointF(x, headerCentre - kIconSize / 2.0), glyph);
        x += kIconSize + kIconGap;
    }

    painter.setFont(titleFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(x, kPadding, width() - x - kPadding, kHeaderHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(m_title, Qt::ElideRight,
                                             qRound(width() - x - kPadding)));

    // --- Detail lines next to the gauge --------------------------------------
    const qreal detailLeft = kPadding + kGaugeSize + 14;
    const qreal detailWidth = qMax(10.0, width() - detailLeft - kPadding);

    const QFont primaryFont = Theme::font(12, QFont::Medium);
    const QFont secondaryFont = Theme::font(12);
    const QFontMetrics primaryMetrics(primaryFont);
    const QFontMetrics secondaryMetrics(secondaryFont);

    const qreal blockHeight = primaryMetrics.height() + 2 + secondaryMetrics.height();
    const qreal gaugeCentre = kPadding + kHeaderHeight + 12 + kGaugeSize / 2.0;
    const qreal blockTop = gaugeCentre - blockHeight / 2.0;

    painter.setFont(primaryFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(detailLeft, blockTop, detailWidth, primaryMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     primaryMetrics.elidedText(m_detailPrimary, Qt::ElideRight,
                                               qRound(detailWidth)));

    painter.setFont(secondaryFont);
    painter.setPen(c.textMuted);
    painter.drawText(QRectF(detailLeft, blockTop + primaryMetrics.height() + 2, detailWidth,
                            secondaryMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     secondaryMetrics.elidedText(m_detailSecondary, Qt::ElideRight,
                                                 qRound(detailWidth)));
}
