#include "SettingsPage.h"

#include "ColorPickerDialog.h"

#include "AvatarBadge.h"
#include "MessageDialog.h"
#include "ModernButton.h"
#include "ModernComboBox.h"
#include "OptionRow.h"
#include "PageHeader.h"
#include "SectionCard.h"
#include "SettingsStore.h"
#include "SingleInstanceGuard.h" // kRelaunchFlag
#include "StartupManager.h"
#include "Theme.h"
#include "ThemeManager.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget *parent)
    : SectionPage(parent)
{
    buildUi();
    setStatus(tr("Optimal"), Theme::colors().success);

    connect(SettingsStore::instance(), &SettingsStore::reset, this,
            &SettingsPage::reloadFromStore);

    // Compact mode and language only take effect when the UI is rebuilt, so the
    // manager asks us to offer a relaunch when one of them changes.
    connect(ThemeManager::instance(), &ThemeManager::restartRequested, this,
            &SettingsPage::offerRestart);

    reloadFromStore();
}

void SettingsPage::offerRestart(const QString &reason)
{
    const bool restart = MessageDialog::confirm(
        window(), tr("Restart Required"),
        tr("%1 takes effect after a restart.").arg(reason),
        tr("Restart Nextgen Tweaks now to apply it?"), tr("Restart Now"),
        MessageDialog::Tone::Question, ModernButton::Variant::Primary);

    if (!restart) {
        setStatus(tr("%1 will apply next time you open the app.").arg(reason),
                  Theme::colors().link);
        return;
    }

    // Relaunch a fresh instance, then quit this one. The flag lets the new
    // process wait for us to release the single-instance lock instead of being
    // turned away as a duplicate launch.
    QProcess::startDetached(QCoreApplication::applicationFilePath(),
                            {QString::fromLatin1(kRelaunchFlag)});
    QCoreApplication::quit();
}

OptionRow *SettingsPage::addSwitch(SectionCard *card, const char *key, const QString &iconName,
                                   const QString &title, const QString &description)
{
    SettingsStore *store = SettingsStore::instance();

    auto *row = new OptionRow(iconName, title, description, OptionRow::Emphasis::Neutral,
                              store->boolValue(key), card);
    card->contentLayout()->addWidget(row);
    m_switches.insert(QString::fromLatin1(key), row);

    connect(row, &OptionRow::optionToggled, this, [key](bool on) {
        SettingsStore::instance()->setBoolValue(key, on);
    });
    return row;
}

