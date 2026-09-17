#include "ColorPickerDialog.h"

#include "IconProvider.h"
#include "ModernButton.h"
#include "ModernLineEdit.h"
#include "Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kAreaSize = 240;
constexpr int kStripHeight = 16;
constexpr qreal kAreaRadius = 10.0;

/// Quick picks: the four original accents plus a few more.
const QColor kPresets[] = {
    QColor(0x1E, 0x88, 0xFF), QColor(0x2E, 0xD3, 0xF0), QColor(0xA8, 0x6C, 0xF0),
    QColor(0x35, 0xD0, 0x8A), QColor(0xFF, 0x4D, 0x5E), QColor(0xFF, 0x8A, 0x3D),
    QColor(0xFF, 0x5F, 0xB0), QColor(0xF5, 0xC5, 0x42),
};

/// Draws the white-ringed circle used as the drag handle.
void drawHandle(QPainter &painter, const QPointF &centre, const QColor &fill)
{
    painter.setBrush(fill);
    painter.setPen(QPen(QColor(0, 0, 0, 90), 3.0));
    painter.drawEllipse(centre, 8.5, 8.5);
    painter.setPen(QPen(Qt::white, 2.0));
    painter.drawEllipse(centre, 8.0, 8.0);
}

} // namespace

// ----------------------------------------------------- SaturationValueArea --

SaturationValueArea::SaturationValueArea(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(kAreaSize, kAreaSize);
    setCursor(Qt::CrossCursor);
}

void SaturationValueArea::setHue(int hue)
{
    if (hue == m_hue)
        return;
    m_hue = hue;
    m_cache = QImage();
    update();
}

void SaturationValueArea::setSaturationValue(int saturation, int value)
{
    m_saturation = saturation;
    m_value = value;
    update();
}

void SaturationValueArea::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_cache = QImage();
}

void SaturationValueArea::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if (m_cache.isNull()) {
        // Pure hue left-to-right from white, then darkened top-to-bottom. Two
        // gradients composed this way are exactly the HSV square.
        const qreal dpr = devicePixelRatioF();
        m_cache = QImage(size() * dpr, QImage::Format_ARGB32_Premultiplied);
        m_cache.setDevicePixelRatio(dpr);
        m_cache.fill(Qt::transparent);

        QPainter p(&m_cache);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()), kAreaRadius, kAreaRadius);

        QLinearGradient horizontal(0, 0, width(), 0);
        horizontal.setColorAt(0.0, Qt::white);
        horizontal.setColorAt(1.0, QColor::fromHsv(m_hue, 255, 255));
        p.fillPath(clip, horizontal);

        QLinearGradient vertical(0, 0, 0, height());
        vertical.setColorAt(0.0, QColor(0, 0, 0, 0));
        vertical.setColorAt(1.0, QColor(0, 0, 0, 255));
        p.fillPath(clip, vertical);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawImage(0, 0, m_cache);

    painter.setPen(QPen(Theme::colors().fieldBorder, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kAreaRadius,
                            kAreaRadius);

    const QPointF handle(m_saturation / 255.0 * width(), (1.0 - m_value / 255.0) * height());
    drawHandle(painter, handle, QColor::fromHsv(m_hue, m_saturation, m_value));
}

void SaturationValueArea::pickAt(const QPointF &position)
{
    const qreal x = qBound(0.0, position.x() / width(), 1.0);
    const qreal y = qBound(0.0, position.y() / height(), 1.0);
    m_saturation = qRound(x * 255);
    m_value = qRound((1.0 - y) * 255);
    update();
    emit picked(m_saturation, m_value);
}

void SaturationValueArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        pickAt(event->position());
}

void SaturationValueArea::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        pickAt(event->position());
}

// ---------------------------------------------------------------- HueStrip --

HueStrip::HueStrip(QWidget *parent)
    : QWidget(parent)
{
    // Extra height around the strip leaves room for the handle.
    setFixedSize(kAreaSize, kStripHeight + 8);
    setCursor(Qt::PointingHandCursor);
}

void HueStrip::setHue(int hue)
{
    m_hue = hue;
    update();
}

