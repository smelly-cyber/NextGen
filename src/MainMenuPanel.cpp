#include "MainMenuPanel.h"

#include "AccountMenu.h"
#include "IconProvider.h"
#include "Theme.h"
#include "UserCard.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QVBoxLayout>

namespace {

/// Declarative description of the home grid, so adding a section is a one line
/// change. `tags` are the capability chips; an empty list gives the short card.
struct SectionSpec
{
    MenuSection section;
    const char *iconName;
    const char *title;
    const char *subtitle;
    const char *tags; ///< Comma separated, empty for none.
};

const SectionSpec kSections[] = {
    {MenuSection::Optimize, "rocket", QT_TRANSLATE_NOOP("MainMenuPanel", "Optimise"),
     QT_TRANSLATE_NOOP("MainMenuPanel", "Analyse and optimise your system for peak performance."),
     QT_TRANSLATE_NOOP("MainMenuPanel", "Clean,Optimise,Boost")},
    {MenuSection::Enhance, "sparkles", QT_TRANSLATE_NOOP("MainMenuPanel", "Enhance"),
     QT_TRANSLATE_NOOP("MainMenuPanel", "Improve system responsiveness and efficiency."),
     QT_TRANSLATE_NOOP("MainMenuPanel", "Tweak,Customise,Fine Tune")},
    {MenuSection::Perform, "chart", QT_TRANSLATE_NOOP("MainMenuPanel", "Perform"),
     QT_TRANSLATE_NOOP("MainMenuPanel", "Boost performance and monitor key system metrics."),
     QT_TRANSLATE_NOOP("MainMenuPanel", "FPS,Monitor,Analyse")},
    {MenuSection::SystemInfo, "monitor",
     QT_TRANSLATE_NOOP("MainMenuPanel", "System Info"),
     QT_TRANSLATE_NOOP("MainMenuPanel",
                       "View detailed information about your hardware and software."),
     QT_TRANSLATE_NOOP("MainMenuPanel", "Hardware,Software,Details")},
    {MenuSection::Settings, "settings", QT_TRANSLATE_NOOP("MainMenuPanel", "Settings"),
     QT_TRANSLATE_NOOP("MainMenuPanel", "Configure preferences and customise your experience."),
     ""},
};

} // namespace

MainMenuPanel::MainMenuPanel(QWidget *parent)
    : QWidget(parent)
{
    m_userName = tr("User");
    buildUi();
}

