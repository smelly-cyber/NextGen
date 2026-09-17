#include "CustomTitleBar.h"

#include "IconButton.h"
#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSvgRenderer>
#include <QWindow>

namespace {
/// Region of the 1024x1024 logo artwork that contains just the NT monogram.
/// Measured from the asset; used as a viewBox override so the mark can be drawn
/// small and sharp without the wordmark turning into a smudge.
const QRectF kMarkViewBox(232.0, 196.0, 648.0, 360.0);

const QLatin1String kMarkSvg(":/assets/logo/nextgen_tweaks_mark.svg");
const QLatin1String kMarkPng(":/assets/logo/nextgen_tweaks_mark.png");
const QLatin1String kLogoSvg(":/assets/logo/nextgen_tweaks_logo.svg");
const QLatin1String kLogoPng(":/assets/logo/nextgen_tweaks_logo.png");
} // namespace

// --------------------------------------------------------------- BrandMark --

BrandMark::BrandMark(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Preference order:
    //   1. a dedicated NT mark (mark.svg / mark.png) - drawn whole,
    //   2. the full logo SVG - cropped to just the monogram via a viewBox,
    //   3. the full logo PNG - drawn whole (last-resort, may look busy).
    QFile markSvg{QString(kMarkSvg)};
    if (markSvg.exists() && markSvg.open(QIODevice::ReadOnly)) {
        m_svg = markSvg.readAll();
        m_cropToMonogram = false;
        return;
    }

    m_logo = QPixmap(QString(kMarkPng));
    if (!m_logo.isNull())
        return;

    QFile logoSvg{QString(kLogoSvg)};
    if (logoSvg.exists() && logoSvg.open(QIODevice::ReadOnly)) {
        m_svg = logoSvg.readAll();
        m_cropToMonogram = true;
        return;
    }
    m_logo = QPixmap(QString(kLogoPng));
}

void BrandMark::setMarkHeight(int height)
{
    m_markHeight = height;
    updateGeometry();
    update();
}

QSize BrandMark::sizeHint() const
{
    if (m_svg.isEmpty() && !m_logo.isNull()) {
        const qreal aspect = qreal(m_logo.width()) / qreal(m_logo.height());
        return QSize(qRound(m_markHeight * aspect), m_markHeight);
    }
    const qreal aspect = kMarkViewBox.width() / kMarkViewBox.height();
    return QSize(qRound(m_markHeight * aspect), m_markHeight);
}

void BrandMark::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!m_svg.isEmpty()) {
        QSvgRenderer renderer(m_svg);
        if (!renderer.isValid())
            return;
        // A full logo is cropped to its monogram; a dedicated mark is drawn whole.
        if (m_cropToMonogram)
            renderer.setViewBox(kMarkViewBox);
        renderer.setAspectRatioMode(Qt::KeepAspectRatio);
        renderer.render(&painter, QRectF(rect()));
        return;
    }

    if (!m_logo.isNull()) {
        const qreal dpr = devicePixelRatioF();
        QPixmap scaled = m_logo.scaled(QSize(qRound(width() * dpr), qRound(height() * dpr)),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        const QPointF at((width() - scaled.width() / dpr) / 2.0,
                         (height() - scaled.height() / dpr) / 2.0);
        painter.drawPixmap(at, scaled);
    }
}

// ---------------------------------------------------------- CustomTitleBar --

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent)
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    setFixedHeight(m.titleBarHeight);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    m_back = new IconButton(QStringLiteral("chevron_left"), this);
    m_back->setFixedSize(m.windowButtonSize, m.windowButtonSize);
    m_back->setIconSize(18);
    m_back->setFocusPolicy(Qt::NoFocus);
    m_back->setColors(c.textSecondary, c.primaryBright);
    m_back->setHoverBackground(Theme::alpha(c.primary, 38));
    m_back->setToolTip(tr("Back to menu (Esc)"));
    m_back->setAccessibleName(tr("Back to menu"));
    m_back->hide();

    m_brandMark = new BrandMark(this);
    m_brandMark->setMarkHeight(20);
    m_brandMark->hide();

    m_title = new QLabel(this);
    m_title->setFont(Theme::font(14, QFont::Medium));
    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_title->setCursor(Qt::PointingHandCursor);
    m_title->hide();

    m_minimise = new IconButton(QStringLiteral("minimize"), this);
    m_minimise->setToolTip(tr("Minimise"));
    m_minimise->setAccessibleName(tr("Minimise"));

    m_close = new IconButton(QStringLiteral("close"), this);
    m_close->setToolTip(tr("Close"));
    m_close->setAccessibleName(tr("Close"));
    m_close->setHoverBackground(c.closeHover);
    m_close->setColors(c.textSecondary, QColor(0xFF, 0xFF, 0xFF));

    for (IconButton *button : {m_minimise, m_close}) {
        button->setFixedSize(m.windowButtonSize, m.windowButtonSize);
        button->setIconSize(16);
        // Window controls are chrome; they must not grab keyboard focus (which
        // would draw a focus ring around them the moment the window opens).
        button->setFocusPolicy(Qt::NoFocus);
    }

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 14, 0);
    layout->setSpacing(4);
    layout->addWidget(m_back, 0, Qt::AlignVCenter);
    layout->addSpacing(6);
    layout->addWidget(m_brandMark, 0, Qt::AlignVCenter);
    layout->addSpacing(12);
    layout->addWidget(m_title, 0, Qt::AlignVCenter);
    layout->addStretch(1);
    layout->addWidget(m_minimise, 0, Qt::AlignVCenter);
    layout->addWidget(m_close, 0, Qt::AlignVCenter);

    connect(m_back, &IconButton::clicked, this, &CustomTitleBar::backRequested);
    connect(m_minimise, &IconButton::clicked, this, &CustomTitleBar::minimiseRequested);
    connect(m_close, &IconButton::clicked, this, &CustomTitleBar::closeRequested);
}

