#include "ModernButton.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QtMath>

namespace {
/// Space reserved around the button body for its glow and drop shadow.
constexpr int kGlowPadding = 6;
/// Gap between the icon and the label.
constexpr int kIconGap = 12;

/// Shifts a colour's HSL lightness by \a amount (-1..1), keeping hue and
/// saturation. Used to lift the fill on hover and sink it while pressed.
QColor shiftLightness(const QColor &color, qreal amount)
{
    int h = 0, s = 0, l = 0, a = 0;
    color.getHsl(&h, &s, &l, &a);
    const int shifted = qBound(0, int(qRound(l + amount * 255.0)), 255);
    QColor out;
    out.setHsl(h, s, shifted, a);
    return out;
}

/// White at the given alpha - the workhorse for gloss and rim highlights.
inline QColor sheen(int alpha)
{
    return QColor(255, 255, 255, qBound(0, alpha, 255));
}
} // namespace

ModernButton::ModernButton(const QString &text, Variant variant, QWidget *parent)
    : QAbstractButton(parent)
    , m_variant(variant)
{
    setText(text);
    m_restingText = text;
    m_cornerRadius = Theme::metrics().controlRadius;

    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setFont(Theme::font(17, QFont::DemiBold));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_pressAnimation = new QPropertyAnimation(this, "pressProgress", this);
    m_pressAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_busyAnimation = new QPropertyAnimation(this, "busyProgress", this);
    m_busyAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_spinnerAnimation = new QVariantAnimation(this);
    m_spinnerAnimation->setStartValue(0.0);
    m_spinnerAnimation->setEndValue(360.0);
    m_spinnerAnimation->setDuration(Theme::durations().spinner);
    m_spinnerAnimation->setLoopCount(-1);
    m_spinnerAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(m_spinnerAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_spinnerAngle = value.toReal();
                update();
            });
}

ModernButton::~ModernButton() = default;

void ModernButton::setVariant(Variant variant)
{
    if (m_variant == variant)
        return;
    m_variant = variant;
    update();
}

void ModernButton::setIconName(const QString &iconName)
{
    if (m_iconName == iconName)
        return;
    m_iconName = iconName;
    updateGeometry();
    update();
}

void ModernButton::setTrailingIconName(const QString &iconName)
{
    if (m_trailingIconName == iconName)
        return;
    m_trailingIconName = iconName;
    updateGeometry();
    update();
}

void ModernButton::setIconPixelSize(int size)
{
    m_iconPixelSize = size;
    updateGeometry();
    update();
}

void ModernButton::setCornerRadius(int radius)
{
    m_cornerRadius = radius;
    update();
}

void ModernButton::setBusy(bool busy, const QString &busyText)
{
    if (m_busy == busy)
        return;

    m_busy = busy;
    if (busy) {
        m_restingText = text();
        m_busyText = busyText.isEmpty() ? tr("Please wait...") : busyText;
        setText(m_busyText);
        m_spinnerAnimation->start();
    } else {
        setText(m_restingText);
        m_spinnerAnimation->stop();
    }

    setCursor(busy ? Qt::BusyCursor : Qt::PointingHandCursor);
    animateTo(m_busyAnimation, m_busyProgress, busy ? 1.0 : 0.0, Theme::durations().state);
    update();
}

void ModernButton::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

void ModernButton::setPressProgress(qreal value)
{
    if (qFuzzyCompare(m_pressProgress, value))
        return;
    m_pressProgress = value;
    update();
}

void ModernButton::setBusyProgress(qreal value)
{
    if (qFuzzyCompare(m_busyProgress, value))
        return;
    m_busyProgress = value;
    update();
}

QSize ModernButton::sizeHint() const
{
    const QFontMetrics fm(font());
    int width = fm.horizontalAdvance(text()) + 72;
    if (!m_iconName.isEmpty())
        width += m_iconPixelSize + kIconGap;
    if (!m_trailingIconName.isEmpty())
        width += m_iconPixelSize + kIconGap;
    return QSize(width, Theme::metrics().buttonHeight + 2 * kGlowPadding);
}

QSize ModernButton::minimumSizeHint() const
{
    return QSize(160, Theme::metrics().buttonHeight + 2 * kGlowPadding);
}

void ModernButton::animateTo(QPropertyAnimation *animation, qreal current, qreal target,
                             int duration)
{
    animation->stop();
    animation->setDuration(duration);
    animation->setStartValue(current);
    animation->setEndValue(target);
    animation->start();
}

void ModernButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRectF body = QRectF(rect()).adjusted(kGlowPadding, kGlowPadding, -kGlowPadding, -kGlowPadding);

    // A barely perceptible squash while the button is held down.
    const qreal shrink = 1.5 * m_pressProgress;
    body = body.adjusted(shrink, shrink, -shrink, -shrink);

    if (!isEnabled())
        painter.setOpacity(0.45);

    if (m_variant == Variant::Primary)
        paintPrimary(painter, body, m_cornerRadius);
    else
        paintSecondary(painter, body, m_cornerRadius);

    if (hasFocus()) {
        QPen ring(Theme::colors().focusRing);
        ring.setWidthF(1.4);
        painter.setPen(ring);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(body.adjusted(-3, -3, 3, 3), m_cornerRadius + 3, m_cornerRadius + 3);
    }

    paintContent(painter, body);
}

void ModernButton::paintPrimary(QPainter &painter, const QRectF &body, qreal radius)
{
    const Theme::Palette &c = Theme::colors();
    const qreal hover = m_hoverProgress;
    const qreal press = m_pressProgress;

    // Every overlay below is filled through this one path, so each layer is
    // anti-aliased against the rounded silhouette. (Using setClipPath() instead
    // would give the overlays hard, non-anti-aliased corner edges.)
    QPainterPath shape;
    shape.addRoundedRect(body, radius, radius);

    painter.setPen(Qt::NoPen);

    // --- 1. Ambient drop shadow -------------------------------------------
    // Grounds the button on the panel. It sits slightly low and tightens while
    // the button is pressed, as though the button moves toward the surface.
    if (isEnabled()) {
        const qreal drop = 1.6 * (1.0 - press * 0.7);
        for (int i = kGlowPadding; i >= 1; --i) {
            const qreal t = qreal(i) / kGlowPadding;
            painter.setBrush(QColor(0, 0, 0, int(20.0 * (1.0 - t) * (1.0 - press * 0.5))));
            painter.drawRoundedRect(body.adjusted(-i * 0.7, -i * 0.3 + drop, i * 0.7, i + drop),
                                    radius + i, radius + i);
        }
    }

    // --- 2. Coloured bloom -------------------------------------------------
    // A blue halo that blooms on hover, so the control feels energised.
    if (isEnabled()) {
        const qreal strength = 0.5 + 0.9 * hover;
        painter.setBrush(Qt::NoBrush);
        for (int i = 1; i <= kGlowPadding; ++i) {
            QColor ring = c.primary;
            ring.setAlphaF(qBound(0.0, 0.17 * strength * (1.0 - qreal(i - 1) / kGlowPadding), 1.0));
            QPen pen(ring);
            pen.setWidthF(1.9);
            painter.setPen(pen);
            painter.drawRoundedRect(body.adjusted(-i, -i, i, i), radius + i, radius + i);
        }
        painter.setPen(Qt::NoPen);
    }

    // --- 3. Base fill ------------------------------------------------------
    // A vertical (not diagonal) ramp reads as a physical, lit surface. The pair
    // of stops either side of the midpoint gives the classic glossy "waist".
    const qreal lift = hover * 0.07 - press * 0.05;
    const QColor top = shiftLightness(c.primaryBright, lift + 0.04);
    const QColor mid = shiftLightness(c.primary, lift);
    const QColor deep = shiftLightness(c.primaryDeep, lift);

    QLinearGradient fill(body.topLeft(), body.bottomLeft());
    fill.setColorAt(0.00, top);
    fill.setColorAt(0.46, mid);
    fill.setColorAt(0.54, shiftLightness(mid, -0.025));
    fill.setColorAt(1.00, deep);
    painter.setBrush(fill);
    painter.drawPath(shape);

    // --- 4. Top gloss ------------------------------------------------------
    // The bright "glass" cap over the upper half, fading out before midway.
    QLinearGradient gloss(body.topLeft(), body.bottomLeft());
    gloss.setColorAt(0.00, sheen(int(72 + 34 * hover)));
    gloss.setColorAt(0.30, sheen(int(24 + 12 * hover)));
    gloss.setColorAt(0.55, sheen(0));
    gloss.setColorAt(1.00, sheen(0));
    painter.setBrush(gloss);
    painter.drawPath(shape);

    // --- 5. Specular sweep -------------------------------------------------
    // A soft diagonal band of light; barely visible at rest, catches on hover.
    QLinearGradient sweepLight(body.topLeft(), body.bottomRight());
    sweepLight.setColorAt(0.00, sheen(0));
    sweepLight.setColorAt(0.32, sheen(int(8 + 18 * hover)));
    sweepLight.setColorAt(0.52, sheen(0));
    sweepLight.setColorAt(1.00, sheen(0));
    painter.setBrush(sweepLight);
    painter.drawPath(shape);

    // --- 6. Grounded base --------------------------------------------------
    // Darkening at the very bottom gives the face its thickness.
    QLinearGradient depth(body.topLeft(), body.bottomLeft());
    depth.setColorAt(0.00, QColor(0, 0, 0, 0));
    depth.setColorAt(0.68, QColor(0, 0, 0, 0));
    depth.setColorAt(1.00, QColor(0, 0, 0, 46));
    painter.setBrush(depth);
    painter.drawPath(shape);

    // --- 7. Pressed inner shadow -------------------------------------------
    // While held, light falls from the top edge inward: the face looks recessed.
    if (press > 0.001) {
        QLinearGradient inset(body.topLeft(), body.bottomLeft());
        inset.setColorAt(0.00, QColor(0, 0, 0, int(66 * press)));
        inset.setColorAt(0.42, QColor(0, 0, 0, 0));
        inset.setColorAt(1.00, QColor(0, 0, 0, int(20 * press)));
        painter.setBrush(inset);
        painter.drawPath(shape);
    }

    // --- 8. Rim light ------------------------------------------------------
    // One stroke whose pen carries a gradient: lit along the top edge, shaded
    // along the bottom. This is what makes the edge read as a bevel.
    QLinearGradient edge(body.topLeft(), body.bottomLeft());
    edge.setColorAt(0.00, sheen(int(110 + 50 * hover)));
    edge.setColorAt(0.45, sheen(30));
    edge.setColorAt(1.00, QColor(0, 0, 0, 66));
    QPen rim(QBrush(edge), 1.0);
    painter.setPen(rim);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

void ModernButton::paintSecondary(QPainter &painter, const QRectF &body, qreal radius)
{
    const Theme::Palette &c = Theme::colors();
    const bool danger = m_variant == Variant::Danger;
    const QColor accent = danger ? c.danger : c.primary;
    const qreal hover = m_hoverProgress;
    const qreal press = m_pressProgress;

    QPainterPath shape;
    shape.addRoundedRect(body, radius, radius);

    painter.setPen(Qt::NoPen);

    // --- 1. Glass body -----------------------------------------------------
    // A faint dark pane even at rest, so the control has presence instead of
    // reading as an empty outline, warming toward the accent on hover.
    QColor restTop = QColor(255, 255, 255, 20);
    QColor restBottom = QColor(0, 0, 0, 30);
    QColor hotTop = accent;
    hotTop.setAlphaF(0.30 * hover);
    QColor hotBottom = accent;
    hotBottom.setAlphaF(0.12 * hover);

    QLinearGradient fill(body.topLeft(), body.bottomLeft());
    fill.setColorAt(0.0, Theme::mix(restTop, hotTop, hover));
    fill.setColorAt(1.0, Theme::mix(restBottom, hotBottom, hover));
    painter.setBrush(fill);
    painter.drawPath(shape);

    // --- 2. Top gloss ------------------------------------------------------
    QLinearGradient gloss(body.topLeft(), body.bottomLeft());
    gloss.setColorAt(0.00, sheen(int(26 + 26 * hover)));
    gloss.setColorAt(0.50, sheen(0));
    gloss.setColorAt(1.00, sheen(0));
    painter.setBrush(gloss);
    painter.drawPath(shape);

    // --- 3. Pressed inner shadow -------------------------------------------
    if (press > 0.001) {
        QLinearGradient inset(body.topLeft(), body.bottomLeft());
        inset.setColorAt(0.00, QColor(0, 0, 0, int(56 * press)));
        inset.setColorAt(0.45, QColor(0, 0, 0, 0));
        painter.setBrush(inset);
        painter.drawPath(shape);
    }

    // --- 4. Outer bloom on hover -------------------------------------------
    if (hover > 0.001 && isEnabled()) {
        painter.setBrush(Qt::NoBrush);
        for (int i = 1; i <= 3; ++i) {
            QColor ring = accent;
            ring.setAlphaF(qBound(0.0, 0.13 * hover * (1.0 - qreal(i - 1) / 3.0), 1.0));
            QPen glow(ring);
            glow.setWidthF(1.7);
            painter.setPen(glow);
            painter.drawRoundedRect(body.adjusted(-i, -i, i, i), radius + i, radius + i);
        }
    }

    // --- 5. Bevelled border ------------------------------------------------
    // Gradient pen again: catches light along the top, settles into shadow at
    // the bottom, and picks up the accent colour as the button lights up.
    const QColor base = danger ? Theme::alpha(c.danger, 150) : c.outlineButtonBorder;
    const QColor lit = Theme::mix(base, accent, hover);
    QLinearGradient edge(body.topLeft(), body.bottomLeft());
    edge.setColorAt(0.00, Theme::mix(lit, sheen(190), 0.35 + 0.25 * hover));
    edge.setColorAt(0.50, lit);
    edge.setColorAt(1.00, Theme::mix(lit, QColor(0, 0, 0, 200), 0.35));
    QPen border(QBrush(edge), 1.0 + 0.3 * hover);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    const qreal half = border.widthF() / 2.0;
    painter.drawRoundedRect(body.adjusted(half, half, -half, -half), radius, radius);
}

void ModernButton::paintContent(QPainter &painter, const QRectF &body)
{
    const Theme::Palette &c = Theme::colors();
    QColor contentColor;
    switch (m_variant) {
    case Variant::Primary:
        contentColor = c.textOnPrimary;
        break;
    case Variant::Danger:
        contentColor = Theme::mix(c.danger, QColor(0xFF, 0xFF, 0xFF), m_hoverProgress * 0.4);
        break;
    case Variant::Secondary:
        contentColor = Theme::mix(c.textPrimary, QColor(0xFF, 0xFF, 0xFF), m_hoverProgress);
        break;
    }

    const QFontMetrics fm(font());
    const QString label = text();
    const int textWidth = fm.horizontalAdvance(label);

    const bool showGlyph = m_busy || !m_iconName.isEmpty();
    const int glyphWidth = showGlyph ? m_iconPixelSize : 0;
    const int totalWidth = textWidth + (showGlyph ? glyphWidth + kIconGap : 0);

    qreal x = body.center().x() - totalWidth / 2.0;
    const qreal centerY = body.center().y();

    if (showGlyph) {
        const QRectF glyphRect(x, centerY - m_iconPixelSize / 2.0, m_iconPixelSize,
                               m_iconPixelSize);
        if (m_busy) {
            paintSpinner(painter, glyphRect);
        } else {
            const QPixmap glyph = IconProvider::pixmap(m_iconName, m_iconPixelSize, contentColor,
                                                       devicePixelRatioF());
            if (!glyph.isNull())
                painter.drawPixmap(glyphRect.topLeft(), glyph);
        }
        x += glyphWidth + kIconGap;
    }

    painter.setFont(font());
    const QRectF textRect(x, body.top(), textWidth + 2, body.height());

    // On the filled variant the label sits on a bright gradient, so a whisper of
    // shadow underneath keeps it crisp rather than glowing into the background.
    if (m_variant == Variant::Primary && isEnabled()) {
        painter.setPen(QColor(0, 0, 0, 58));
        painter.drawText(textRect.translated(0, 1), Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    painter.setPen(contentColor);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label);

    if (!m_trailingIconName.isEmpty() && !m_busy) {
        const QPixmap trailing = IconProvider::pixmap(m_trailingIconName, m_iconPixelSize,
                                                      contentColor, devicePixelRatioF());
        if (!trailing.isNull()) {
            painter.drawPixmap(QPointF(body.right() - 22 - m_iconPixelSize,
                                       body.center().y() - m_iconPixelSize / 2.0),
                               trailing);
        }
    }
}

void ModernButton::paintSpinner(QPainter &painter, const QRectF &box)
{
    const QColor spinnerColor = m_variant == Variant::Primary ? Theme::colors().textOnPrimary
                                                              : Theme::colors().primary;

    painter.save();
    painter.translate(box.center());
    painter.rotate(m_spinnerAngle);

    const qreal r = box.width() / 2.0 - 1.5;
    const QRectF arcRect(-r, -r, 2 * r, 2 * r);

    QPen track(Theme::alpha(spinnerColor, 60));
    track.setWidthF(2.2);
    track.setCapStyle(Qt::RoundCap);
    painter.setPen(track);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(arcRect);

    QPen head(spinnerColor);
    head.setWidthF(2.2);
    head.setCapStyle(Qt::RoundCap);
    painter.setPen(head);
    painter.drawArc(arcRect, 90 * 16, -110 * 16);

    painter.restore();
}

void ModernButton::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    if (isEnabled() && !m_busy)
        animateTo(m_hoverAnimation, m_hoverProgress, 1.0, Theme::durations().hover);
}

void ModernButton::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    animateTo(m_hoverAnimation, m_hoverProgress, 0.0, Theme::durations().hover);
}

void ModernButton::mousePressEvent(QMouseEvent *event)
{
    QAbstractButton::mousePressEvent(event);
    if (isEnabled() && !m_busy)
        animateTo(m_pressAnimation, m_pressProgress, 1.0, Theme::durations().press);
}

void ModernButton::mouseReleaseEvent(QMouseEvent *event)
{
    QAbstractButton::mouseReleaseEvent(event);
    animateTo(m_pressAnimation, m_pressProgress, 0.0, Theme::durations().press);
}

void ModernButton::changeEvent(QEvent *event)
{
    QAbstractButton::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled()) {
            m_hoverAnimation->stop();
            setHoverProgress(0.0);
            m_pressAnimation->stop();
            setPressProgress(0.0);
        }
        update();
    }
}

void ModernButton::focusInEvent(QFocusEvent *event)
{
    QAbstractButton::focusInEvent(event);
    update();
}

void ModernButton::focusOutEvent(QFocusEvent *event)
{
    QAbstractButton::focusOutEvent(event);
    update();
}
