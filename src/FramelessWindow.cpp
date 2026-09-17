#include "FramelessWindow.h"

#include "CustomTitleBar.h"
#include "RoundedCornersEffect.h"
#include "Theme.h"
#include "WindowStyle.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
// Windows 11 lets DWM round a window's own frame. We deliberately switch that
// OFF (see applyNativeCornerPolicy) rather than stack it on top of our own
// clipping, so these are the only Windows bits the shell needs.
#  include <windows.h>
#  include <dwmapi.h>
// Older mingw / SDK headers predate the Windows 11 corner attribute, so define
// the pieces we use if they are missing. The values are from the Windows SDK.
#  ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#    define DWMWA_WINDOW_CORNER_PREFERENCE 33
#  endif
#  ifndef DWMWCP_DONOTROUND
#    define DWMWCP_DONOTROUND 1
#  endif
#  ifndef DWMWCP_ROUND
#    define DWMWCP_ROUND 2
#  endif
#endif

// ----------------------------------------------------- WindowFrameOverlay --

WindowFrameOverlay::WindowFrameOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
}

void WindowFrameOverlay::setFrameGeometry(const QRect &body, int radius)
{
    m_body = body;
    m_radius = radius;
    update();
}

void WindowFrameOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if (m_body.isEmpty())
        return;

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body = QRectF(m_body).adjusted(0.5, 0.5, -0.5, -0.5);

    // The rounded silhouette is produced by an anti-aliased graphics effect on
    // the content (RoundedCornersEffect); this overlay only draws the outline on
    // top so it stays crisply anti-aliased and always aligns with the corners.

    // Thin blue outline plus a softer inner highlight.
    QPen outline(c.windowBorder);
    outline.setWidthF(1.2);
    painter.setPen(outline);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, m_radius, m_radius);

    QPen inner(Theme::alpha(QColor(0xFF, 0xFF, 0xFF), 14));
    inner.setWidthF(1.0);
    painter.setPen(inner);
    painter.drawRoundedRect(body.adjusted(1.2, 1.2, -1.2, -1.2), qMax(0, m_radius - 1),
                            qMax(0, m_radius - 1));
}

// -------------------------------------------------------- FramelessWindow --

FramelessWindow::FramelessWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);

    const Theme::Metrics &m = Theme::metrics();
    setMinimumSize(m.windowMinWidth + 2 * m.glowMargin, m.windowMinHeight + 2 * m.glowMargin);
    resize(m.windowDefaultWidth + 2 * m.glowMargin, m.windowDefaultHeight + 2 * m.glowMargin);

    // The root container owns the visible background; the window itself stays
    // fully transparent so the desktop shows through outside the rounded body.
    m_content = new RootContainer(this);
    m_content->installEventFilter(this);

    // Anti-aliased rounded corners for the container AND every widget inside it.
    // This is the piece that actually fixes the square corners: each panel
    // (branding rail, stacked pages, status strip) paints an opaque rectangle,
    // and this clip trims all of them together on every repaint, so the corners
    // can never be squared off, go stale, or turn black.
    m_roundEffect = new RoundedCornersEffect(this);
    m_content->setGraphicsEffect(m_roundEffect);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_content);

    // The title bar floats above the content so panel separators can run the
    // full height of the window.
    m_titleBar = new CustomTitleBar(m_content);
    m_titleBar->raise();

    m_frameOverlay = new WindowFrameOverlay(this);
    m_frameOverlay->raise();

    connect(m_titleBar, &CustomTitleBar::minimiseRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &CustomTitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &CustomTitleBar::maximiseRestoreRequested, this,
            &FramelessWindow::toggleMaximiseRestore);

    updateShellGeometry();
}

FramelessWindow::~FramelessWindow() = default;

void FramelessWindow::setMaximiseAllowed(bool allowed)
{
    if (m_maximiseAllowed == allowed)
        return;

    m_maximiseAllowed = allowed;
    m_titleBar->setMaximiseEnabled(allowed);

    if (!allowed && (isMaximized() || isFullScreen()))
        showNormal();
}