void CustomTitleBar::setBrand(const QString &title)
{
    m_title->setText(title);
    m_title->setVisible(!title.isEmpty());
    m_brandMark->setVisible(!title.isEmpty());
}

void CustomTitleBar::setBackVisible(bool visible)
{
    m_back->setVisible(visible);
}

bool CustomTitleBar::isBackVisible() const
{
    return m_back->isVisible();
}

void CustomTitleBar::setMaximiseEnabled(bool enabled)
{
    if (enabled == (m_maximise != nullptr))
        return;

    if (!enabled) {
        delete m_maximise;
        m_maximise = nullptr;
        return;
    }

    const Theme::Metrics &m = Theme::metrics();

    m_maximise = new IconButton(m_maximised ? QStringLiteral("restore")
                                            : QStringLiteral("maximize"),
                                this);
    m_maximise->setFixedSize(m.windowButtonSize, m.windowButtonSize);
    m_maximise->setIconSize(16);
    m_maximise->setFocusPolicy(Qt::NoFocus);
    m_maximise->setToolTip(m_maximised ? tr("Restore") : tr("Maximise"));
    m_maximise->setAccessibleName(m_maximise->toolTip());

    connect(m_maximise, &IconButton::clicked, this, &CustomTitleBar::maximiseRestoreRequested);

    // Slot the button in between minimise and close.
    if (auto *box = qobject_cast<QHBoxLayout *>(layout()))
        box->insertWidget(box->indexOf(m_close), m_maximise, 0, Qt::AlignVCenter);
}

void CustomTitleBar::setMaximised(bool maximised)
{
    if (m_maximised == maximised)
        return;

    m_maximised = maximised;
    if (!m_maximise)
        return;

    m_maximise->setIconName(maximised ? QStringLiteral("restore") : QStringLiteral("maximize"));
    m_maximise->setToolTip(maximised ? tr("Restore") : tr("Maximise"));
    m_maximise->setAccessibleName(m_maximise->toolTip());
}

bool CustomTitleBar::isOnDragArea(const QPoint &position) const
{
    // Anything that is not one of the window buttons can be used to drag; the
    // brand mark and title are transparent for mouse events.
    QWidget *child = childAt(position);
    return child == nullptr;
}

bool CustomTitleBar::isOnBrand(const QPoint &position) const
{
    if (!m_title->isVisible())
        return false;
    const QRect brand = m_brandMark->geometry().united(m_title->geometry());
    return brand.adjusted(-6, 0, 8, 0).contains(position);
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();

    if (event->button() == Qt::LeftButton && isOnBrand(pos)) {
        emit brandClicked();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && isOnDragArea(pos)) {
        // The platform move loop started below never delivers a double click, so
        // the second click of a pair is detected manually.
        const bool isDoubleClick = m_lastPress.isValid()
                                   && m_lastPress.elapsed() <= QApplication::doubleClickInterval()
                                   && (pos - m_lastPressPos).manhattanLength() < 8;
        m_lastPress.restart();
        m_lastPressPos = pos;

        if (isDoubleClick && m_maximise) {
            m_lastPress.invalidate();
            emit maximiseRestoreRequested();
            event->accept();
            return;
        }

        if (QWindow *handle = window()->windowHandle()) {
            // Hand the drag over to the platform so multi monitor handling and
            // DPI changes keep working while the window is moved.
            handle->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