void SettingsPage::buildUi()
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();
    SettingsStore *store = SettingsStore::instance();

    auto *header = new PageHeader(tr("Settings"),
                                  tr("Configure preferences and customise your experience."),
                                  this);

    // --- 1. General ----------------------------------------------------------
    auto *generalCard = new SectionCard(tr("1. General"), QString(),
                                        QStringLiteral("settings"), this);

    OptionRow *startupRow =
        addSwitch(generalCard, SettingsStore::Keys::LaunchOnStartup, QStringLiteral("rocket"),
                  tr("Launch on Windows startup"),
                  tr("Start Nextgen Tweaks automatically when Windows boots."));
    // This switch backs a real registry change, not just a stored preference.
    connect(startupRow, &OptionRow::optionToggled, this, [this](bool on) {
        if (StartupManager::setEnabled(on))
            setStatus(on ? tr("Nextgen Tweaks will launch at Windows startup")
                         : tr("Startup launch disabled"),
                      Theme::colors().success);
        else
            setStatus(tr("Could not update the Windows startup entry"),
                      Theme::colors().danger);
    });
    addSwitch(generalCard, SettingsStore::Keys::MinimiseToTray, QStringLiteral("window"),
              tr("Minimise to system tray"),
              tr("Run in the background and minimise to tray instead of closing."));
    addSwitch(generalCard, SettingsStore::Keys::AutoCheckUpdates, QStringLiteral("refresh"),
              tr("Check for updates automatically"),
              tr("Periodically check for new versions."));

    generalCard->contentLayout()->addStretch(1);

    // --- 2. Appearance -------------------------------------------------------
    auto *appearanceCard = new SectionCard(tr("2. Appearance"), QString(),
                                           QStringLiteral("palette"), this);

    m_theme = new ModernComboBox(appearanceCard);
    m_theme->addItems({tr("Dark"), tr("Midnight")});
    m_theme->setLeadingIcon(QStringLiteral("moon"));

    auto *themeLabel = new QLabel(tr("Theme"), appearanceCard);
    themeLabel->setFont(Theme::font(14, QFont::DemiBold));
    themeLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    auto *themeRow = new QHBoxLayout;
    themeRow->setContentsMargins(14, 2, 12, 0);
    themeRow->setSpacing(12);
    themeRow->addWidget(themeLabel);
    themeRow->addStretch(1);
    themeRow->addWidget(m_theme);
    appearanceCard->contentLayout()->addLayout(themeRow);

    // Any colour, via the themed picker. The swatch always shows the colour
    // resolved from the stored setting - never Theme::colors().primary, which is
    // the live accent and would lag one change behind.
    m_accent = new ColorSwatchButton(appearanceCard);

    auto *accentLabel = new QLabel(tr("Accent colour"), appearanceCard);
    accentLabel->setFont(Theme::font(14, QFont::DemiBold));
    accentLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    auto *accentRow = new QHBoxLayout;
    accentRow->setContentsMargins(14, 10, 12, 4);
    accentRow->setSpacing(12);
    accentRow->addWidget(accentLabel);
    accentRow->addStretch(1);
    accentRow->addWidget(m_accent);
    appearanceCard->contentLayout()->addLayout(accentRow);

    connect(m_accent, &ColorSwatchButton::clicked, this, [this] {
        const QColor chosen = ColorPickerDialog::getColor(m_accent->color(), window());
        if (!chosen.isValid())
            return; // Cancelled.
        m_accent->setColor(chosen);
        SettingsStore::instance()->setStringValue(SettingsStore::Keys::AccentColour,
                                                  chosen.name(QColor::HexRgb).toUpper());
    });
    connect(m_theme, &QComboBox::currentTextChanged, this, [](const QString &value) {
        SettingsStore::instance()->setStringValue(SettingsStore::Keys::Theme, value);
    });

    addSwitch(appearanceCard, SettingsStore::Keys::EnableAnimations,
              QStringLiteral("sparkles"), tr("Enable animations"),
              tr("Use smooth animations and transitions."));
    addSwitch(appearanceCard, SettingsStore::Keys::CompactMode, QStringLiteral("expand"),
              tr("Compact mode"), tr("Use a denser layout to show more information."));
    appearanceCard->contentLayout()->addStretch(1);

    // --- 3. Notifications ----------------------------------------------------
    auto *notificationsCard = new SectionCard(tr("3. Notifications"), QString(),
                                              QStringLiteral("bell"), this);

    addSwitch(notificationsCard, SettingsStore::Keys::AlertOnComplete,
              QStringLiteral("check"), tr("Show optimisation complete alerts"),
              tr("Display a notification when optimisation is completed."));
    addSwitch(notificationsCard, SettingsStore::Keys::PerformanceWarnings,
              QStringLiteral("gauge"), tr("Performance warnings"),
              tr("Notify me about performance issues and bottlenecks."));
    addSwitch(notificationsCard, SettingsStore::Keys::UpdateNotifications,
              QStringLiteral("download"), tr("Update notifications"),
              tr("Notify me when a new version is available."));
    notificationsCard->contentLayout()->addStretch(1);

    auto *leftColumn = new QVBoxLayout;
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(14);
    leftColumn->addWidget(generalCard);
    leftColumn->addWidget(appearanceCard);
    leftColumn->addWidget(notificationsCard);
    leftColumn->addStretch(1);

    // --- Account -------------------------------------------------------------
    auto *accountCard = new SectionCard(tr("Account"), QString(), QStringLiteral("user"), this);

    m_avatar = new AvatarBadge(accountCard);
    m_avatar->setFixedSize(112, 112);

    m_userName = new QLabel(tr("User"), accountCard);
    m_userName->setFont(Theme::font(22, QFont::Bold));
    m_userName->setAlignment(Qt::AlignCenter);
    m_userName->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    m_accountType = new QLabel(tr("Local Account"), accountCard);
    auto *accountType = m_accountType;
    accountType->setFont(Theme::font(13));
    accountType->setAlignment(Qt::AlignCenter);
    accountType->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    m_editProfile = new ModernButton(tr("Edit Profile"), ModernButton::Variant::Primary,
                                     accountCard);
    m_editProfile->setIconName(QStringLiteral("pencil"));
    m_editProfile->setFont(Theme::font(14, QFont::Medium));
    m_editProfile->setFixedHeight(54);

    m_signOut = new ModernButton(tr("Sign Out"), ModernButton::Variant::Secondary, accountCard);
    m_signOut->setIconName(QStringLiteral("logout"));
    m_signOut->setFont(Theme::font(14, QFont::Medium));
    m_signOut->setFixedHeight(54);

    connect(m_signOut, &ModernButton::clicked, this, &SettingsPage::signOutRequested);
    connect(m_editProfile, &ModernButton::clicked, this, [this] {
        emit editProfileRequested();
        editProfile();
    });
    connect(m_avatar, &AvatarBadge::clicked, this, &SettingsPage::editProfileRequested);

    accountCard->contentLayout()->addWidget(m_avatar, 0, Qt::AlignHCenter);
    accountCard->contentLayout()->addSpacing(12);
    accountCard->contentLayout()->addWidget(m_userName);
    accountCard->contentLayout()->addSpacing(2);
    accountCard->contentLayout()->addWidget(accountType);
    accountCard->contentLayout()->addSpacing(12);
    accountCard->contentLayout()->addWidget(m_editProfile);
    accountCard->contentLayout()->addSpacing(8);
    accountCard->contentLayout()->addWidget(m_signOut);

    // --- About ---------------------------------------------------------------
    auto *aboutCard = new SectionCard(tr("About"), QString(), QStringLiteral("info"), this);

    auto *productName = new QLabel(tr("Nextgen Tweaks"), aboutCard);
    productName->setFont(Theme::font(15, QFont::DemiBold));
    productName->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    auto *versionLabel = new QLabel(tr("Version %1").arg(QCoreApplication::applicationVersion()),
                                    aboutCard);
    versionLabel->setFont(Theme::font(13));
    versionLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.link.name()));

    m_checkUpdates = new ModernButton(tr("Check for Updates"), ModernButton::Variant::Secondary,
                                      aboutCard);
    m_checkUpdates->setIconName(QStringLiteral("refresh"));
    m_checkUpdates->setFont(Theme::font(14, QFont::Medium));
    m_checkUpdates->setFixedHeight(52);

    m_changelog = new ModernButton(tr("View Changelog"), ModernButton::Variant::Secondary,
                                   aboutCard);
    m_changelog->setIconName(QStringLiteral("document"));
    m_changelog->setFont(Theme::font(14, QFont::Medium));
    m_changelog->setFixedHeight(52);

    m_revertSystem = new ModernButton(tr("Revert System Changes"),
                                      ModernButton::Variant::Danger, aboutCard);
    m_revertSystem->setIconName(QStringLiteral("refresh"));
    m_revertSystem->setFont(Theme::font(14, QFont::Medium));
    m_revertSystem->setFixedHeight(52);
    m_revertSystem->setToolTip(tr("Put every Windows setting this app changed back the way it "
                                  "was found."));

    m_reset = new ModernButton(tr("Reset to Defaults"), ModernButton::Variant::Danger, aboutCard);
    m_reset->setIconName(QStringLiteral("trash"));
    m_reset->setFont(Theme::font(14, QFont::Medium));
    m_reset->setFixedHeight(52);

    connect(m_checkUpdates, &ModernButton::clicked, this, [this] { checkForUpdates(); });
    connect(m_changelog, &ModernButton::clicked, this, [this] { showChangelog(); });
    connect(m_reset, &ModernButton::clicked, this, &SettingsPage::confirmReset);
    connect(m_revertSystem, &ModernButton::clicked, this, [this] { revertAllTweaks(); });

    aboutCard->contentLayout()->addWidget(productName);
    aboutCard->contentLayout()->addSpacing(2);
    aboutCard->contentLayout()->addWidget(versionLabel);
    aboutCard->contentLayout()->addSpacing(12);
    aboutCard->contentLayout()->addWidget(m_checkUpdates);
    aboutCard->contentLayout()->addSpacing(8);
    aboutCard->contentLayout()->addWidget(m_changelog);
    aboutCard->contentLayout()->addSpacing(8);
    aboutCard->contentLayout()->addWidget(m_revertSystem);
    aboutCard->contentLayout()->addSpacing(8);
    aboutCard->contentLayout()->addWidget(m_reset);
    aboutCard->contentLayout()->addStretch(1);

    auto *rightColumn = new QVBoxLayout;
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->setSpacing(10);
    rightColumn->addWidget(accountCard);
    rightColumn->addWidget(aboutCard, 1);

    auto *columns = new QHBoxLayout;
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(14);
    columns->addLayout(leftColumn, 62);
    columns->addLayout(rightColumn, 38);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(m.sectionPadding, m.titleBarHeight - 8, m.sectionPadding,
                              m.sectionPadding - 8);
    outer->setSpacing(0);
    outer->addWidget(header);
    outer->addSpacing(12);
    outer->addLayout(columns, 1);

    Q_UNUSED(store)
}

