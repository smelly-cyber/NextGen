#include "PageHeader.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>

namespace {
constexpr int kRuleGap = 20;
constexpr int kRuleWidth = 120;
constexpr int kSubtitleGap = 4;
} // namespace

PageHeader::PageHeader(const QString &title, const QString &subtitle, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_subtitle(subtitle)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void PageHeader::setTitle(const QString &title)
{
    m_title = title;
    updateGeometry();
    update();
}

void PageHeader::setSubtitle(const QString &subtitle)
{
    m_subtitle = subtitle;
    updateGeometry();
    update();
}

QSize PageHeader::sizeHint() const
{
    const QFontMetrics titleMetrics(Theme::font(40, QFont::Bold, -0.5));
    const QFontMetrics subtitleMetrics(Theme::font(14));
    return QSize(320, titleMetrics.height() + kSubtitleGap + subtitleMetrics.height() + 4);
}

QSize PageHeader::minimumSizeHint() const
{
    return sizeHint();
}

void PageHeader::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QFont titleFont = Theme::font(40, QFont::Bold, -0.5);
    const QFont subtitleFont = Theme::font(14);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics subtitleMetrics(subtitleFont);

    painter.setFont(titleFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(0, 0, width(), titleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // Accent rule trailing off to the right of the title.
    const qreal ruleLeft = titleMetrics.horizontalAdvance(m_title) + kRuleGap;
    const qreal ruleRight = qMin(qreal(width()), ruleLeft + kRuleWidth);
    if (ruleRight > ruleLeft + 12) {
        const qreal y = titleMetrics.height() / 2.0 + 2;
        QLinearGradient rule(ruleLeft, 0, ruleRight, 0);
        rule.setColorAt(0.0, c.primary);
        rule.setColorAt(1.0, Theme::alpha(c.primary, 0));

        QPen pen(QBrush(rule), 2.4);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(QPointF(ruleLeft, y), QPointF(ruleRight, y));
    }

    painter.setFont(subtitleFont);
    painter.setPen(c.textSecondary);
    painter.drawText(QRectF(0, titleMetrics.height() + kSubtitleGap, width(),
                            subtitleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_subtitle);
}
