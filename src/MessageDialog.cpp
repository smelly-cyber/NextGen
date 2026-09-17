#include "MessageDialog.h"

#include "IconProvider.h"
#include "ModernLineEdit.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace {
struct ToneStyle
{
    QString icon;
    QColor color;
};

ToneStyle styleFor(MessageDialog::Tone tone)
{
    const Theme::Palette &c = Theme::colors();
    switch (tone) {
    case MessageDialog::Tone::Success:
        return {QStringLiteral("check"), c.success};
    case MessageDialog::Tone::Warning:
    case MessageDialog::Tone::Danger:
        return {QStringLiteral("info"), c.danger};
    case MessageDialog::Tone::Question:
        return {QStringLiteral("info"), c.primaryBright};
    case MessageDialog::Tone::Info:
    default:
        return {QStringLiteral("info"), c.link};
    }
}
} // namespace

MessageDialog::MessageDialog(QWidget *parent, const QString &title)
    : FramelessDialog(parent)
{
    const Theme::Palette &c = Theme::colors();

    setHeaderTitle(title);
    setMinimumWidth(480);
    setMaximumWidth(560);

    m_icon = new QLabel;
    m_icon->setFixedWidth(34);

    m_heading = new QLabel;
    m_heading->setFont(Theme::font(20, QFont::Bold));
    m_heading->setWordWrap(true);
    m_heading->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    auto *headingRow = new QHBoxLayout;
    headingRow->setContentsMargins(0, 0, 0, 0);
    headingRow->setSpacing(12);
    // Both centred on the same baseline: aligning the glyph to the top left it
    // floating above a single-line heading.
    headingRow->addWidget(m_icon, 0, Qt::AlignVCenter);
    headingRow->addWidget(m_heading, 1, Qt::AlignVCenter);

    m_body = new QLabel;
    m_body->setTextFormat(Qt::RichText);
    m_body->setFont(Theme::font(14));
    m_body->setWordWrap(true);
    m_body->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));
    m_body->hide();

    m_input = new ModernLineEdit;
    m_input->hide();

    m_buttonRow = new QHBoxLayout;
    m_buttonRow->setContentsMargins(0, 0, 0, 0);
    m_buttonRow->setSpacing(10);
    m_buttonRow->addStretch(1);

    QVBoxLayout *content = contentLayout();
    content->addLayout(headingRow);
    content->addWidget(m_body);
    content->addWidget(m_input);
    content->addSpacing(18);
    content->addLayout(m_buttonRow);

    setTone(Tone::Info);
}

void MessageDialog::setTone(Tone tone)
{
    const ToneStyle style = styleFor(tone);
    m_icon->setPixmap(IconProvider::pixmap(style.icon, 28, style.color, devicePixelRatioF()));
}

void MessageDialog::setHeading(const QString &heading)
{
    m_heading->setText(heading);
}

void MessageDialog::setBody(const QString &richText)
{
    m_body->setText(richText);
    m_body->setVisible(!richText.isEmpty());
    // A little breathing room between the heading and the body.
    m_body->setContentsMargins(46, 8, 0, 0);
}

void MessageDialog::enableInput(const QString &placeholder, const QString &initial)
{
    m_input->setPlaceholderText(placeholder);
    m_input->setText(initial);
    m_input->show();
    m_input->setContentsMargins(0, 14, 0, 0);
    connect(m_input, &ModernLineEdit::returnPressed, this, [this] {
        // Enter confirms with the highest (rightmost / primary) result code.
        m_result = 1;
        accept();
    });
}

QString MessageDialog::inputText() const
{
    return m_input->text().trimmed();
}

void MessageDialog::addButton(const QString &text, ModernButton::Variant variant, int resultCode,
                              bool isDefault)
{
    auto *button = new ModernButton(text, variant, this);
    button->setFont(Theme::font(14, QFont::Medium));
    button->setFixedHeight(50);
    m_buttonRow->addWidget(button, 0);

    connect(button, &ModernButton::clicked, this, [this, resultCode] {
        m_result = resultCode;
        accept();
    });
    if (isDefault)
        button->setFocus();
}

void MessageDialog::onCloseRequested()
{
    m_result = -1;
    reject();
}

int MessageDialog::run()
{
    exec();
    return m_result;
}

void MessageDialog::information(QWidget *parent, const QString &title, const QString &heading,
                                const QString &body, Tone tone)
{
    MessageDialog dialog(parent, title);
    dialog.setTone(tone);
    dialog.setHeading(heading);
    dialog.setBody(body);
    dialog.addButton(QObject::tr("OK"), ModernButton::Variant::Primary, 1, true);
    dialog.run();
}

bool MessageDialog::confirm(QWidget *parent, const QString &title, const QString &heading,
                            const QString &body, const QString &confirmText, Tone tone,
                            ModernButton::Variant confirmVariant)
{
    MessageDialog dialog(parent, title);
    dialog.setTone(tone);
    dialog.setHeading(heading);
    dialog.setBody(body);
    dialog.addButton(QObject::tr("Cancel"), ModernButton::Variant::Secondary, 0);
    dialog.addButton(confirmText, confirmVariant, 1, true);
    return dialog.run() == 1;
}

QString MessageDialog::getText(QWidget *parent, const QString &title, const QString &heading,
                               const QString &placeholder, const QString &initial, bool *ok)
{
    MessageDialog dialog(parent, title);
    dialog.setTone(Tone::Question);
    dialog.setHeading(heading);
    dialog.enableInput(placeholder, initial);
    dialog.addButton(QObject::tr("Cancel"), ModernButton::Variant::Secondary, 0);
    dialog.addButton(QObject::tr("Save"), ModernButton::Variant::Primary, 1, true);

    const int result = dialog.run();
    if (ok)
        *ok = (result == 1);
    return result == 1 ? dialog.inputText() : QString();
}
