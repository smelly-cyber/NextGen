#include "ModernLineEdit.h"

#include "IconButton.h"
#include "IconProvider.h"
#include "Theme.h"

#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>

namespace {
/// Space reserved around the field so the focus glow is not clipped.
constexpr int kGlowPadding = 5;
/// Distance from the field edge to the centre of the leading icon.
constexpr int kIconInset = 22;
/// Distance from the field edge to the first text pixel, when a leading icon
/// has reserved room for itself.
constexpr int kTextInset = 54;
/// Left padding when there is NO leading icon. Without this the text used to
/// start 54px in regardless, leaving a gap where an icon would have been and
/// making the placeholder look centred rather than aligned to the field.
constexpr int kPlainTextInset = 18;

// Compact variants, used by the admin console.
constexpr int kCompactFieldHeight = 28;
constexpr int kCompactIconInset = 17;
constexpr int kCompactTextInset = 34;
constexpr int kCompactPlainInset = 14;
} // namespace

ModernLineEdit::ModernLineEdit(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::StrongFocus);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setFrame(false);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_lineEdit->setFont(Theme::font(15));
    m_lineEdit->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; }"));
    m_lineEdit->installEventFilter(this);
    applyInnerPalette();

    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(8);
    layout->addWidget(m_lineEdit, 1);
    updateTextInset();

    // Keyboard focus lands straight on the editor.
    setFocusProxy(m_lineEdit);

    m_hoverAnimation = new QPropertyAnimation(this, "hoverProgress", this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_focusAnimation = new QPropertyAnimation(this, "focusProgress", this);
    m_focusAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_errorAnimation = new QPropertyAnimation(this, "errorProgress", this);
    m_errorAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_lineEdit, &QLineEdit::textChanged, this, [this](const QString &value) {
        if (m_errorState)
            setErrorState(false);
        emit textChanged(value);
    });
    connect(m_lineEdit, &QLineEdit::returnPressed, this, &ModernLineEdit::returnPressed);
}

ModernLineEdit::~ModernLineEdit() = default;

void ModernLineEdit::applyInnerPalette()
{
    const Theme::Palette &c = Theme::colors();
    QPalette pal = m_lineEdit->palette();
    pal.setColor(QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::PlaceholderText, c.textMuted);
    pal.setColor(QPalette::Highlight, c.selectionBackground);
    pal.setColor(QPalette::HighlightedText, c.textPrimary);
    m_lineEdit->setPalette(pal);
}

void ModernLineEdit::setLeadingIcon(const QString &iconName)
{
    m_leadingIcon = iconName;
    updateTextInset();
    update();
}

void ModernLineEdit::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    m_lineEdit->setFont(Theme::font(compact ? 13 : 15));
    updateTextInset();
    updateGeometry();
    update();
}

