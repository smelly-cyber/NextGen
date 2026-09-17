// StatusStrip.h - Bottom bar of the main window.
//
// Shield icon + system status, a hairline separator, the app version, and a
// soft blue accent line trailing off to the right.
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

class StatusStrip : public QWidget
{
    Q_OBJECT

public:
    explicit StatusStrip(QWidget *parent = nullptr);

    /// Sets the status word and the colour it is drawn in (green / amber / red).
    void setStatus(const QString &text, const QColor &color);
    void setVersion(const QString &version);

    /// Right hand tagline. When set, the strip switches to the section-page
    /// layout (status left, version + tagline right) instead of the accent line.
    void setTrailingText(const QString &text);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_status;
    QColor m_statusColor;
    QString m_version;
    QString m_trailing;
};
