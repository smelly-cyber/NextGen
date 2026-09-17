// UserCard.h - The account panel in the top right of the home screen.
//
// Avatar, display name, an "Online" presence dot and a chevron that opens the
// account menu. The whole card is clickable, which is what the chevron hints at.
#pragma once

#include <QString>
#include <QWidget>

class QVariantAnimation;

class UserCard : public QWidget
{
    Q_OBJECT

public:
    explicit UserCard(QWidget *parent = nullptr);

    void setUserName(const QString &name);
    QString userName() const { return m_userName; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QString m_userName;
    qreal m_hover = 0.0;
    QVariantAnimation *m_hoverAnimation = nullptr;
};
