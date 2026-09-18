#include "SocialLinksBar.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QToolButton>
#include <QUrl>

namespace {
// -------------------------------------------------------------------------
//  REPLACE THESE with the real links. Each icon opens its URL in the browser.
// -------------------------------------------------------------------------
const char *kDiscordUrl = "https://discord.gg/5pQUCbjA8V";
const char *kTikTokUrl = "https://www.tiktok.com/@your-handle";
const char *kYouTubeUrl = "https://www.youtube.com/@your-channel";
} // namespace

SocialLinksBar::SocialLinksBar(QWidget *parent)
    : QWidget(parent)
{
    m_row = new QHBoxLayout(this);
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(6);

    addIcon(QStringLiteral("discord"), QString::fromLatin1(kDiscordUrl), tr("Join our Discord"));
    addIcon(QStringLiteral("tiktok"), QString::fromLatin1(kTikTokUrl), tr("Follow us on TikTok"));
    addIcon(QStringLiteral("youtube"), QString::fromLatin1(kYouTubeUrl),
            tr("Subscribe on YouTube"));
    m_row->addStretch(1);
}

void SocialLinksBar::setIconSize(int pixels)
{
    m_iconSize = pixels;
    for (QToolButton *button : findChildren<QToolButton *>())
        button->setIconSize(QSize(pixels, pixels));
}

void SocialLinksBar::addIcon(const QString &asset, const QString &url, const QString &tooltip)
{
    auto *button = new QToolButton(this);
    button->setIcon(QIcon(QStringLiteral(":/assets/logo/%1.png").arg(asset)));
    button->setIconSize(QSize(m_iconSize, m_iconSize));
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; background: transparent; padding: 5px; border-radius: 8px; }"
        "QToolButton:hover { background: rgba(255, 255, 255, 0.10); }"
        "QToolButton:pressed { background: rgba(255, 255, 255, 0.16); }"));

    connect(button, &QToolButton::clicked, this, [url] {
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    });

    m_row->addWidget(button, 0, Qt::AlignVCenter);
}
