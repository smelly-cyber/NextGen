#include "DonutGauge.h"

#include "Theme.h"

#include <QConicalGradient>
#include <QFontMetrics>
#include <QPainter>
#include <QPropertyAnimation>

DonutGauge::DonutGauge(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_animation = new QPropertyAnimation(this, "displayValue", this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    m_animation->setDuration(600);
}

DonutGauge::~DonutGauge() = default;

void DonutGauge::setValue(qreal percent)
{
    m_value = percent;

    const qreal target = percent < 0.0 ? 0.0 : qBound(0.0, percent, 100.0);
    m_animation->stop();
    m_animation->setStartValue(m_displayValue);
    m_animation->setEndValue(target);
    m_animation->start();

    update();
}

void DonutGauge::setUnit(const QString &unit)
{
    m_unit = unit;
    update();
}

void DonutGauge::setShowUnit(bool show)
{
    m_showUnit = show;
    update();
}

void DonutGauge::setCentreText(const QString &text)
{
    if (m_centreText == text)
        return;
    m_centreText = text;
    update();
}

void DonutGauge::setRingThickness(qreal thickness)
{
    m_ringThickness = thickness;
    update();
}

void DonutGauge::setValueFontSize(int pixelSize)
{
    m_valueFontSize = pixelSize;
    update();
}

void DonutGauge::setDisplayValue(qreal value)
{
    if (qFuzzyCompare(m_displayValue, value))
        return;
    m_displayValue = value;
    update();
}

QSize DonutGauge::sizeHint() const
{
    return QSize(76, 76);
}

void DonutGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const bool available = m_value >= 0.0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal inset = m_ringThickness / 2.0 + 1.0;
    const QRectF ring = QRectF(rect()).adjusted(inset, inset, -inset, -inset);

    // --- Track --------------------------------------------------------------
    QPen track(c.gaugeTrack);
    track.setWidthF(m_ringThickness);
    track.setCapStyle(Qt::RoundCap);
    painter.setPen(track);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(ring);

    // --- Value arc ----------------------------------------------------------
    if (available && m_displayValue > 0.2) {
        QConicalGradient sweep(ring.center(), 90);
        sweep.setColorAt(0.0, c.cyan);
        sweep.setColorAt(0.35, c.primary);
        sweep.setColorAt(1.0, c.primaryDeep);

        QPen arc(QBrush(sweep), m_ringThickness);
        arc.setCapStyle(Qt::RoundCap);
        painter.setPen(arc);
        painter.drawArc(ring, 90 * 16, -qRound(360.0 * 16.0 * m_displayValue / 100.0));
    }

    // --- Centre label -------------------------------------------------------
    const QFont valueFont = Theme::font(m_valueFontSize, QFont::Bold);
    const QFontMetrics valueMetrics(valueFont);

    if (!available) {
        painter.setFont(valueFont);
        painter.setPen(c.textMuted);
        painter.drawText(QRectF(rect()), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    if (!m_centreText.isEmpty()) {
        const QFont textFont = Theme::font(qMax(10, m_valueFontSize - 6), QFont::DemiBold);
        painter.setFont(textFont);
        painter.setPen(c.textPrimary);
        painter.drawText(QRectF(rect()).adjusted(4, 0, -4, 0), Qt::AlignCenter, m_centreText);
        return;
    }

    const QString number = QString::number(qRound(m_displayValue));
    const QFont unitFont = Theme::font(qMax(10, m_valueFontSize - 8), QFont::Medium);
    const QFontMetrics unitMetrics(unitFont);

    const int numberWidth = valueMetrics.horizontalAdvance(number);
    const int unitWidth = m_showUnit ? unitMetrics.horizontalAdvance(m_unit) + 1 : 0;
    const qreal totalWidth = numberWidth + unitWidth;

    qreal x = rect().center().x() - totalWidth / 2.0;
    const qreal centreY = rect().center().y();

    painter.setFont(valueFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(x, centreY - valueMetrics.height() / 2.0, numberWidth + 2,
                            valueMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, number);

    if (m_showUnit) {
        x += numberWidth + 1;
        painter.setFont(unitFont);
        painter.setPen(c.textSecondary);
        painter.drawText(QRectF(x, centreY - valueMetrics.height() / 2.0, unitWidth + 2,
                                valueMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_unit);
    }
}
