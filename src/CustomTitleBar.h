// CustomTitleBar.h - Replacement for the native Windows caption bar.
//
// The bar is transparent (the panels beneath it show through), provides the
// drag area for moving the window and hosts the window buttons.
//
// It can optionally show the NEXTGEN TWEAKS brand mark and a title on the left
// (the main window does) and an optional maximise / restore button (the login
// window is a fixed-format dialog and leaves it off).
#pragma once

#include <QByteArray>
#include <QPixmap>
#include <QElapsedTimer>
#include <QPoint>
#include <QString>
#include <QWidget>

class IconButton;
class QLabel;

/// Draws just the "NT" monogram out of the logo asset, cropped from the full
/// artwork with a viewBox override so it stays sharp at small sizes.
class BrandMark : public QWidget
{
public:
    explicit BrandMark(QWidget *parent = nullptr);

    void setMarkHeight(int height);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QByteArray m_svg;
    QPixmap m_logo;
    bool m_cropToMonogram = true;
    int m_markHeight = 22;
};

class CustomTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);

    /// Shows the brand mark plus \a title on the left of the bar.
    void setBrand(const QString &title);

    /// Shows the "back to menu" control at the very left of the bar.
    void setBackVisible(bool visible);
    bool isBackVisible() const;

    /// Adds / removes the maximise button.
    void setMaximiseEnabled(bool enabled);
    bool isMaximiseEnabled() const { return m_maximise != nullptr; }

    /// Switches the maximise button between the maximise and restore glyphs.
    void setMaximised(bool maximised);

signals:
    /// The back control was activated.
    void backRequested();
    /// The brand mark / title was clicked (also a "go home" affordance).
    void brandClicked();
    void minimiseRequested();
    void maximiseRestoreRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool isOnDragArea(const QPoint &position) const;
    bool isOnBrand(const QPoint &position) const;

    IconButton *m_back = nullptr;
    BrandMark *m_brandMark = nullptr;
    QLabel *m_title = nullptr;
    IconButton *m_minimise = nullptr;
    IconButton *m_maximise = nullptr;
    IconButton *m_close = nullptr;
    bool m_maximised = false;

    // startSystemMove() swallows the follow-up double click, so a double click
    // on the drag area is detected manually.
    QElapsedTimer m_lastPress;
    QPoint m_lastPressPos;
};
