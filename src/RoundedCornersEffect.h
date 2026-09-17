// RoundedCornersEffect.h - Anti-aliased rounded-corner clip for a widget tree.
//
// A QGraphicsEffect that renders its source (the window content and every child)
// and multiplies its alpha by an anti-aliased rounded rectangle. Unlike a 1-bit
// QWidget mask it produces genuinely smooth corners, and unlike a one-shot paint
// trick it re-runs on every repaint, so the corners can never go stale or turn
// black - all while the translucent window keeps its outer glow.
#pragma once

#include <QGraphicsEffect>

class RoundedCornersEffect : public QGraphicsEffect
{
    Q_OBJECT

public:
    explicit RoundedCornersEffect(QObject *parent = nullptr);

    /// Corner radius in logical pixels. 0 disables rounding (square).
    void setRadius(qreal radius);
    qreal radius() const { return m_radius; }

protected:
    void draw(QPainter *painter) override;

private:
    qreal m_radius = 0.0;
};
