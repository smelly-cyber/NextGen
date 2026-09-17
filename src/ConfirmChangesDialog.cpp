#include "ConfirmChangesDialog.h"

#include "ModernButton.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

ConfirmChangesDialog::ConfirmChangesDialog(const QString &title, const QString &subtitle,
                                           const QStringList &changes, QWidget *parent)
    : FramelessDialog(parent)
    , m_subtitleText(subtitle)
{
    const Theme::Palette &c = Theme::colors();

    setHeaderTitle(title);
    setMinimumSize(660, 520);

    auto *heading = new QLabel(title, this);
    heading->setFont(Theme::font(24, QFont::Bold));
    heading->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    m_subtitle = new QLabel(subtitle, this);
    m_subtitle->setFont(Theme::font(14));
    m_subtitle->setWordWrap(true);
    m_subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    m_notices = new QLabel(this);
    m_notices->setFont(Theme::font(13, QFont::Medium));
    m_notices->setWordWrap(true);
    m_notices->hide();

    m_changes = new QPlainTextEdit(this);
    m_changes->setReadOnly(true);
    m_changes->setFont(Theme::font(12));
    m_changes->setPlainText(changes.join(QLatin1Char('\n')));
    m_changes->setStyleSheet(
        QStringLiteral("QPlainTextEdit {"
                       "  background-color: %1;"
                       "  border: 1px solid %2;"
                       "  border-radius: 10px;"
                       "  padding: 12px;"
                       "  color: %3;"
                       "}")
            .arg(c.fieldBackground.name(), c.cardBorder.name(), c.textSecondary.name()));

    m_cancel = new ModernButton(tr("Cancel"), ModernButton::Variant::Secondary, this);
    m_cancel->setFont(Theme::font(14, QFont::Medium));
    m_cancel->setFixedHeight(52);

    m_elevate = new ModernButton(tr("Restart as Administrator"),
                                 ModernButton::Variant::Secondary, this);
    m_elevate->setIconName(QStringLiteral("shield_check"));
    m_elevate->setFont(Theme::font(14, QFont::Medium));
    m_elevate->setFixedHeight(52);
    m_elevate->hide();

    m_confirm = new ModernButton(tr("Apply Changes"), ModernButton::Variant::Primary, this);
    m_confirm->setIconName(QStringLiteral("check"));
    m_confirm->setFont(Theme::font(15, QFont::DemiBold));
    m_confirm->setFixedHeight(52);

    connect(m_cancel, &ModernButton::clicked, this, &QDialog::reject);
    connect(m_confirm, &ModernButton::clicked, this, &QDialog::accept);
    connect(m_elevate, &ModernButton::clicked, this, [this] {
        m_elevationRequested = true;
        accept();
    });

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    buttons->addWidget(m_cancel, 0);
    buttons->addStretch(1);
    buttons->addWidget(m_elevate, 0);
    buttons->addWidget(m_confirm, 0);

    setContentMargins(28, 22, 28, 24);
    QVBoxLayout *layout = contentLayout();
    layout->addWidget(heading);
    layout->addSpacing(6);
    layout->addWidget(m_subtitle);
    layout->addSpacing(14);
    layout->addWidget(m_notices);
    layout->addSpacing(10);
    layout->addWidget(m_changes, 1);
    layout->addSpacing(18);
    layout->addLayout(buttons);
}

void ConfirmChangesDialog::setNeedsAdmin(bool needsAdmin, bool alreadyElevated)
{
    m_needsAdmin = needsAdmin;
    m_elevated = alreadyElevated;

    // Without an elevated token the machine-level writes simply cannot happen,
    // so offer the restart rather than letting the user hit a wall of errors.
    const bool blocked = needsAdmin && !alreadyElevated;
    m_elevate->setVisible(blocked);
    m_confirm->setEnabled(!blocked);
    rebuildNotices();
}

void ConfirmChangesDialog::setNeedsReboot(bool needsReboot)
{
    m_needsReboot = needsReboot;
    rebuildNotices();
}

void ConfirmChangesDialog::setDestructive(bool destructive)
{
    m_destructive = destructive;
    rebuildNotices();
}

void ConfirmChangesDialog::setConfirmText(const QString &text)
{
    m_confirm->setText(text);
}

void ConfirmChangesDialog::rebuildNotices()
{
    const Theme::Palette &c = Theme::colors();
    QStringList lines;

    if (m_needsAdmin && !m_elevated) {
        lines << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                     .arg(c.danger.name(),
                          tr("Some of these need administrator rights. Restart as "
                             "administrator to continue."));
    } else if (m_needsAdmin) {
        lines << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                     .arg(c.link.name(),
                          tr("Running as administrator. A system restore point is created "
                             "first."));
    }

    if (m_destructive) {
        lines << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                     .arg(c.danger.name(),
                          tr("This permanently deletes files and cannot be undone by Revert."));
    }

    if (m_needsReboot) {
        lines << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                     .arg(c.textSecondary.name(),
                          tr("One or more changes only take effect after a restart."));
    }

    lines << QStringLiteral("<span style=\"color:%1;\">%2</span>")
                 .arg(c.textMuted.name(),
                      tr("Every setting below is recorded first, so Revert puts it back."));

    m_notices->setText(lines.join(QStringLiteral("<br>")));
    m_notices->setVisible(!lines.isEmpty());
}