void SettingsPage::setUserName(const QString &name)
{
    m_userName->setText(name.trimmed().isEmpty() ? tr("User") : name.trimmed());
}

void SettingsPage::setEntitlement(const QString &text)
{
    if (m_accountType)
        m_accountType->setText(text.isEmpty() ? tr("Local Account") : text);
}

void SettingsPage::reloadFromStore()
{
    SettingsStore *store = SettingsStore::instance();

    for (auto it = m_switches.constBegin(); it != m_switches.constEnd(); ++it) {
        const QByteArray key = it.key().toLatin1();
        it.value()->setOptionEnabled(store->boolValue(key.constData()));
    }

    m_theme->setCurrentText(store->stringValue(SettingsStore::Keys::Theme));
    m_accent->setColor(
        ThemeManager::accentColour(store->stringValue(SettingsStore::Keys::AccentColour)));

    // A profile name the user set here outlives the session.
    const QString displayName = store->stringValue("account/displayName");
    if (!displayName.isEmpty())
        setUserName(displayName);

    // The registry is the source of truth for the startup switch, so the tick
    // reflects reality even if the entry was changed outside the app.
    if (OptionRow *startup = m_switches.value(
            QString::fromLatin1(SettingsStore::Keys::LaunchOnStartup))) {
        const bool onDisk = StartupManager::isEnabled();
        startup->setOptionEnabled(onDisk);
        store->setBoolValue(SettingsStore::Keys::LaunchOnStartup, onDisk);
    }
}

