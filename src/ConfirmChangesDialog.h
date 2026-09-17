// ConfirmChangesDialog.h - The gate every system change passes through.
//
// Lists the exact settings that will be written, flags administrator and
// reboot requirements, and will not enable its confirm button until the user
// has had the chance to read it.
#pragma once

#include "FramelessDialog.h"

#include <QStringList>

class ModernButton;
class QLabel;
class QPlainTextEdit;

class ConfirmChangesDialog : public FramelessDialog
{
    Q_OBJECT

public:
    ConfirmChangesDialog(const QString &title, const QString &subtitle,
                         const QStringList &changes, QWidget *parent = nullptr);

    void setNeedsAdmin(bool needsAdmin, bool alreadyElevated);
    void setNeedsReboot(bool needsReboot);
    void setDestructive(bool destructive);
    void setConfirmText(const QString &text);

    /// True when the user chose to restart the app elevated instead of applying.
    bool elevationRequested() const { return m_elevationRequested; }

private:
    void rebuildNotices();

    QString m_subtitleText;
    QLabel *m_subtitle = nullptr;
    QLabel *m_notices = nullptr;
    QPlainTextEdit *m_changes = nullptr;
    ModernButton *m_confirm = nullptr;
    ModernButton *m_cancel = nullptr;
    ModernButton *m_elevate = nullptr;

    bool m_needsAdmin = false;
    bool m_elevated = false;
    bool m_needsReboot = false;
    bool m_destructive = false;
    bool m_elevationRequested = false;
};
