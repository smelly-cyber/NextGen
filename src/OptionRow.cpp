#include "OptionRow.h"

#include "IconProvider.h"
#include "Theme.h"
#include "ToggleSwitch.h"

#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

namespace {
constexpr int kIconSize = 22;
constexpr int kIconInset = 14;
constexpr int kTextLeft = 50;
constexpr int kBadgeGap = 14;
} // namespace

OptionRow::OptionRow(const QString &iconName, const QString &title, const QString &description,
                     Emphasis emphasis, bool enabledByDefault, QWidget *parent)
    : QWidget(parent)
    , m_iconName(iconName)
    , m_title(title)
    , m_description(description)
{
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    switch (emphasis) {
    case Emphasis::Recommended:
        m_badge = new Badge(tr("Recommended"), Badge::Style::Dot, this);
        break;
    case Emphasis::Optional:
        m_badge = new Badge(tr("Optional"), Badge::Style::Outlined, this);
        break;
    case Emphasis::Neutral:
        break;
    }

    m_toggle = new ToggleSwitch(this);
    m_toggle->setChecked(enabledByDefault);
    m_toggle->setAccessibleName(title);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(kTextLeft, 0, 12, 0);
    layout->setSpacing(kBadgeGap);
    layout->addStretch(1);
    if (m_badge)
        layout->addWidget(m_badge, 0, Qt::AlignVCenter);
    layout->addWidget(m_toggle, 0, Qt::AlignVCenter);

    connect(m_toggle, &ToggleSwitch::toggled, this, [this](bool on) {
        update();
        emit optionToggled(on);
    });
}

bool OptionRow::isOptionEnabled() const
{
    return m_toggle->isChecked();
}

void OptionRow::setOptionEnabled(bool enabled)
{
    m_toggle->setChecked(enabled);
}

void OptionRow::setFramed(bool framed)
{
    if (m_framed == framed)
        return;
    m_framed = framed;
    updateGeometry();
    update();
}

void OptionRow::setBadgeInline(bool inlineBadge)
{
    if (m_badgeInline == inlineBadge || !m_badge)
        return;

    m_badgeInline = inlineBadge;
    if (auto *box = qobject_cast<QHBoxLayout *>(layout())) {
        if (inlineBadge)
            box->removeWidget(m_badge);   // positioned manually in resizeEvent
        else
            box->insertWidget(box->count() - 1, m_badge, 0, Qt::AlignVCenter);
    }
    updateGeometry();
    update();
}

void OptionRow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (!m_badgeInline || !m_badge)
        return;

    const QFontMetrics titleMetrics(Theme::font(14, QFont::DemiBold));
    const int x = kTextLeft + titleMetrics.horizontalAdvance(m_title) + 12;
    const QSize hint = m_badge->sizeHint();
    m_badge->setGeometry(x, (height() - hint.height()) / 2, hint.width(), hint.height());
}

QSize OptionRow::sizeHint() const
{
    const int extra = m_framed ? 10 : 0;
    return QSize(420, Theme::metrics().optionRowHeight + extra);
}

QSize OptionRow::minimumSizeHint() const
{
    return QSize(300, Theme::metrics().optionRowHeight + (m_framed ? 10 : 0));
}

void OptionRow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const bool on = m_toggle->isChecked();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_framed) {
        // Each tweak gets its own card.
        const QRectF box = QRectF(rect()).adjusted(0.5, 3.5, -0.5, -3.5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_hovered ? Theme::mix(c.cardTop, c.cardHoverTop, 0.8) : c.cardTop);
        painter.drawRoundedRect(box, 9, 9);

        QPen border(m_hovered ? Theme::mix(c.cardBorder, c.primary, 0.6) : c.cardBorder);
        border.setWidthF(1.0);
        painter.setPen(border);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box, 9, 9);
    } else if (m_hovered) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::alpha(c.primary, 16));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 1, -0.5, -1), 8, 8);
    }

    // --- Icon ---------------------------------------------------------------
    const QColor iconColor = on ? c.primaryBright : c.textMuted;
    const QPixmap glyph = IconProvider::pixmap(m_iconName, kIconSize, iconColor,
                                               devicePixelRatioF());
    if (!glyph.isNull()) {
        painter.drawPixmap(QPointF(kIconInset, height() / 2.0 - kIconSize / 2.0), glyph);
    }

    // --- Title and description ----------------------------------------------
    const QFont titleFont = Theme::font(14, QFont::DemiBold);
    const QFont descriptionFont = Theme::font(12);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics descriptionMetrics(descriptionFont);

    qreal textRight = width() - 12.0;
    if (m_badge && !m_badgeInline)
        textRight = m_badge->x() - kBadgeGap;
    else if (m_toggle)
        textRight = m_toggle->x() - kBadgeGap;
    qreal textWidth = qMax(24.0, textRight - kTextLeft);

    const bool hasDescription = !m_description.isEmpty();
    const qreal blockHeight =
        hasDescription ? titleMetrics.height() + 2 + descriptionMetrics.height()
                       : titleMetrics.height();
    const qreal blockTop = height() / 2.0 - blockHeight / 2.0;

    // An inline badge sits after the title, so the title keeps its natural width.
    const qreal titleWidth =
        m_badgeInline ? qMin(textWidth, qreal(titleMetrics.horizontalAdvance(m_title) + 2))
                      : textWidth;

    painter.setFont(titleFont);
    painter.setPen(on ? c.textPrimary : Theme::mix(c.textPrimary, c.textMuted, 0.45));
    painter.drawText(QRectF(kTextLeft, blockTop, titleWidth, titleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(m_title, Qt::ElideRight, qRound(titleWidth)));

    if (!hasDescription)
        return;

    painter.setFont(descriptionFont);
    painter.setPen(c.textMuted);
    painter.drawText(QRectF(kTextLeft, blockTop + titleMetrics.height() + 2, textWidth,
                            descriptionMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     descriptionMetrics.elidedText(m_description, Qt::ElideRight,
                                                   qRound(textWidth)));
}

void OptionRow::updateHoverState()
{
    // A Leave arrives as soon as the cursor moves onto the toggle, so hover is
    // derived from the real cursor position instead.
    const bool hovered = rect().contains(mapFromGlobal(QCursor::pos()));
    if (m_hovered == hovered)
        return;
    m_hovered = hovered;
    update();
}

void OptionRow::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    updateHoverState();
}

void OptionRow::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    updateHoverState();
}

void OptionRow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Accept the press so the matching release is delivered here.
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void OptionRow::mouseReleaseEvent(QMouseEvent *event)
{
    // Clicking anywhere on the row flips the switch.
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        m_toggle->toggle();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