void FramelessWindow::setResizable(bool enabled)
{
    m_resizable = enabled;
    if (!enabled) {
        // Pin the window to its current size so edge drags, snap gestures and
        // programmatic layout changes can never resize it.
        m_lockedSize = size();
        setFixedSize(m_lockedSize);

        // A locked window's own size is authoritative: the panels inside live
        // within it, never the other way round. Without this the content's
        // minimum size hint bubbles all the way up to the window - and it is
        // bigger than the window, because the dense section pages ask for more
        // height than a 1080p screen can give them. Windows then grows the
        // window to satisfy that minimum the first time a drag starts a move
        // loop, which is what made the window "get enlarged when you drag it".
        m_content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    } else {
        m_lockedSize = QSize();
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
    applyNativeSizePolicy();
}

void FramelessWindow::toggleMaximiseRestore()
{
    if (!m_maximiseAllowed)
        return;

    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

bool FramelessWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_content && event->type() == QEvent::Resize) {
        if (m_titleBar)
            m_titleBar->setGeometry(0, 0, m_content->width(), m_titleBar->height());
    }

    return QWidget::eventFilter(watched, event);
}

int FramelessWindow::shellMargin() const
{
    return isMaximized() || isFullScreen() ? 0 : Theme::metrics().glowMargin;
}

int FramelessWindow::shellRadius() const
{
    return isMaximized() || isFullScreen() ? 0 : Theme::metrics().windowRadius;
}

QRect FramelessWindow::bodyRect() const
{
    const int margin = shellMargin();
    return rect().adjusted(margin, margin, -margin, -margin);
}

void FramelessWindow::updateShellGeometry()
{
    const int margin = shellMargin();

    if (auto *outer = qobject_cast<QVBoxLayout *>(layout()))
        outer->setContentsMargins(margin, margin, margin, margin);

    if (m_titleBar && m_content)
        m_titleBar->setGeometry(0, 0, m_content->width(), m_titleBar->height());

    if (m_frameOverlay) {
        m_frameOverlay->setGeometry(rect());
        m_frameOverlay->setFrameGeometry(bodyRect(), shellRadius());
        m_frameOverlay->raise();
    }

    if (m_roundEffect)
        m_roundEffect->setRadius(shellRadius());

    update();
}

void FramelessWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const int radius = shellRadius();
    const QRect body = bodyRect();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // --- Outer glow --------------------------------------------------------
    const int margin = shellMargin();
    if (margin > 0) {
        painter.setBrush(Qt::NoBrush);
        for (int i = margin; i >= 1; --i) {
            const qreal t = qreal(i) / margin;
            QColor ring = c.windowGlow;
            ring.setAlphaF(0.085 * (1.0 - t) * (1.0 - t));
            QPen pen(ring);
            pen.setWidthF(2.0);
            painter.setPen(pen);
            painter.drawRoundedRect(QRectF(body).adjusted(-i, -i, i, i), radius + i, radius + i);
        }
    }

    // NOTE: the window deliberately paints NO body fill. The background belongs
    // to RootContainer, which the corner effect clips. Painting a second rounded
    // body here would sit behind the container's anti-aliased corner edge and
    // show through it as a dark halo, so the shell stays transparent apart from
    // the glow above.
}

void FramelessWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // The native handle exists by now, so the platform policies can be set.
    applyNativeCornerPolicy();
    applyNativeSizePolicy();
}

void FramelessWindow::applyNativeSizePolicy()
{
    QWindow *handle = windowHandle();
    if (!handle)
        return; // Not created yet; showEvent() calls us again once it is.

    // THE root cause of "the window gets bigger when I drag it".
    //
    // Qt only forwards a widget's size constraints to its platform window once
    // that platform window exists. setResizable() runs from the constructor,
    // long before the window is shown, so the constraints never got any further
    // than the QWidget: Windows was told this window had no maximum size at all
    // (a WM_GETMINMAXINFO trace showed it being handed the whole virtual
    // desktop, 5780x1359). The shell therefore felt free to pick its own size
    // the moment a drag started a move loop, and Qt applies geometry that comes
    // *from* the platform without clamping it.
    //
    // Pushing the constraints onto the QWindow here re-runs
    // propagateSizeHints(), so Windows finally knows the real maximum and stops
    // resizing the window behind our back. Fixing it at this level matters:
    // correcting the size afterwards from resizeEvent() worked, but resizing a
    // window from inside the shell's modal drag loop wedges that loop and the
    // window stops responding to clicks.
    if (!m_resizable && m_lockedSize.isValid()) {
        handle->setMinimumSize(m_lockedSize);
        handle->setMaximumSize(m_lockedSize);
    } else {
        handle->setMinimumSize(QSize(0, 0));
        handle->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
    }

#ifdef Q_OS_WIN
    // Belt to that brace: Windows only offers snap gestures - edge snapping,
    // Win+Up, the caption double-click grow - to windows it considers sizable,
    // and those gestures call SetWindowPos directly rather than going through
    // the size hints above. Telling the shell the truth at the style level takes
    // all of them off the table at once.
    const auto hwnd = reinterpret_cast<HWND>(handle->winId());
    if (!hwnd)
        return;

    const LONG_PTR sizingStyles = WS_THICKFRAME | WS_MAXIMIZEBOX;
    const LONG_PTR current = GetWindowLongPtr(hwnd, GWL_STYLE);
    const LONG_PTR wanted = m_resizable ? (current | sizingStyles) : (current & ~sizingStyles);
    if (wanted == current)
        return;

    SetWindowLongPtr(hwnd, GWL_STYLE, wanted);
    // Style changes only take effect once the frame is recalculated.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
}

