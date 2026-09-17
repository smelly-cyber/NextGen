// PanelSeparator.h - Hairline that fades out towards both ends.
//
// Used between the branding and content panels, under the title bar and above
// the status strip.
#pragma once

#include <QWidget>

class PanelSeparator : public QWidget
{
public:
    explicit PanelSeparator(Qt::Orientation orientation = Qt::Vertical, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::Orientation m_orientation;
};
