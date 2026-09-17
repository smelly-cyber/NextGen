// DetailRow.h - "Label: value" line inside the System Info panels.
//
// An optional leading icon, a muted label on the left, the value on the right
// and a hairline underneath.
#pragma once

#include <QString>
#include <QWidget>

class DetailRow : public QWidget
{
    Q_OBJECT

public:
    DetailRow(const QString &label, const QString &iconName = QString(),
              QWidget *parent = nullptr);

    void setValue(const QString &value);
    QString value() const { return m_value; }

    /// Draws the hairline under the row (off for the last row in a panel).
    void setSeparatorVisible(bool visible);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_label;
    QString m_iconName;
    QString m_value;
    bool m_separator = true;
};
