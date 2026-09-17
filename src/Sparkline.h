// Sparkline.h - Tiny history plot drawn along the bottom of a metric card.
#pragma once

#include <QList>
#include <QWidget>

class Sparkline : public QWidget
{
    Q_OBJECT

public:
    explicit Sparkline(QWidget *parent = nullptr);

    /// Values are percentages, oldest first.
    void setValues(const QList<qreal> &values);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<qreal> m_values;
};
