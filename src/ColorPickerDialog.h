// ColorPickerDialog.h - Themed colour picker for the accent colour.
//
// QColorDialog would open a stock Windows-style dialog with the native title
// bar, which every other popup in the app has deliberately replaced. This one
// is built on FramelessDialog: a saturation/brightness square, a hue strip, a
// hex field, a row of quick presets, and a live preview.
#pragma once

#include "FramelessDialog.h"

#include <QColor>
#include <QWidget>

class ModernLineEdit;

/// Square picking saturation (x) and brightness (y) for one hue.
class SaturationValueArea : public QWidget
{
    Q_OBJECT

public:
    explicit SaturationValueArea(QWidget *parent = nullptr);

    void setHue(int hue);
    void setSaturationValue(int saturation, int value);

signals:
    void picked(int saturation, int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void pickAt(const QPointF &position);

    int m_hue = 210;
    int m_saturation = 255;
    int m_value = 255;
    QImage m_cache; ///< Rendered gradient for the current hue and size.
};

/// Horizontal strip picking the hue.
class HueStrip : public QWidget
{
    Q_OBJECT

public:
    explicit HueStrip(QWidget *parent = nullptr);

    void setHue(int hue);
    int hue() const { return m_hue; }

signals:
    void picked(int hue);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void pickAt(qreal x);

    int m_hue = 210;
};

class ColorPickerDialog : public FramelessDialog
{
    Q_OBJECT

public:
    explicit ColorPickerDialog(const QColor &initial, QWidget *parent = nullptr);

    QColor color() const { return m_color; }

    /// Opens the picker; returns the chosen colour, or an invalid colour if the
    /// user cancelled.
    static QColor getColor(const QColor &initial, QWidget *parent);

private:
    void setColor(const QColor &color, bool updateHexField = true);

    QColor m_color;
    SaturationValueArea *m_area = nullptr;
    HueStrip *m_hue = nullptr;
    ModernLineEdit *m_hex = nullptr;
    QWidget *m_preview = nullptr;
};

/// The control on the Settings page: looks like the other fields, shows the
/// current colour and its hex code, and opens the picker when clicked.
class ColorSwatchButton : public QWidget
{
    Q_OBJECT

public:
    explicit ColorSwatchButton(QWidget *parent = nullptr);

    void setColor(const QColor &color);
    QColor color() const { return m_color; }

    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QColor m_color;
    bool m_hovered = false;
};
