#include "RoundedCornersEffect.h"

#include <QImage>
#include <QPaintDevice>
#include <QPainter>
#include <QPixmap>

RoundedCornersEffect::RoundedCornersEffect(QObject *parent)
    : QGraphicsEffect(parent)
{
}

void RoundedCornersEffect::setRadius(qreal radius)
{
    radius = qMax(0.0, radius);
    if (qFuzzyCompare(m_radius + 1.0, radius + 1.0))
        return;
    m_radius = radius;
    update();
}

void RoundedCornersEffect::draw(QPainter *painter)
{
    // Radius 0 (e.g. while maximised) means square corners: hand the source
    // straight through, with no pixmap round-trip at all.
    if (m_radius <= 0.0) {
        drawSource(painter);
        return;
    }

    // Render the source - the root container and every child widget - into a
    // pixmap at full device resolution. Working in device coordinates is what
    // keeps the corners sharp under Windows display scaling (high DPI).
    QPoint offset;
    const QPixmap source = sourcePixmap(Qt::DeviceCoordinates, &offset, QGraphicsEffect::NoPad);
    if (source.isNull()) {
        drawSource(painter);
        return;
    }

    // The masking is done on a QImage rather than on the QPixmap: composition
    // modes other than SourceOver are not reliably supported when painting onto
    // a QPixmap (it can be backed by a native surface), whereas a raster QImage
    // always honours them.
    QImage content = source.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // sourcePixmap() in device coordinates returns raw device pixels, so the
    // radius has to be scaled by the same device pixel ratio the painter uses.
    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qreal(1.0);
    const qreal deviceRadius = m_radius * dpr;

    // Build a full-size alpha mask: transparent everywhere, opaque inside an
    // ANTI-ALIASED rounded rectangle.
    QImage mask(content.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter maskPainter(&mask);
        maskPainter.setRenderHint(QPainter::Antialiasing, true);
        maskPainter.setPen(Qt::NoPen);
        maskPainter.setBrush(Qt::white);
        maskPainter.drawRoundedRect(QRectF(0, 0, mask.width(), mask.height()), deviceRadius,
                                    deviceRadius);
    }

    // Multiply the content's alpha by that mask. It is applied as a full-size
    // IMAGE, not as a shape: a DestinationIn drawRoundedRect() would only
    // composite the pixels the shape itself rasterises and would leave every
    // pixel OUTSIDE the rounded rect untouched - i.e. still fully opaque. That
    // is precisely what used to leave square corners behind.
    {
        QPainter contentPainter(&content);
        contentPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        contentPainter.drawImage(0, 0, mask);
    }

    // The image is already in device pixels, so blit it 1:1 with the world
    // transform reset - the documented way to draw a device-coordinate effect
    // result back onto the painter.
    painter->save();
    painter->setWorldTransform(QTransform());
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(offset, content);
    painter->restore();
}
