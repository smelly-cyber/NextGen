// ScoreBox.h - "Before 62/100" / "After (Estimated) 91/100" panel.
#pragma once

#include <QString>
#include <QWidget>

class ScoreBox : public QWidget
{
    Q_OBJECT

public:
    enum class Tone {
        Measured,  ///< Neutral: a value we actually read.
        Projected  ///< Accented: a modelled estimate.
    };

    ScoreBox(const QString &caption, Tone tone, QWidget *parent = nullptr);

    /// Score out of 100; negative renders a dash.
    void setScore(qreal score);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_caption;
    Tone m_tone;
    qreal m_score = -1.0;
};
