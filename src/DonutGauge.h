// DonutGauge.h - Circular percentage ring with the value in the centre.
//
// A negative value means "not measurable", which the gauge renders as a dash
// rather than inventing a number.
#pragma once

#include <QString>
#include <QWidget>

class QPropertyAnimation;

class DonutGauge : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal displayValue READ displayValue WRITE setDisplayValue)

public:
    explicit DonutGauge(QWidget *parent = nullptr);
    ~DonutGauge() override;

    /// Percentage 0..100, or a negative number when unavailable.
    void setValue(qreal percent);
    qreal value() const { return m_value; }

    /// Text drawn under the number instead of "%" (e.g. "s" for seconds).
    void setUnit(const QString &unit);
    /// Draws the value without a unit at all (e.g. a plain process count).
    void setShowUnit(bool show);

    /// Replaces the centre label entirely (e.g. "18 Mbps"). Pass an empty
    /// string to go back to the numeric value.
    void setCentreText(const QString &text);

    void setRingThickness(qreal thickness);
    void setValueFontSize(int pixelSize);

    qreal displayValue() const { return m_displayValue; }
    void setDisplayValue(qreal value);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_value = -1.0;
    qreal m_displayValue = 0.0;
    QString m_unit = QStringLiteral("%");
    QString m_centreText;
    bool m_showUnit = true;
    qreal m_ringThickness = 7.0;
    int m_valueFontSize = 20;
    QPropertyAnimation *m_animation = nullptr;
};
