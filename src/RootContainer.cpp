#include "RootContainer.h"

#include "Theme.h"

#include <QLinearGradient>
#include <QPainter>

RootContainer::RootContainer(QWidget *parent)
    : QWidget(parent)
{
    // No system background: this widget paints every one of its pixels itself,
    // which keeps the translucent window free of any native grey/black fill.
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    // Give it an object name so any future stylesheet can target it precisely
    // instead of using a broad "QWidget { background: ... }" rule, which would
    // leak an opaque background onto the transparent shell.
    setObjectName(QStringLiteral("rootContainer"));
}

void RootContainer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();

    QPainter painter(this);

    // A plain (square) fill on purpose - see the note in RootContainer.h. The
    // rounding is applied once by the corner effect that clips this whole tree,
    // so nothing here needs its own radius. This is the application's base
    // background; the panels stacked on top cover it, but it guarantees there is
    // never a transparent gap between them.
    QLinearGradient base(QPointF(rect().topLeft()), QPointF(rect().bottomRight()));
    base.setColorAt(0.0, c.windowTop);
    base.setColorAt(1.0, c.windowBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(rect());
}
