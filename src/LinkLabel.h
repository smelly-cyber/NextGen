// LinkLabel.h - Clickable, keyboard focusable text link ("Forgot password?").
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

class QPropertyAnimation;

class LinkLabel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit LinkLabel(const QString &text, QWidget *parent = nullptr);
    ~LinkLabel() override;

    void setText(const QString &text);
    QString text() const { return m_text; }

    void setColors(const QColor &normal, const QColor &hover);

    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QString m_text;
    QColor m_normalColor;
    QColor m_hoverColor;
    qreal m_hoverProgress = 0.0;
    QPropertyAnimation *m_hoverAnimation = nullptr;
};