void ModernLineEdit::updateTextInset()
{
    auto *box = layout();
    if (!box)
        return;

    if (m_leadingIcon.isEmpty()) {
        // No icon to sit beside, so the text is centred in the field. Equal
        // margins on both sides are what make it land in the true centre.
        const int side = kGlowPadding + (m_compact ? kCompactPlainInset : kPlainTextInset);
        box->setContentsMargins(side, kGlowPadding, side, kGlowPadding);
        m_lineEdit->setAlignment(Qt::AlignCenter);
    } else {
        // With an icon the text starts just after it and stays left aligned.
        box->setContentsMargins(kGlowPadding + (m_compact ? kCompactTextInset : kTextInset),
                                kGlowPadding, kGlowPadding + 14, kGlowPadding);
        m_lineEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
}

void ModernLineEdit::setPlaceholderText(const QString &text)
{
    m_lineEdit->setPlaceholderText(text);
}

QString ModernLineEdit::placeholderText() const
{
    return m_lineEdit->placeholderText();
}

QString ModernLineEdit::text() const
{
    return m_lineEdit->text();
}

void ModernLineEdit::setText(const QString &text)
{
    m_lineEdit->setText(text);
}

void ModernLineEdit::setEchoMode(QLineEdit::EchoMode mode)
{
    m_lineEdit->setEchoMode(mode);
    m_passwordVisible = (mode == QLineEdit::Normal);
    if (m_passwordToggle) {
        m_passwordToggle->setIconName(m_passwordVisible ? QStringLiteral("eye")
                                                        : QStringLiteral("eye_off"));
    }
}

QLineEdit::EchoMode ModernLineEdit::echoMode() const
{
    return m_lineEdit->echoMode();
}

void ModernLineEdit::setPasswordToggleEnabled(bool enabled)
{
    if (enabled == (m_passwordToggle != nullptr))
        return;

    if (!enabled) {
        delete m_passwordToggle;
        m_passwordToggle = nullptr;
        return;
    }

    const Theme::Palette &c = Theme::colors();
    m_passwordToggle = new IconButton(m_passwordVisible ? QStringLiteral("eye")
                                                        : QStringLiteral("eye_off"),
                                      this);
    m_passwordToggle->setIconSize(20);
    m_passwordToggle->setFixedSize(34, 34);
    m_passwordToggle->setCornerRadius(8);
    m_passwordToggle->setColors(c.fieldIcon, c.fieldIconActive);
    m_passwordToggle->setHoverBackground(Theme::alpha(c.primary, 34));
    m_passwordToggle->setToolTip(tr("Show or hide the password"));
    m_passwordToggle->setAccessibleName(tr("Toggle password visibility"));

    connect(m_passwordToggle, &IconButton::clicked, this, &ModernLineEdit::togglePasswordVisibility);

    if (auto *box = qobject_cast<QHBoxLayout *>(layout())) {
        box->setContentsMargins(kGlowPadding + kTextInset, kGlowPadding, kGlowPadding + 10,
                                kGlowPadding);
        box->addWidget(m_passwordToggle, 0, Qt::AlignVCenter);
    }
}

void ModernLineEdit::togglePasswordVisibility()
{
    m_passwordVisible = !m_passwordVisible;
    m_lineEdit->setEchoMode(m_passwordVisible ? QLineEdit::Normal : QLineEdit::Password);
    m_passwordToggle->setIconName(m_passwordVisible ? QStringLiteral("eye")
                                                    : QStringLiteral("eye_off"));
    emit passwordVisibilityChanged(m_passwordVisible);
}

void ModernLineEdit::setErrorState(bool error)
{
    if (m_errorState == error)
        return;
    m_errorState = error;
    animateTo(m_errorAnimation, m_errorProgress, error ? 1.0 : 0.0, Theme::durations().state);
}

void ModernLineEdit::shake()
{
    if (m_shakeAnimation) {
        m_shakeAnimation->stop();
        delete m_shakeAnimation;
        m_shakeAnimation = nullptr;
    }

    const QPoint origin = pos();
    const int offsets[] = {7, -6, 4, -3, 0};

    m_shakeAnimation = new QSequentialAnimationGroup(this);
    QPoint from = origin;
    for (int dx : offsets) {
        auto *step = new QPropertyAnimation(this, "pos");
        step->setDuration(48);
        step->setStartValue(from);
        from = origin + QPoint(dx, 0);
        step->setEndValue(from);
        step->setEasingCurve(QEasingCurve::OutQuad);
        m_shakeAnimation->addAnimation(step);
    }
    m_shakeAnimation->start();
}

void ModernLineEdit::updateHoverState()
{
    // Qt sends a Leave event to this widget as soon as the cursor moves onto the
    // embedded editor, so hover is derived from the real cursor position instead.
    const bool hovered = rect().contains(mapFromGlobal(QCursor::pos()));
    animateTo(m_hoverAnimation, m_hoverProgress, hovered ? 1.0 : 0.0, Theme::durations().hover);
}

void ModernLineEdit::setHoverProgress(qreal value)
{
    if (qFuzzyCompare(m_hoverProgress, value))
        return;
    m_hoverProgress = value;
    update();
}

void ModernLineEdit::setFocusProgress(qreal value)
{
    if (qFuzzyCompare(m_focusProgress, value))
        return;
    m_focusProgress = value;
    update();
}

void ModernLineEdit::setErrorProgress(qreal value)
{
    if (qFuzzyCompare(m_errorProgress, value))
        return;
    m_errorProgress = value;
    update();
}

QSize ModernLineEdit::sizeHint() const
{
    const int height = m_compact ? kCompactFieldHeight : Theme::metrics().fieldHeight;
    return QSize(m_compact ? 200 : 320, height + 2 * kGlowPadding);
}

QSize ModernLineEdit::minimumSizeHint() const
{
    const int height = m_compact ? kCompactFieldHeight : Theme::metrics().fieldHeight;
    return QSize(m_compact ? 140 : 220, height + 2 * kGlowPadding);
}

void ModernLineEdit::animateTo(QPropertyAnimation *animation, qreal current, qreal target,
                               int duration)
{
    animation->stop();
    animation->setDuration(duration);
    animation->setStartValue(current);
    animation->setEndValue(target);
    animation->start();
}

void ModernLineEdit::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const int radius = Theme::metrics().controlRadius;
    const QRectF field = QRectF(rect()).adjusted(kGlowPadding, kGlowPadding,
                                                 -kGlowPadding, -kGlowPadding);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // --- Outer glow -------------------------------------------------------
    const qreal glowStrength = qMax(m_focusProgress, m_errorProgress);
    if (glowStrength > 0.001) {
        const QColor glowColor = m_errorProgress > m_focusProgress ? c.danger : c.primary;
        painter.setBrush(Qt::NoBrush);
        for (int i = 1; i <= kGlowPadding; ++i) {
            QColor ring = glowColor;
            ring.setAlphaF(0.20 * glowStrength * (1.0 - qreal(i - 1) / kGlowPadding));
            QPen pen(ring);
            pen.setWidthF(1.6);
            painter.setPen(pen);
            painter.drawRoundedRect(field.adjusted(-i, -i, i, i), radius + i, radius + i);
        }
    }

    // --- Fill -------------------------------------------------------------
    const qreal lift = qBound(0.0, m_hoverProgress * 0.7 + m_focusProgress * 0.5, 1.0);
    QLinearGradient fill(field.topLeft(), field.bottomLeft());
    fill.setColorAt(0.0, Theme::mix(c.fieldBackground, c.fieldBackgroundHover, lift));
    fill.setColorAt(1.0, Theme::mix(c.fieldBackground.darker(112), c.fieldBackgroundHover, lift * 0.6));
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(field, radius, radius);

    // --- Border -----------------------------------------------------------
    QColor border = Theme::mix(c.fieldBorder, c.fieldBorderHover, m_hoverProgress);
    border = Theme::mix(border, c.fieldBorderFocus, m_focusProgress);
    border = Theme::mix(border, c.danger, m_errorProgress);

    QPen borderPen(border);
    borderPen.setWidthF(1.0 + 0.4 * qMax(m_focusProgress, m_errorProgress));
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    const qreal half = borderPen.widthF() / 2.0;
    painter.drawRoundedRect(field.adjusted(half, half, -half, -half), radius, radius);

    // --- Leading icon -----------------------------------------------------
    if (!m_leadingIcon.isEmpty()) {
        const int iconSize = m_compact ? 16 : Theme::metrics().iconSize;
        QColor iconColor = Theme::mix(c.fieldIcon, c.fieldIconActive, m_focusProgress);
        iconColor = Theme::mix(iconColor, c.danger, m_errorProgress);

        const QPixmap glyph = IconProvider::pixmap(m_leadingIcon, iconSize, iconColor,
                                                   devicePixelRatioF());
        if (!glyph.isNull()) {
            const QPointF at(field.left() + (m_compact ? kCompactIconInset : kIconInset)
                                 - iconSize / 2.0,
                             field.center().y() - iconSize / 2.0);
            painter.drawPixmap(at, glyph);
        }
    }
}

void ModernLineEdit::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    updateHoverState();
}

void ModernLineEdit::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    updateHoverState();
}

bool ModernLineEdit::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_lineEdit) {
        switch (event->type()) {
        case QEvent::FocusIn:
            animateTo(m_focusAnimation, m_focusProgress, 1.0, Theme::durations().focus);
            break;
        case QEvent::FocusOut:
            animateTo(m_focusAnimation, m_focusProgress, 0.0, Theme::durations().focus);
            break;
        case QEvent::Enter:
        case QEvent::Leave:
            updateHoverState();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}