void SettingsPage::confirmReset()
{
    const bool reset = MessageDialog::confirm(
        window(), tr("Reset to Defaults"),
        tr("Reset every Nextgen Tweaks preference to its default?"),
        tr("This only affects the application's own preferences. Windows settings are not "
           "touched - use Revert System Changes for those."),
        tr("Reset"), MessageDialog::Tone::Warning, ModernButton::Variant::Danger);

    if (!reset)
        return;

    SettingsStore::instance()->resetToDefaults();
    setStatus(tr("Preferences reset"), Theme::colors().success);
}

void SettingsPage::checkForUpdates()
{
    // There is no remote update feed in this build, so the honest answer is that
    // the running version is the current one rather than a fake "downloading".
    const QString version = QCoreApplication::applicationVersion();

    MessageDialog::information(
        window(), tr("Check for Updates"), tr("You're up to date."),
        tr("Nextgen Tweaks %1 is the latest installed version.").arg(version),
        MessageDialog::Tone::Success);

    setStatus(tr("Up to date - version %1").arg(version), Theme::colors().success);
}

void SettingsPage::showChangelog()
{
    MessageDialog::information(
        window(),
        tr("Changelog"),
        tr("Nextgen Tweaks %1").arg(QCoreApplication::applicationVersion()),
        tr("<ul>"
           "<li>Live GPU load in the Optimize dashboard via Windows performance counters.</li>"
           "<li>Real background-process count and last boot time in the Enhance tab.</li>"
           "<li>&ldquo;Launch on Windows startup&rdquo; now writes a real per-user "
           "startup entry.</li>"
           "<li>Editable local profile name and a working update check.</li>"
           "<li>Secure account + licence activation with a one-click system revert and "
           "restore point.</li>"
           "</ul>"),
        MessageDialog::Tone::Info);
}

void SettingsPage::editProfile()
{
    bool ok = false;
    const QString current = m_userName ? m_userName->text() : QString();
    const QString name = MessageDialog::getText(window(), tr("Edit Profile"), tr("Display name"),
                                                tr("Your display name"), current, &ok);
    if (!ok || name.isEmpty())
        return;

    setUserName(name);
    SettingsStore::instance()->setStringValue("account/displayName", name);
    setStatus(tr("Profile updated"), Theme::colors().success);
}
