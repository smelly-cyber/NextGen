// SocialLinksBar.h - A row of clickable social icons (Discord, TikTok, YouTube).
//
// Shows the brand's social logos from assets/logo and opens each one's URL in
// the default browser when clicked. The target URLs are placeholders declared in
// SocialLinksBar.cpp - replace them with the real links there.
#pragma once

#include <QWidget>

class QHBoxLayout;

class SocialLinksBar : public QWidget
{
    Q_OBJECT

public:
    explicit SocialLinksBar(QWidget *parent = nullptr);

    /// Sets the rendered icon size in logical pixels (default 26).
    void setIconSize(int pixels);

private:
    void addIcon(const QString &asset, const QString &url, const QString &tooltip);

    QHBoxLayout *m_row = nullptr;
    int m_iconSize = 26;
};
