// StarRating.h - Five star read-out for the performance score.
#pragma once

#include <QWidget>

class StarRating : public QWidget
{
    Q_OBJECT

public:
    explicit StarRating(QWidget *parent = nullptr);

    /// Score out of 100; negative hides the stars.
    void setScore(qreal score);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_score = -1.0;
};
