#include "PanelSeparator.h"

#include "Theme.h"

#include <QLinearGradient>
#include <QPainter>

PanelSeparator::PanelSeparator(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
{
    if (m_orientation == Qt::Vertical) {
        setFixedWidth(1);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    } else {
        setFixedHeight(1);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

QSize PanelSeparator::sizeHint() const
{
    return m_orientation == Qt::Vertical ? QSize(1, 10) : QSize(10, 1);
}

void PanelSeparator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);

    QLinearGradient gradient = m_orientation == Qt::Vertical
                                   ? QLinearGradient(0, 0, 0, height())
                                   : QLinearGradient(0, 0, width(), 0);
    gradient.setColorAt(0.0, Theme::alpha(c.panelDivider, 0));
    gradient.setColorAt(0.18, Theme::alpha(c.panelDivider, 150));
    gradient.setColorAt(0.5, c.panelDivider);
    gradient.setColorAt(0.82, Theme::alpha(c.panelDivider, 150));
    gradient.setColorAt(1.0, Theme::alpha(c.panelDivider, 0));
    painter.fillRect(rect(), gradient);
}
