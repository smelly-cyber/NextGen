// StatTile.h - Compact metric tile used across the top of the Enhance page.
//
// Either a donut gauge (percentages) or a large plain value, with an icon above
// and a caption below.
#pragma once

#include <QString>
#include <QWidget>

class DonutGauge;

class StatTile : public QWidget
{
    Q_OBJECT

public:
    enum class Mode {
        Gauge, ///< Donut ring with the percentage in the middle.
        Value  ///< Large number with an optional unit suffix.
    };

    StatTile(const QString &iconName, const QString &caption, Mode mode,
             QWidget *parent = nullptr);

    /// Percentage for Gauge mode, raw number for Value mode. Negative means the
    /// value could not be measured.
    void setValue(qreal value);
    /// Suffix drawn after the number in Value mode ("s", "" ...).
    void setUnit(const QString &unit);
    /// Overrides the Value-mode text with a pre-formatted string (e.g. "2h 14m").
    /// Pass an empty string to fall back to the numeric value.
    void setText(const QString &text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QString m_iconName;
    QString m_caption;
    QString m_unit;
    QString m_text; ///< When set, shown verbatim in Value mode instead of m_value.
    Mode m_mode;
    qreal m_value = -1.0;

    DonutGauge *m_gauge = nullptr;
};
