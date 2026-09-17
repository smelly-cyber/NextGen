#include "AccountMenu.h"

#include "ModernButton.h"
#include "Theme.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QVBoxLayout>

AccountMenu::AccountMenu(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_signOut = new ModernButton(tr("Sign out"), ModernButton::Variant::Secondary, this);
    m_signOut->setIconName(QStringLiteral("logout"));
    m_signOut->setFont(Theme::font(14, QFont::Medium));
    m_signOut->setFixedHeight(46);
    m_signOut->setMinimumWidth(168);

    connect(m_signOut, &ModernButton::clicked, this, [this] {
        close();
        emit signOutClicked();
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);
    layout->addWidget(m_signOut);
}

void AccountMenu::popupUnder(QWidget *anchor)
{
    adjustSize();

    QPoint pos;
    if (anchor) {
        // Right-align the popover under the avatar.
        const QPoint topRight = anchor->mapToGlobal(QPoint(anchor->width(), anchor->height()));
        pos = QPoint(topRight.x() - width(), topRight.y() + 8);

        // Keep it on screen.
        if (QScreen *screen = anchor->screen()) {
            const QRect avail = screen->availableGeometry();
            pos.setX(qBound(avail.left() + 8, pos.x(), avail.right() - width() - 8));
            pos.setY(qMin(pos.y(), avail.bottom() - height() - 8));
        }
    }

    move(pos);
    show();
    raise();
    activateWindow();
}

void AccountMenu::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(body, 12, 12);
    painter.fillPath(path, c.cardTop);

    QPen border(Theme::alpha(c.windowBorder, 150));
    border.setWidthF(1.0);
    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, 12, 12);
}