void HueStrip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF strip(0, (height() - kStripHeight) / 2.0, width(), kStripHeight);
    QLinearGradient gradient(strip.topLeft(), strip.topRight());
    for (int i = 0; i <= 6; ++i)
        gradient.setColorAt(i / 6.0, QColor::fromHsv(qMin(359, i * 60), 255, 255));

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(strip, kStripHeight / 2.0, kStripHeight / 2.0);

    // Keep the handle fully inside the widget at both ends.
    const qreal inset = 9.0;
    const qreal x = inset + m_hue / 359.0 * (width() - 2 * inset);
    drawHandle(painter, QPointF(x, height() / 2.0), QColor::fromHsv(m_hue, 255, 255));
}

void HueStrip::pickAt(qreal x)
{
    const qreal inset = 9.0;
    const qreal t = qBound(0.0, (x - inset) / (width() - 2 * inset), 1.0);
    m_hue = qRound(t * 359);
    update();
    emit picked(m_hue);
}

void HueStrip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        pickAt(event->position().x());
}

void HueStrip::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        pickAt(event->position().x());
}

// ------------------------------------------------------- ColorPickerDialog --

ColorPickerDialog::ColorPickerDialog(const QColor &initial, QWidget *parent)
    : FramelessDialog(parent)
{
    const Theme::Palette &c = Theme::colors();

    setHeaderTitle(tr("Accent Colour"));

    m_area = new SaturationValueArea(this);
    m_hue = new HueStrip(this);

    // --- Side panel: preview, hex, presets ----------------------------------
    m_preview = new QWidget(this);
    m_preview->setFixedSize(120, 72);

    auto *hexLabel = new QLabel(tr("Hex"), this);
    hexLabel->setFont(Theme::font(12, QFont::DemiBold));
    hexLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    m_hex = new ModernLineEdit(this);
    m_hex->setPlaceholderText(QStringLiteral("#1E88FF"));
    m_hex->setFixedWidth(140);

    auto *presetLabel = new QLabel(tr("Presets"), this);
    presetLabel->setFont(Theme::font(12, QFont::DemiBold));
    presetLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    auto *presetGrid = new QGridLayout;
    presetGrid->setContentsMargins(0, 0, 0, 0);
    presetGrid->setSpacing(8);
    int index = 0;
    for (const QColor &preset : kPresets) {
        auto *swatch = new QToolButton(this);
        swatch->setFixedSize(26, 26);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setFocusPolicy(Qt::NoFocus);
        swatch->setToolTip(preset.name(QColor::HexRgb).toUpper());
        swatch->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 1px solid "
                                             "rgba(255,255,255,40); border-radius: 13px; }"
                                             "QToolButton:hover { border: 2px solid white; }")
                                  .arg(preset.name()));
        connect(swatch, &QToolButton::clicked, this, [this, preset] { setColor(preset); });
        presetGrid->addWidget(swatch, index / 4, index % 4);
        ++index;
    }

    auto *side = new QVBoxLayout;
    side->setContentsMargins(0, 0, 0, 0);
    side->setSpacing(6);
    side->addWidget(m_preview);
    side->addSpacing(10);
    side->addWidget(hexLabel);
    side->addWidget(m_hex);
    side->addSpacing(10);
    side->addWidget(presetLabel);
    side->addLayout(presetGrid);
    side->addStretch(1);

    auto *pickers = new QVBoxLayout;
    pickers->setContentsMargins(0, 0, 0, 0);
    pickers->setSpacing(14);
    pickers->addWidget(m_area);
    pickers->addWidget(m_hue);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(22);
    body->addLayout(pickers);
    body->addLayout(side);

    // --- Buttons ------------------------------------------------------------
    auto *cancel = new ModernButton(tr("Cancel"), ModernButton::Variant::Secondary, this);
    auto *apply = new ModernButton(tr("Apply"), ModernButton::Variant::Primary, this);
    for (ModernButton *button : {cancel, apply}) {
        button->setFont(Theme::font(14, QFont::Medium));
        button->setFixedHeight(44);
        button->setMinimumWidth(120);
    }
    connect(cancel, &ModernButton::clicked, this, &QDialog::reject);
    connect(apply, &ModernButton::clicked, this, &QDialog::accept);

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(apply);

    contentLayout()->addLayout(body);
    contentLayout()->addSpacing(20);
    contentLayout()->addLayout(buttons);

    // --- Wiring -------------------------------------------------------------
    connect(m_area, &SaturationValueArea::picked, this, [this](int s, int v) {
        setColor(QColor::fromHsv(m_hue->hue(), s, v));
    });
    connect(m_hue, &HueStrip::picked, this, [this](int h) {
        setColor(QColor::fromHsv(h, m_color.hsvSaturation(), m_color.value()));
    });
    connect(m_hex, &ModernLineEdit::textChanged, this, [this](const QString &text) {
        static const QRegularExpression pattern(QStringLiteral("^#?[0-9A-Fa-f]{6}$"));
        const QString trimmed = text.trimmed();
        if (!pattern.match(trimmed).hasMatch())
            return;
        const QColor parsed(trimmed.startsWith(QLatin1Char('#')) ? trimmed
                                                                  : QLatin1Char('#') + trimmed);
        if (parsed.isValid() && parsed != m_color)
            setColor(parsed, /*updateHexField=*/false);
    });

    setColor(initial.isValid() ? initial : kPresets[0]);
}

