#include "StatusStrip.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>

namespace {
constexpr int kSidePadding = 26;
constexpr int kShieldSize = 30;
constexpr int kItemGap = 14;
} // namespace

StatusStrip::StatusStrip(QWidget *parent)
    : QWidget(parent)
{
    m_status = tr("Optimal");
    m_statusColor = Theme::colors().success;
    m_version = tr("Version %1").arg(QCoreApplication::applicationVersion());

    setFixedHeight(Theme::metrics().statusBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void StatusStrip::setStatus(const QString &text, const QColor &color)
{
    m_status = text;
    m_statusColor = color;
    update();
}

void StatusStrip::setVersion(const QString &version)
{
    m_version = version;
    update();
}

void StatusStrip::setTrailingText(const QString &text)
{
    m_trailing = text;
    update();
}

QSize StatusStrip::sizeHint() const
{
    return QSize(600, Theme::metrics().statusBarHeight);
}

void StatusStrip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF body(rect());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // --- Background ---------------------------------------------------------
    QLinearGradient base(body.topLeft(), body.bottomLeft());
    base.setColorAt(0.0, c.statusBarTop);
    base.setColorAt(1.0, c.statusBarBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(body);

    const qreal centreY = body.center().y();
    qreal x = kSidePadding;

    // --- Shield -------------------------------------------------------------
    const QPixmap shield = IconProvider::pixmap(QStringLiteral("shield_check"), kShieldSize,
                                                c.primaryBright, devicePixelRatioF());
    if (!shield.isNull()) {
        painter.drawPixmap(QPointF(x, centreY - kShieldSize / 2.0), shield);
        x += kShieldSize + kItemGap;
    }

    // --- "System Status: Optimal" -------------------------------------------
    const QFont labelFont = Theme::font(15, QFont::Medium);
    const QFont valueFont = Theme::font(15, QFont::DemiBold);
    const QFontMetrics labelMetrics(labelFont);
    const QFontMetrics valueMetrics(valueFont);

    const QString label = tr("System Status:");
    painter.setFont(labelFont);
    painter.setPen(c.textPrimary);
    painter.drawText(QRectF(x, body.top(), labelMetrics.horizontalAdvance(label) + 2,
                            body.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, label);
    x += labelMetrics.horizontalAdvance(label) + 8;

    painter.setFont(valueFont);
    painter.setPen(m_statusColor);
    painter.drawText(QRectF(x, body.top(), valueMetrics.horizontalAdvance(m_status) + 2,
                            body.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_status);
    x += valueMetrics.horizontalAdvance(m_status) + 26;

    const QFont versionFont = Theme::font(14);
    const QFontMetrics versionMetrics(versionFont);

    const auto drawDivider = [&](qreal at) {
        QLinearGradient divider(0, centreY - 12, 0, centreY + 12);
        divider.setColorAt(0.0, Theme::alpha(c.panelDivider, 0));
        divider.setColorAt(0.5, c.panelDivider);
        divider.setColorAt(1.0, Theme::alpha(c.panelDivider, 0));
        painter.setPen(QPen(QBrush(divider), 1.0));
        painter.drawLine(QPointF(at, centreY - 12), QPointF(at, centreY + 12));
    };

    if (!m_trailing.isEmpty()) {
        // Section-page layout: version and tagline pinned to the right.
        const QFont trailingFont = Theme::font(14);
        const QFontMetrics trailingMetrics(trailingFont);

        const int trailingWidth = trailingMetrics.horizontalAdvance(m_trailing);
        const int versionWidth = versionMetrics.horizontalAdvance(m_version);

        qreal right = body.width() - kSidePadding;
        painter.setFont(trailingFont);
        painter.setPen(c.textSecondary);
        painter.drawText(QRectF(right - trailingWidth, body.top(), trailingWidth + 2,
                                body.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_trailing);
        right -= trailingWidth + 26;

        drawDivider(right);
        right -= 26;

        painter.setFont(versionFont);
        painter.setPen(c.textSecondary);
        painter.drawText(QRectF(right - versionWidth, body.top(), versionWidth + 2,
                                body.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_version);
        return;
    }

    // --- Menu layout: version beside the status, accent line trailing off ----
    drawDivider(x);
    x += 26;

    painter.setFont(versionFont);
    painter.setPen(c.textSecondary);
    painter.drawText(QRectF(x, body.top(), versionMetrics.horizontalAdvance(m_version) + 2,
                            body.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_version);
    x += versionMetrics.horizontalAdvance(m_version);

    const qreal lineLeft = qMax(x + 40.0, body.width() * 0.55);
    const qreal lineRight = body.width() - kSidePadding;
    if (lineRight > lineLeft) {
        QLinearGradient accent(lineLeft, 0, lineRight, 0);
        accent.setColorAt(0.0, Qt::transparent);
        accent.setColorAt(0.55, Theme::alpha(c.primary, 150));
        accent.setColorAt(0.85, c.cyan);
        accent.setColorAt(1.0, Theme::alpha(c.cyan, 0));

        QPen accentPen(QBrush(accent), 2.0);
        accentPen.setCapStyle(Qt::RoundCap);
        painter.setPen(accentPen);
        painter.drawLine(QPointF(lineLeft, centreY + 8), QPointF(lineRight, centreY + 8));
    }
}
