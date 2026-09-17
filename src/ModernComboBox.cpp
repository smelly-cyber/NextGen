#include "ModernComboBox.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPropertyAnimation>

namespace {
constexpr int kHeight = 36;
constexpr int kPaddingX = 12;
constexpr int kChevronSize = 16;
constexpr int kLeadSize = 16;
constexpr int kLeadGap = 9;
} // namespace

ModernComboBox::ModernComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setFont(Theme::font(13, QFont::Medium));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const Theme::Palette &c = Theme::colors();

    // The popup is a native view, so it is styled with QSS rather than painted.
    view()->setStyleSheet(
        QStringLiteral("QAbstractItemView {"
                       "  background-color: %1;"
                       "  border: 1px solid %2;"
                       "  border-radius: 8px;"
                       "  padding: 4px;"
                       "  color: %3;"
                       "  outline: none;"
                       "  selection-background-color: %4;"
                       "}"
                       "QAbstractItemView::item { height: 26px; padding-left: 6px;"
                       "  border-radius: 5px; }")
            .arg(c.fieldBackground.name(), c.fieldBorder.name(), c.textPrimary.name(),
                 c.primary.name()));

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_hoverAnimation->setDuration(Theme::durations().hover);
}

ModernComboBox::~ModernComboBox() = default;

void ModernComboBox::setLeadingIcon(const QString &iconName)
{
    m_leadingIcon = iconName;
    updateGeometry();
    update();
}

void ModernComboBox::setSwatchColor(const QColor &color)
{
    m_swatch = color;
    updateGeometry();
    update();
}

void ModernComboBox::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

QSize ModernComboBox::sizeHint() const
{
    const QFontMetrics fm(font());
    int width = kPaddingX * 2 + kChevronSize + 10;
    if (!m_leadingIcon.isEmpty() || m_swatch.isValid())
        width += kLeadSize + kLeadGap;

    int textWidth = 0;
    for (int i = 0; i < count(); ++i)
        textWidth = qMax(textWidth, fm.horizontalAdvance(itemText(i)));

    return QSize(width + textWidth, kHeight);
}

QSize ModernComboBox::minimumSizeHint() const
{
    return sizeHint();
}

void ModernComboBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const qreal radius = 8.0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QLinearGradient fill(body.topLeft(), body.bottomLeft());
    fill.setColorAt(0.0, Theme::mix(c.fieldBackground, c.fieldBackgroundHover, m_hoverProgress));
    fill.setColorAt(1.0, c.fieldBackground);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(body, radius, radius);

    // Focus is shown by lighting up the border itself. It must NOT be a ring
    // drawn outside `body`: painting is clipped to the widget rect, so an
    // outset rounded rect only survives at its four corners and shows up as
    // four stray coloured marks around the control.
    QColor edge = Theme::mix(c.fieldBorder, c.fieldBorderHover, m_hoverProgress);
    if (hasFocus())
        edge = c.fieldBorderFocus;

    QPen border(edge);
    border.setWidthF(hasFocus() ? 1.4 : 1.0);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, radius, radius);

    qreal x = kPaddingX;

    if (m_swatch.isValid()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_swatch);
        painter.drawEllipse(QPointF(x + kLeadSize / 2.0, body.center().y()), kLeadSize / 2.0 - 1,
                            kLeadSize / 2.0 - 1);
        x += kLeadSize + kLeadGap;
    } else if (!m_leadingIcon.isEmpty()) {
        const QPixmap glyph = IconProvider::pixmap(m_leadingIcon, kLeadSize, c.textSecondary,
                                                   devicePixelRatioF());
        if (!glyph.isNull()) {
            painter.drawPixmap(QPointF(x, body.center().y() - kLeadSize / 2.0), glyph);
            x += kLeadSize + kLeadGap;
        }
    }

    const qreal chevronLeft = body.right() - kPaddingX - kChevronSize;
    const qreal textWidth = qMax(10.0, chevronLeft - x - 8);

    const QFontMetrics fm(font());
    painter.setFont(font());
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(x, 0, textWidth, height()), Qt::AlignLeft | Qt::AlignVCenter,
                     fm.elidedText(currentText(), Qt::ElideRight, qRound(textWidth)));

    const QPixmap chevron = IconProvider::pixmap(QStringLiteral("chevron_down"), kChevronSize,
                                                 Theme::mix(c.textMuted, c.primaryBright,
                                                            m_hoverProgress),
                                                 devicePixelRatioF());
    if (!chevron.isNull())
        painter.drawPixmap(QPointF(chevronLeft, body.center().y() - kChevronSize / 2.0), chevron);
}

void ModernComboBox::enterEvent(QEnterEvent *event)
{
    QComboBox::enterEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->start();
}

void ModernComboBox::leaveEvent(QEvent *event)
{
    QComboBox::leaveEvent(event);
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(0.0);
    m_hoverAnimation->start();
}
