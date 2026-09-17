// AvatarBadge.h - Circular user avatar with a presence dot.
//
// Clickable so it can open the account menu once that exists.
#pragma once

#include <QAbstractButton>

class QPropertyAnimation;

class AvatarBadge : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit AvatarBadge(QWidget *parent = nullptr);
    ~AvatarBadge() override;

    void setOnline(bool online);
    bool isOnline() const { return m_online; }

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    bool m_online = true;
    qreal m_hoverProgress = 0.0;
    QPropertyAnimation *m_hoverAnimation = nullptr;
};
