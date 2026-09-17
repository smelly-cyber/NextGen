#include "Badge.h"

#include "Theme.h"

#include <QFontMetrics>
#include <QPainter>

namespace {
constexpr int kDotDiameter = 7;
constexpr int kDotGap = 8;
constexpr int kPillPaddingX = 11;
constexpr int kPillHeight = 24;
} // namespace

Badge::Badge(const QString &text, Style style, QWidget *parent)
    : QWidget(parent)
    , m_text(text)
    , m_style(style)
{
    setFont(Theme::font(13));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void Badge::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    updateGeometry();
    update();
}

void Badge::setStyle(Style style)
{
    if (m_style == style)
        return;
    m_style = style;
    updateGeometry();
    update();
}

QSize Badge::sizeHint() const
{
    const QFontMetrics fm(font());
    int width = fm.horizontalAdvance(m_text) + 2;

    switch (m_style) {
    case Style::Dot:
        width += kDotDiameter + kDotGap;
        break;
    case Style::Outlined:
        width += 2 * kPillPaddingX;
        break;
    case Style::Plain:
        break;
    }

    return QSize(width, qMax(kPillHeight, fm.height() + 4));
}

QSize Badge::minimumSizeHint() const
{
    return sizeHint();
}

void Badge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font());

    const qreal centreY = height() / 2.0;
    qreal textLeft = 0.0;

    switch (m_style) {
    case Style::Dot: {
        const QRectF dot(0, centreY - kDotDiameter / 2.0, kDotDiameter, kDotDiameter);
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.primary);
        painter.drawEllipse(dot);
        textLeft = kDotDiameter + kDotGap;
        painter.setPen(c.badgeText);
        break;
    }
    case Style::Outlined: {
        const QRectF pill = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPen outline(c.badgeOutline);
        outline.setWidthF(1.0);
        painter.setPen(outline);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
        textLeft = kPillPaddingX;
        painter.setPen(c.textSecondary);
        break;
    }
    case Style::Plain:
        painter.setPen(c.textSecondary);
        break;
    }

    painter.drawText(QRectF(textLeft, 0, width() - textLeft, height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_text);
}
