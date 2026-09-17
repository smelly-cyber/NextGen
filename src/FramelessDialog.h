// FramelessDialog.h - Shared base for every popup in the app.
//
// Gives dialogs the same look as the main windows: no Windows title bar, a
// rounded translucent surface painted in the brand gradient, and a custom
// header carrying the NT mark, a title and the window controls (a close button,
// and an optional minimise). Subclasses only add their body to contentLayout().
//
// Closing (the header X or Alt+F4) routes through onCloseRequested(), which
// subclasses can override - e.g. the licence gate quits the whole app instead
// of just hiding.
#pragma once

#include <QDialog>

class IconButton;
class QLabel;
class QVBoxLayout;

class FramelessDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FramelessDialog(QWidget *parent = nullptr);

    /// Text shown in the custom header, next to the brand mark.
    void setHeaderTitle(const QString &title);

    /// When false the window cannot be dragged (used by the licence gate).
    void setMovable(bool movable);

    /// Shows a minimise control in the header (hidden by default).
    void setMinimiseButtonVisible(bool visible);

    /// Layout the subclass adds its body widgets to.
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

    /// Adjusts the padding around the body content.
    void setContentMargins(int left, int top, int right, int bottom);

protected:
    /// Invoked by the header X and by Alt+F4. Default: reject().
    virtual void onCloseRequested();

    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_header = nullptr;
    QLabel *m_title = nullptr;
    IconButton *m_minimise = nullptr;
    IconButton *m_close = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    bool m_movable = true;
};