void MainMenuPanel::buildUi()
{
    const Theme::Palette &c = Theme::colors();
    const Theme::Metrics &m = Theme::metrics();

    // --- Header: eyebrow, two-tone title, tagline ---------------------------
    m_eyebrow = new QLabel(tr("WELCOME BACK"), this);
    m_eyebrow->setFont(Theme::font(13, QFont::DemiBold, 2.6));
    m_eyebrow->setStyleSheet(QStringLiteral("color: %1;").arg(c.link.name()));

    m_heading = new QLabel(this);
    m_heading->setTextFormat(Qt::RichText);
    m_heading->setFont(Theme::font(42, QFont::Bold, -0.6));

    m_tagline = new QLabel(tr("Optimise. Enhance. Perform."), this);
    m_tagline->setFont(Theme::font(17));
    m_tagline->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    // The padlock uses the icon set rather than an emoji.
    m_lockNote = new QWidget(this);
    auto *lockRow = new QHBoxLayout(m_lockNote);
    lockRow->setContentsMargins(0, 0, 0, 0);
    lockRow->setSpacing(7);

    auto *lockIcon = new QLabel(m_lockNote);
    lockIcon->setPixmap(IconProvider::pixmap(QStringLiteral("lock"), 15, c.danger,
                                             devicePixelRatioF()));
    auto *lockText = new QLabel(tr("Features locked - activate a licence to unlock them"),
                                m_lockNote);
    lockText->setFont(Theme::font(13, QFont::Medium));
    lockText->setStyleSheet(QStringLiteral("color: %1;").arg(c.danger.name()));

    lockRow->addWidget(lockIcon, 0, Qt::AlignVCenter);
    lockRow->addWidget(lockText, 0, Qt::AlignVCenter);
    lockRow->addStretch(1);
    m_lockNote->hide();

    auto *headingColumn = new QVBoxLayout;
    headingColumn->setContentsMargins(0, 0, 0, 0);
    headingColumn->setSpacing(0);
    headingColumn->addWidget(m_eyebrow);
    headingColumn->addSpacing(6);
    headingColumn->addWidget(m_heading);
    headingColumn->addSpacing(4);
    headingColumn->addWidget(m_tagline);
    headingColumn->addSpacing(10);
    headingColumn->addWidget(m_lockNote);
    headingColumn->addStretch(1);

    // --- Header right: account card -----------------------------------------
    m_userCard = new UserCard(this);
    connect(m_userCard, &UserCard::clicked, this, [this] {
        // The avatar opens a small popover; signing out happens from there.
        if (!m_accountMenu) {
            m_accountMenu = new AccountMenu(this);
            connect(m_accountMenu, &AccountMenu::signOutClicked, this,
                    &MainMenuPanel::accountRequested);
        }
        m_accountMenu->popupUnder(m_userCard);
    });

    auto *accountColumn = new QVBoxLayout;
    accountColumn->setContentsMargins(0, 0, 0, 0);
    accountColumn->setSpacing(10);
    accountColumn->addWidget(m_userCard, 0, Qt::AlignRight);
    accountColumn->addStretch(1);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(28);
    headerRow->addLayout(headingColumn, 1);
    headerRow->addLayout(accountColumn, 0);

    // --- Section grid --------------------------------------------------------
    m_cardGroup = new QButtonGroup(this);
    m_cardGroup->setExclusive(true);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(m.cardSpacing + 4);
    grid->setVerticalSpacing(m.cardSpacing + 4);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    int index = 0;
    for (const SectionSpec &spec : kSections) {
        auto *card = new MenuCard(spec.section, QString::fromLatin1(spec.iconName),
                                  tr(spec.title), tr(spec.subtitle), this);
        const QString tagList = tr(spec.tags);
        if (!tagList.isEmpty())
            card->setTags(tagList.split(QLatin1Char(','), Qt::SkipEmptyParts));

        m_cards.append(card);
        m_cardGroup->addButton(card);

        // Cards with chips sit two per row; the chip-less one spans the width.
        if (tagList.isEmpty())
            grid->addWidget(card, index / 2 + (index % 2), 0, 1, 2);
        else
            grid->addWidget(card, index / 2, index % 2);
        ++index;

        connect(card, &MenuCard::clicked, this, [this, card] {
            if (m_locked) {
                card->setChecked(false);
                emit activateRequested();
                return;
            }
            m_currentSection = card->section();
            emit sectionActivated(card->section());
        });
    }

    // --- Page ----------------------------------------------------------------
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(m.panelPadding, m.panelPadding + m.titleBarHeight - 18,
                              m.panelPadding, m.panelPadding - 10);
    outer->setSpacing(0);
    outer->addLayout(headerRow);
    outer->addSpacing(26);
    outer->addLayout(grid);
    outer->addStretch(1);

    // --- Keyboard navigation -------------------------------------------------
    for (int i = 1; i < m_cards.size(); ++i)
        setTabOrder(m_cards.at(i - 1), m_cards.at(i));

    setUserName(m_userName);
    // No card is highlighted by default - selection only appears on hover, on
    // click, or while its section page is open.
    clearSelection();
}

void MainMenuPanel::clearSelection()
{
    m_cardGroup->setExclusive(false);
    for (MenuCard *card : m_cards)
        card->setChecked(false);
    m_cardGroup->setExclusive(true);
}

void MainMenuPanel::setUserName(const QString &name)
{
    const Theme::Palette &c = Theme::colors();

    m_userName = name.trimmed().isEmpty() ? tr("User") : name.trimmed();

    // "Nextgen" in white, "Tweaks" in the brand accent.
    m_heading->setText(QStringLiteral("<span style=\"color:%1;\">%2</span> "
                                      "<span style=\"color:%3;\">%4</span>")
                           .arg(c.textPrimary.name(), tr("Nextgen"), c.primaryBright.name(),
                                tr("Tweaks")));
    if (m_userCard)
        m_userCard->setUserName(m_userName);
}

MenuCard *MainMenuPanel::cardFor(MenuSection section) const
{
    for (MenuCard *card : m_cards) {
        if (card->section() == section)
            return card;
    }
    return nullptr;
}

void MainMenuPanel::setLocked(bool locked)
{
    m_locked = locked;
    m_lockNote->setVisible(locked);
    for (MenuCard *card : m_cards)
        card->setLocked(locked);
}

void MainMenuPanel::setCurrentSection(MenuSection section)
{
    m_currentSection = section;
    if (MenuCard *card = cardFor(section))
        card->setChecked(true);
}

void MainMenuPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    const QRectF body(rect());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient base(body.topLeft(), body.bottomRight());
    base.setColorAt(0.0, c.formPanelTop);
    base.setColorAt(1.0, c.formPanelBottom);
    painter.setPen(Qt::NoPen);
    painter.setBrush(base);
    painter.drawRect(body);

    // A faint blue bloom in the upper right keeps the panel from looking flat.
    QRadialGradient bloom(QPointF(body.width() * 0.92, body.height() * 0.06),
                          qMax(body.width(), body.height()) * 0.8);
    bloom.setColorAt(0.0, Theme::alpha(c.primary, 26));
    bloom.setColorAt(1.0, Qt::transparent);
    painter.setBrush(bloom);
    painter.drawRect(body);
}
