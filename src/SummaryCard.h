// SummaryCard.h - Icon + title + a short value, used across the top of the
// System Info page.
#pragma once

#include <QString>
#include <QWidget>

class SummaryCard : public QWidget
{
    Q_OBJECT

public:
    SummaryCard(const QString &iconName, const QString &title, QWidget *parent = nullptr);

    void setValue(const QString &value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_iconName;
    QString m_title;
    QString m_value;
};
