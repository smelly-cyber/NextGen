#include "IconProvider.h"

#include <QFile>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

namespace IconProvider {

namespace {

/// Colour token used inside every bundled SVG asset.
const QByteArray kColorToken = QByteArrayLiteral("#FFFFFF");

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> instance;
    return instance;
}

QHash<QString, QByteArray> &sourceCache()
{
    static QHash<QString, QByteArray> instance;
    return instance;
}

QByteArray sourceFor(const QString &name)
{
    auto it = sourceCache().constFind(name);
    if (it != sourceCache().constEnd())
        return it.value();

    QFile file(QStringLiteral(":/assets/icons/%1.svg").arg(name));
    QByteArray data;
    if (file.open(QIODevice::ReadOnly))
        data = file.readAll();

    sourceCache().insert(name, data);
    return data;
}

} // namespace

QPixmap pixmap(const QString &name, const QSize &size, const QColor &color, qreal devicePixelRatio)
{
    if (name.isEmpty() || size.isEmpty())
        return QPixmap();

    const qreal dpr = devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;
    const QString key = QStringLiteral("%1|%2x%3|%4|%5")
                            .arg(name)
                            .arg(size.width())
                            .arg(size.height())
                            .arg(color.name(QColor::HexArgb))
                            .arg(dpr);

    auto cached = cache().constFind(key);
    if (cached != cache().constEnd())
        return cached.value();

    QByteArray markup = sourceFor(name);
    if (markup.isEmpty())
        return QPixmap();

    markup.replace(kColorToken, color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(markup);
    if (!renderer.isValid())
        return QPixmap();

    QImage image(QSize(qRound(size.width() * dpr), qRound(size.height() * dpr)),
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    image.setDevicePixelRatio(dpr);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
    painter.end();

    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(dpr);

    // The alpha channel of the source already carries the anti-aliasing, so an
    // opaque colour is applied by modulating the rendered artwork directly.
    if (color.alpha() < 255) {
        QPixmap faded(result.size());
        faded.setDevicePixelRatio(dpr);
        faded.fill(Qt::transparent);
        QPainter fadePainter(&faded);
        fadePainter.setOpacity(color.alphaF());
        fadePainter.drawPixmap(0, 0, result);
        fadePainter.end();
        result = faded;
    }

    cache().insert(key, result);
    return result;
}

QPixmap pixmap(const QString &name, int size, const QColor &color, qreal devicePixelRatio)
{
    return pixmap(name, QSize(size, size), color, devicePixelRatio);
}

QIcon icon(const QString &name, int size, const QColor &color)
{
    QIcon result;
    for (qreal dpr : {1.0, 1.5, 2.0}) {
        const QPixmap pm = pixmap(name, QSize(size, size), color, dpr);
        if (!pm.isNull())
            result.addPixmap(pm);
    }
    return result;
}

void clearCache()
{
    cache().clear();
    sourceCache().clear();
}

} // namespace IconProvider
