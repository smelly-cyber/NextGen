// FramelessWindow.h - Shared shell for every NEXTGEN TWEAKS top level window.
//
// Provides everything the native caption bar would normally give us: the dark
// rounded body, the thin blue outline, the outer glow, window dragging, edge
// resizing, minimise / maximise / close, plus a custom title bar.
//
// Shape of the shell:
//
//   FramelessWindow      frameless + WA_TranslucentBackground; paints ONLY the
//   (transparent)        outer glow, so the desktop shows through everywhere
//   │                    outside the rounded body.
//   ├── RootContainer    the rounded body - owns the application background.
//   │   ├── CustomTitleBar
//   │   └── the subclass' panels (sidebar, stacked pages, status strip, ...)
//   └── WindowFrameOverlay   draws the anti-aliased outline on top.
//
// The corners are rounded by a RoundedCornersEffect installed on the root
// container. That clips the container AND every child with an anti-aliased alpha
// mask on each repaint, which is what stops the panels (which all paint opaque
// rectangles) from squaring off the four corners.
//
// Subclasses only fill contentWidget() with their own panels; they never have
// to care about the chrome. LoginWindow and MainWindow both build on this.
#pragma once

#include "RootContainer.h" // Complete type: contentWidget() upcasts inline.

#include <QWidget>

class CustomTitleBar;
class RoundedCornersEffect;

/// Transparent overlay that draws the window outline on top of the panels so
/// the rounded corners stay perfectly anti-aliased.
class WindowFrameOverlay : public QWidget
{
public:
    explicit WindowFrameOverlay(QWidget *parent = nullptr);

    void setFrameGeometry(const QRect &body, int radius);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRect m_body;
    int m_radius = 0;
};

class FramelessWindow : public QWidget
{
    Q_OBJECT

public:
    explicit FramelessWindow(QWidget *parent = nullptr);
    ~FramelessWindow() override;

    /// The title bar floating over the top of the content.
    CustomTitleBar *titleBar() const { return m_titleBar; }

    /// Host for the subclass' own panels - add a layout to this.
    QWidget *contentWidget() const { return m_content; }

    /// Whether the window may be maximised. Off by default: the login window is
    /// a fixed-format dialog, the main window turns it on.
    void setMaximiseAllowed(bool allowed);
    bool isMaximiseAllowed() const { return m_maximiseAllowed; }

    /// Whether Escape closes the window (true by default).
    void setCloseOnEscape(bool enabled) { m_closeOnEscape = enabled; }

    /// Whether the window can be resized by dragging its edges. When disabled
    /// the window is also pinned to its current size so nothing can grow or
    /// shrink it. On by default.
    void setResizable(bool enabled);

public slots:
    void toggleMaximiseRestore();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    /// Current outer padding: the glow margin, or zero while maximised.
    int shellMargin() const;
    int shellRadius() const;
    QRect bodyRect() const;

private:
    void updateShellGeometry();

    /// Which window edges (if any) the cursor at \a pos would resize.
    Qt::Edges edgesAt(const QPoint &pos) const;
    void applyResizeCursor(Qt::Edges edges);

    /// Applies (or clears) the platform's own corner policy. On Windows 11 we
    /// explicitly opt OUT of DWM rounding - see the note in the .cpp.
    void applyNativeCornerPolicy();

    /// Tells the window manager itself whether the window may be resized. Qt's
    /// size constraints are not enough on their own - see the note in the .cpp.
    void applyNativeSizePolicy();

    RootContainer *m_content = nullptr;
    CustomTitleBar *m_titleBar = nullptr;
    WindowFrameOverlay *m_frameOverlay = nullptr;
    RoundedCornersEffect *m_roundEffect = nullptr;

    bool m_maximiseAllowed = false;
    bool m_closeOnEscape = true;
    bool m_resizable = true;

    /// The one size a non-resizable window is allowed to have. Anything that
    /// changes the size behind our back is snapped straight back to it.
    QSize m_lockedSize;
};