void ColorPickerDialog::setColor(const QColor &color, bool updateHexField)
{
    // Greys have no hue of their own; keep the strip where it was rather than
    // snapping it back to red.
    int hue = color.hsvHue();
    if (hue < 0)
        hue = m_hue->hue();

    m_color = QColor::fromHsv(hue, color.hsvSaturation(), color.value());

    m_area->setHue(hue);
    m_area->setSaturationValue(m_color.hsvSaturation(), m_color.value());
    m_hue->setHue(hue);

    m_preview->setStyleSheet(
        QStringLiteral("background: %1; border-radius: 10px; border: 1px solid rgba(255,255,255,40);")
            .arg(m_color.name()));

    if (updateHexField) {
        const QSignalBlocker block(m_hex);
        m_hex->setText(m_color.name(QColor::HexRgb).toUpper());
    }
}

QColor ColorPickerDialog::getColor(const QColor &initial, QWidget *parent)
{
    ColorPickerDialog dialog(initial, parent);
    if (dialog.exec() != QDialog::Accepted)
        return QColor();
    return dialog.color();
}

// ------------------------------------------------------- ColorSwatchButton --

ColorSwatchButton::ColorSwatchButton(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void ColorSwatchButton::setColor(const QColor &color)
{
    m_color = color;
    update();
}

QSize ColorSwatchButton::sizeHint() const
{
    // Same height as the combo boxes on the page.
    return QSize(130, 36);
}

void ColorSwatchButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const qreal radius = 8.0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_hovered ? c.fieldBackgroundHover : c.fieldBackground);
    painter.drawRoundedRect(body, radius, radius);

    QColor edge = m_hovered ? c.fieldBorderHover : c.fieldBorder;
    if (hasFocus())
        edge = c.fieldBorderFocus;
    painter.setPen(QPen(edge, hasFocus() ? 1.4 : 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, radius, radius);

    // Colour dot.
    const QPointF dot(20.0, height() / 2.0);
    painter.setPen(QPen(QColor(255, 255, 255, 50), 1.0));
    painter.setBrush(m_color.isValid() ? m_color : c.primary);
    painter.drawEllipse(dot, 7.0, 7.0);

    // Hex code.
    painter.setFont(Theme::font(13, QFont::Medium));
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(36, 0, width() - 36 - 30, height()), Qt::AlignLeft | Qt::AlignVCenter,
                     m_color.name(QColor::HexRgb).toUpper());

    // Trailing icon hinting that it opens something.
    const QPixmap glyph = IconProvider::pixmap(QStringLiteral("chevron_right"), 14,
                                               c.textSecondary, devicePixelRatioF());
    if (!glyph.isNull())
        painter.drawPixmap(QPointF(width() - 24, height() / 2.0 - 7), glyph);
}

void ColorSwatchButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void ColorSwatchButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void ColorSwatchButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit clicked();
    QWidget::mouseReleaseEvent(event);
}

void ColorSwatchButton::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
        || event->key() == Qt::Key_Enter) {
        emit clicked();
        return;
    }
    QWidget::keyPressEvent(event);
}
