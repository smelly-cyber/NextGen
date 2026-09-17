// Badge.h - Small pill label ("Recommended" / "Optional").
#pragma once

#include <QString>
#include <QWidget>

class Badge : public QWidget
{
    Q_OBJECT

public:
    enum class Style {
        Dot,      ///< Blue dot + label, no outline (Recommended).
        Outlined, ///< Muted label inside a thin pill (Optional).
        Plain     ///< Muted label, no decoration.
    };

    explicit Badge(const QString &text, Style style = Style::Dot, QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const { return m_text; }
    void setStyle(Style style);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    Style m_style;
};
