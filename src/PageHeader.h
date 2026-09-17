// PageHeader.h - Big section title with an accent rule and a subtitle.
#pragma once

#include <QString>
#include <QWidget>

class PageHeader : public QWidget
{
    Q_OBJECT

public:
    PageHeader(const QString &title, const QString &subtitle, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title;
    QString m_subtitle;
};
