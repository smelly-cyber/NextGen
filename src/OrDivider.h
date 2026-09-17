// OrDivider.h - "--------  OR  --------" separator drawn with fading blue lines.
#pragma once

#include <QString>
#include <QWidget>

class OrDivider : public QWidget
{
    Q_OBJECT

public:
    explicit OrDivider(const QString &label = QStringLiteral("OR"), QWidget *parent = nullptr);

    void setLabel(const QString &label);
    QString label() const { return m_label; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_label;
};