void FramelessWindow::applyNativeCornerPolicy()
{
#ifdef Q_OS_WIN
    // Windows 11 can round a window's own frame through DWM
    // (DWMWA_WINDOW_CORNER_PREFERENCE). We deliberately DISABLE it here.
    //
    // Why: this window is translucent and intentionally larger than the visible
    // body - the extra margin on every side holds the outer glow. DWM would
    // round the *outer*, invisible edge (clipping the glow) while leaving the
    // real rounded body untouched. Running two clipping systems at once also
    // double-darkens the corner pixels. The rounding is therefore owned entirely
    // by RoundedCornersEffect, which is anti-aliased and matches the body
    // exactly, and we ask DWM to keep its hands off the frame.
    const auto handle = reinterpret_cast<HWND>(winId());
    if (!handle)
        return;

    const DWORD preference = DWMWCP_DONOTROUND;
    // Harmless on Windows 10 and earlier: the attribute is unsupported there and
    // the call simply returns a failure code, which we intentionally ignore.
    DwmSetWindowAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &preference,
                          sizeof(preference));
#endif
}

void FramelessWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // Last line of defence, for anything the size hints above do not cover.
    //
    // Deliberately deferred, never done here and now: while a window is being
    // dragged Windows is inside a modal move loop, and resizing the window from
    // inside that loop wedges it - the drag finishes but the window stops
    // responding to clicks. A zero timer cannot run until the loop has handed
    // control back, which is exactly when it is safe to correct the geometry.
    if (!m_resizable && m_lockedSize.isValid() && size() != m_lockedSize) {
        const QSize locked = m_lockedSize;
        QTimer::singleShot(0, this, [this, locked] {
            if (!m_resizable && size() != locked)
                resize(locked);
        });
    }

    updateShellGeometry();
}

void FramelessWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        if (!m_maximiseAllowed && (isMaximized() || isFullScreen())) {
            // Fixed-format window: if a snap gesture or the window manager tries
            // to blow it up, drop straight back to the normal state (queued, so
            // we are not re-entering the state change).
            QTimer::singleShot(0, this, [this] { showNormal(); });
        }
        if (m_titleBar)
            m_titleBar->setMaximised(isMaximized());
        updateShellGeometry();
        // Restoring from minimised re-creates the native frame, so the style has
        // to be re-asserted or edge snapping quietly comes back.
        applyNativeSizePolicy();
    }
}

Qt::Edges FramelessWindow::edgesAt(const QPoint &pos) const
{
    if (!m_resizable || isMaximized() || isFullScreen())
        return Qt::Edges();

    const int grip = shellMargin() + Theme::metrics().resizeBorder;

    Qt::Edges edges;
    if (pos.x() <= grip)
        edges |= Qt::LeftEdge;
    if (pos.x() >= width() - grip)
        edges |= Qt::RightEdge;
    if (pos.y() <= grip)
        edges |= Qt::TopEdge;
    if (pos.y() >= height() - grip)
        edges |= Qt::BottomEdge;

    return edges;
}

void FramelessWindow::applyResizeCursor(Qt::Edges edges)
{
    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge)
        || (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge)
               || (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & Qt::LeftEdge || edges & Qt::RightEdge) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & Qt::TopEdge || edges & Qt::BottomEdge) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
}

void FramelessWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const Qt::Edges edges = edgesAt(event->position().toPoint());
        if (edges) {
            if (QWindow *handle = windowHandle()) {
                handle->startSystemResize(edges);
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent *event)
{
    applyResizeCursor(edgesAt(event->position().toPoint()));
    QWidget::mouseMoveEvent(event);
}

void FramelessWindow::leaveEvent(QEvent *event)
{
    unsetCursor();
    QWidget::leaveEvent(event);
}

void FramelessWindow::keyPressEvent(QKeyEvent *event)
{
    if (m_closeOnEscape && event->key() == Qt::Key_Escape) {
        close();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
