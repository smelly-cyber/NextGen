// RootContainer.h - The single widget that owns the window's visible background.
//
// The shell hierarchy is:
//
//   FramelessWindow          transparent, frameless, draws only the outer glow
//   └── RootContainer        the rounded body: paints the app background
//       ├── CustomTitleBar
//       ├── Sidebar / branding rail
//       └── Main content (stacked pages, status strip, ...)
//
// The container deliberately paints a PLAIN rectangle: the rounded silhouette is
// applied once, by RoundedCornersEffect, so the corner edge is anti-aliased
// exactly one time and never doubles up into a soft or dark fringe. Everything
// outside the container stays fully transparent, so the desktop shows through.
#pragma once

#include <QWidget>

class RootContainer : public QWidget
{
    Q_OBJECT

public:
    explicit RootContainer(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
