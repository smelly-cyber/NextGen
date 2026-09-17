#include "FramelessDialog.h"

#include "IconButton.h"
#include "Theme.h"

#include <QCloseEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr int kHeaderHeight = 46;
} // namespace

FramelessDialog::FramelessDialog(QWidget *parent)
    : QDialog(parent)
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- Custom header ------------------------------------------------------
    m_header = new QWidget(this);
    m_header->setFixedHeight(kHeaderHeight);
    m_header->installEventFilter(this);

    auto *mark = new QLabel(m_header);
    mark->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QPixmap logo(QStringLiteral(":/assets/logo/nextgen_tweaks_mark.png"));
    if (!logo.isNull()) {
        const qreal dpr = devicePixelRatioF();
        QPixmap scaled = logo.scaledToHeight(qRound(20 * dpr), Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        mark->setPixmap(scaled);
    }

    m_title = new QLabel(m_header);
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_title->setFont(Theme::font(13, QFont::DemiBold, 0.4));
    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    m_minimise = new IconButton(QStringLiteral("minimize"), m_header);
    m_minimise->setFixedSize(m.windowButtonSize, m.windowButtonSize);
    m_minimise->setIconSize(16);
    m_minimise->setColors(c.textSecondary, c.textPrimary);
    m_minimise->setHoverBackground(c.controlHover);
    m_minimise->setToolTip(tr("Minimise"));
    m_minimise->hide();

    m_close = new IconButton(QStringLiteral("close"), m_header);
    m_close->setFixedSize(m.windowButtonSize, m.windowButtonSize);
    m_close->setIconSize(16);
    m_close->setColors(c.textSecondary, QColor(0xFF, 0xFF, 0xFF));
    m_close->setHoverBackground(c.closeHover);
    m_close->setToolTip(tr("Close"));

    auto *headerRow = new QHBoxLayout(m_header);
    headerRow->setContentsMargins(18, 0, 10, 0);
    headerRow->setSpacing(10);
    headerRow->addWidget(mark, 0, Qt::AlignVCenter);
    headerRow->addWidget(m_title, 0, Qt::AlignVCenter);
    headerRow->addStretch(1);
    headerRow->addWidget(m_minimise, 0, Qt::AlignVCenter);
    headerRow->addWidget(m_close, 0, Qt::AlignVCenter);

    connect(m_minimise, &IconButton::clicked, this, [this] { showMinimized(); });
    connect(m_close, &IconButton::clicked, this, [this] { onCloseRequested(); });

    auto *divider = new QFrame(this);
    divider->setFixedHeight(1);
    divider->setStyleSheet(
        QStringLiteral("background: %1; border: none;").arg(c.panelDivider.name()));

    // --- Content ------------------------------------------------------------
    m_content = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(28, 20, 28, 24);
    m_contentLayout->setSpacing(0);

    outer->addWidget(m_header);
    outer->addWidget(divider);
    outer->addWidget(m_content, 1);
}

void FramelessDialog::setHeaderTitle(const QString &title)
{
    m_title->setText(title);
    setWindowTitle(title);
}

void FramelessDialog::setMovable(bool movable)
{
    m_movable = movable;
}

void FramelessDialog::setMinimiseButtonVisible(bool visible)
{
    m_minimise->setVisible(visible);
}

void FramelessDialog::setContentMargins(int left, int top, int right, int bottom)
{
    m_contentLayout->setContentsMargins(left, top, right, bottom);
}

void FramelessDialog::onCloseRequested()
{
    reject();
}

void FramelessDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // Open centred over the owner window.
    QRect reference;
    if (QWidget *owner = parentWidget())
        reference = owner->frameGeometry();

    // A parentless dialog (the administrator prompt runs before any window
    // exists) has nothing to centre on and would otherwise be left wherever the
    // window manager put it - which can be completely off-screen.
    if (reference.isEmpty()) {
        const QScreen *target = screen() ? screen() : QGuiApplication::primaryScreen();
        if (target)
            reference = target->availableGeometry();
    }

    if (!reference.isEmpty()) {
        QRect self = frameGeometry();
        self.moveCenter(reference.center());
        move(self.topLeft());
    }
}

void FramelessDialog::closeEvent(QCloseEvent *event)
{
    // Alt+F4 is routed through the same path as the header X. accept() closes
    // via done() and never reaches here, so a confirmed dialog still closes.
    event->ignore();
    onCloseRequested();
}

bool FramelessDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header && m_movable && event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            if (QWindow *handle = window()->windowHandle()) {
                handle->startSystemMove();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void FramelessDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const qreal radius = Theme::metrics().windowRadius;
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(body, radius, radius);

    QLinearGradient base(body.topLeft(), body.bottomRight());
    base.setColorAt(0.0, c.windowTop);
    base.setColorAt(1.0, c.windowBottom);
    painter.fillPath(path, base);

    QPen border(Theme::alpha(c.windowBorder, 150));
    border.setWidthF(1.0);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, radius, radius);
}
