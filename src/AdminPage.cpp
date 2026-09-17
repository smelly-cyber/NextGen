#include "AdminPage.h"

#include "AdminClient.h"
#include "FramelessDialog.h"
#include "IconButton.h"
#include "IconProvider.h"
#include "MessageDialog.h"
#include "ModernButton.h"
#include "ModernCheckBox.h"
#include "ModernComboBox.h"
#include "ModernLineEdit.h"
#include "SectionCard.h"
#include "Theme.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

// ============================================================================
//  Sizing. The workspace window is a fixed size, so this page has a hard
//  height budget: anything taller and the layout's size hint pushes the window
//  bigger than it is allowed to be. Every number here is chosen to fit.
// ============================================================================
namespace {
constexpr int kHeaderHeight = 50;
constexpr int kStatCardHeight = 82;
constexpr int kActionRowHeight = 48;
constexpr int kActionRowGap = 6;
constexpr int kTableRowHeight = 29;
/// The tick box needs its indicator inset (4) plus the box itself (22). At the
/// 20px it used to get, the indicator was drawn clipped.
constexpr int kCheckColumnWidth = 26;
constexpr int kGap = 12;

QColor statusColour(const QString &status)
{
    const Theme::Palette &c = Theme::colors();
    if (status == QLatin1String("Active"))
        return c.success;
    if (status == QLatin1String("Expired"))
        return QColor(0xFF, 0xB0, 0x3A);
    return c.danger;
}

/// Small coloured status pill used in the licence table.
class StatusPill : public QWidget
{
public:
    StatusPill(const QString &text, const QColor &colour, QWidget *parent = nullptr)
        : QWidget(parent), m_text(text), m_colour(colour)
    {
        setFixedHeight(20);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    QSize sizeHint() const override
    {
        const QFontMetrics fm(Theme::font(10, QFont::DemiBold));
        return QSize(fm.horizontalAdvance(m_text) + 18, 20);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const QRectF body(0.5, 0.5, width() - 1.0, height() - 1.0);
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::alpha(m_colour, 38));
        p.drawRoundedRect(body, height() / 2.0, height() / 2.0);
        p.setPen(QPen(Theme::alpha(m_colour, 110), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(body, height() / 2.0, height() / 2.0);
        p.setFont(Theme::font(10, QFont::DemiBold));
        p.setPen(m_colour);
        p.drawText(rect(), Qt::AlignCenter, m_text);
    }

private:
    QString m_text;
    QColor m_colour;
};

/// Column stretch weights shared by the licence table's header and its rows, so
/// the two can never drift apart.
constexpr int kColNum = 7;
constexpr int kColUser = 22;
constexpr int kColKey = 26;
constexpr int kColHwid = 14;
constexpr int kColStatus = 15;
constexpr int kColActions = 16;

// ------------------------------------------------------- AddLicenseDialog ---

/// "Add License": pick the kind of licence to mint, then show the key.
class AddLicenseDialog : public FramelessDialog
{
public:
    explicit AddLicenseDialog(QWidget *parent = nullptr)
        : FramelessDialog(parent)
    {
        const Theme::Palette &c = Theme::colors();
        setHeaderTitle(QObject::tr("Add License"));
        setMinimumWidth(420);

        auto *heading = new QLabel(QObject::tr("Mint a new licence key"), this);
        heading->setFont(Theme::font(18, QFont::Bold));
        heading->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

        auto *kindLabel = new QLabel(QObject::tr("Licence type"), this);
        kindLabel->setFont(Theme::font(12, QFont::Medium));
        kindLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

        m_kind = new ModernComboBox(this);
        m_kind->addItem(QObject::tr("Lifetime"));
        m_kind->addItem(QObject::tr("Duration (days)"));
        m_kind->addItem(QObject::tr("Use limited"));

        m_amount = new ModernLineEdit(this);
        m_amount->setPlaceholderText(QObject::tr("30"));
        m_amount->hide();
        QObject::connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                         [this](int index) {
                             m_amount->setVisible(index != 0);
                             m_amount->setPlaceholderText(index == 1 ? QObject::tr("30 (days)")
                                                                     : QObject::tr("25 (uses)"));
                         });

        m_note = new ModernLineEdit(this);
        m_note->setPlaceholderText(QObject::tr("Note (optional)"));

        auto *cancel = new ModernButton(QObject::tr("Cancel"), ModernButton::Variant::Secondary,
                                        this);
        auto *create = new ModernButton(QObject::tr("Create"), ModernButton::Variant::Primary,
                                        this);
        for (ModernButton *b : {cancel, create}) {
            b->setFont(Theme::font(13, QFont::Medium));
            b->setFixedHeight(42);
            b->setMinimumWidth(110);
        }
        QObject::connect(cancel, &ModernButton::clicked, this, &QDialog::reject);
        QObject::connect(create, &ModernButton::clicked, this, &QDialog::accept);

        auto *buttons = new QHBoxLayout;
        buttons->setContentsMargins(0, 0, 0, 0);
        buttons->setSpacing(10);
        buttons->addStretch(1);
        buttons->addWidget(cancel);
        buttons->addWidget(create);

        auto *l = contentLayout();
        l->addWidget(heading);
        l->addSpacing(14);
        l->addWidget(kindLabel);
        l->addSpacing(4);
        l->addWidget(m_kind);
        l->addSpacing(8);
        l->addWidget(m_amount);
        l->addSpacing(8);
        l->addWidget(m_note);
        l->addSpacing(18);
        l->addLayout(buttons);
    }

    QString kind() const
    {
        switch (m_kind->currentIndex()) {
        case 1: return QStringLiteral("duration");
        case 2: return QStringLiteral("uses");
        default: return QStringLiteral("lifetime");
        }
    }
    int amount() const { return m_amount->text().trimmed().toInt(); }
    QString note() const { return m_note->text().trimmed(); }

private:
    ModernComboBox *m_kind = nullptr;
    ModernLineEdit *m_amount = nullptr;
    ModernLineEdit *m_note = nullptr;
};

} // namespace

// ----------------------------------------------------------- AdminActionRow --

/// Clickable row: icon tile, title, subtitle, chevron. Used by Remote
/// Management and Security & Tools.
class AdminActionRow : public QAbstractButton
{
public:
    enum class Tone { Neutral, Accent, Danger, Success };

    AdminActionRow(const QString &icon, const QString &title, const QString &subtitle, Tone tone,
                   QWidget *parent = nullptr)
        : QAbstractButton(parent), m_icon(icon), m_title(title), m_subtitle(subtitle), m_tone(tone)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(kActionRowHeight);
        setToolTip(subtitle);
    }

    /// Re-labels the row in place (used to flip Nuke <-> Restore).
    void setContent(const QString &icon, const QString &title, const QString &subtitle, Tone tone)
    {
        m_icon = icon;
        m_title = title;
        m_subtitle = subtitle;
        m_tone = tone;
        setToolTip(subtitle);
        update();
    }

protected:
    void enterEvent(QEnterEvent *e) override
    {
        m_hover = true;
        update();
        QAbstractButton::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override
    {
        m_hover = false;
        update();
        QAbstractButton::leaveEvent(e);
    }
    void paintEvent(QPaintEvent *) override
    {
        const Theme::Palette &c = Theme::colors();
        const bool danger = m_tone == Tone::Danger;
        const bool success = m_tone == Tone::Success;
        // The colour that carries the row: red for danger, green when a nuked
        // client can be restored, accent blue for a normal action, else muted.
        const QColor accent = danger ? c.danger
                              : success ? c.success
                              : m_tone == Tone::Accent ? c.primaryBright
                                                       : c.textSecondary;
        const bool tinted = danger || success;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        p.setPen(Qt::NoPen);
        p.setBrush(tinted ? Theme::alpha(accent, m_hover ? 46 : 28)
                          : Theme::alpha(c.cardHoverTop, m_hover ? 230 : 140));
        p.drawRoundedRect(body, 10, 10);
        p.setPen(QPen(Theme::alpha(tinted ? accent : c.cardBorder, m_hover ? 170 : 90), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(body, 10, 10);

        const QRectF tile(9, (height() - 30) / 2.0, 30, 30);
        const QColor tileColour = tinted ? accent : c.primaryBright;
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::alpha(tileColour, 38));
        p.drawRoundedRect(tile, 8, 8);
        const QPixmap glyph = IconProvider::pixmap(m_icon, 16, tileColour, devicePixelRatioF());
        if (!glyph.isNull())
            p.drawPixmap(QPointF(tile.center().x() - 8, tile.center().y() - 8), glyph);

        const qreal left = 47;
        const qreal textWidth = width() - left - 26;

        p.setFont(Theme::font(12, QFont::DemiBold));
        p.setPen(tinted ? accent : c.textPrimary);
        p.drawText(QRectF(left, 7, textWidth, 16), Qt::AlignLeft | Qt::AlignVCenter, m_title);

        // Subtitles are elided rather than clipped mid-word.
        p.setFont(Theme::font(10));
        p.setPen(c.textMuted);
        const QFontMetrics fm(Theme::font(10));
        p.drawText(QRectF(left, 25, textWidth, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(m_subtitle, Qt::ElideRight, int(textWidth)));

        const QPixmap chevron = IconProvider::pixmap(QStringLiteral("chevron_right"), 14,
                                                     c.textMuted, devicePixelRatioF());
        if (!chevron.isNull())
            p.drawPixmap(QPointF(width() - 22, height() / 2.0 - 7), chevron);
    }

private:
    QString m_icon, m_title, m_subtitle;
    Tone m_tone;
    bool m_hover = false;
};

// ---------------------------------------------------------------- StatCard --

/// Headline figure tile. Painted in one go rather than assembled from nested
/// layouts, which is what previously let the value and caption overlap.
class StatCard : public QWidget
{
public:
    StatCard(const QString &icon, const QString &caption, const QString &sub,
             const QColor &accent, QWidget *parent = nullptr)
        : QWidget(parent), m_icon(icon), m_caption(caption), m_sub(sub), m_accent(accent)
    {
        setFixedHeight(kStatCardHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setValue(const QString &value)
    {
        m_value = value;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme::Palette &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QLinearGradient fill(body.topLeft(), body.bottomRight());
        fill.setColorAt(0.0, c.cardTop);
        fill.setColorAt(1.0, c.cardBottom);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(body, 14, 14);
        p.setPen(QPen(c.cardBorder, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(body, 14, 14);

        // Icon tile.
        const QRectF tile(14, (height() - 40) / 2.0, 40, 40);
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::alpha(m_accent, 38));
        p.drawRoundedRect(tile, 11, 11);
        const QPixmap glyph = IconProvider::pixmap(m_icon, 21, m_accent, devicePixelRatioF());
        if (!glyph.isNull())
            p.drawPixmap(QPointF(tile.center().x() - 10.5, tile.center().y() - 10.5), glyph);

        const qreal left = 66;
        const qreal textWidth = width() - left - 12;

        p.setFont(Theme::font(11, QFont::Medium));
        p.setPen(c.textSecondary);
        p.drawText(QRectF(left, 12, textWidth, 14), Qt::AlignLeft | Qt::AlignVCenter, m_caption);

        p.setFont(Theme::font(23, QFont::Bold));
        p.setPen(c.textPrimary);
        p.drawText(QRectF(left, 28, textWidth, 28), Qt::AlignLeft | Qt::AlignVCenter, m_value);

        p.setFont(Theme::font(10));
        p.setPen(c.success);
        p.drawText(QRectF(left, 56, textWidth, 14), Qt::AlignLeft | Qt::AlignVCenter, m_sub);
    }

private:
    QString m_icon, m_caption, m_sub, m_value = QStringLiteral("--");
    QColor m_accent;
};

// ============================================================================
//  LicenseTableRow - one licence in the table (global scope: the header
//  forward-declares it).
// ============================================================================
class LicenseTableRow : public QWidget
{
public:
    LicenseTableRow(int index, const QJsonObject &data, AdminPage *page, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_keyId(data.value(QStringLiteral("keyId")).toString())
        , m_data(data)
    {
        const Theme::Palette &c = Theme::colors();
        setFixedHeight(kTableRowHeight);

        m_check = new ModernCheckBox(QString(), this);
        m_check->setFixedWidth(kCheckColumnWidth);

        const auto label = [this, &c](const QString &text, int pt, const QColor &colour) {
            auto *l = new QLabel(text, this);
            l->setFont(Theme::font(pt));
            l->setStyleSheet(QStringLiteral("color: %1;").arg(colour.name()));
            return l;
        };

        auto *num = label(QStringLiteral("%1").arg(index, 3, 10, QLatin1Char('0')), 11, c.textMuted);
        auto *user = label(data.value(QStringLiteral("user")).toString(), 11, c.textPrimary);
        auto *key = label(data.value(QStringLiteral("key")).toString(), 11, c.textSecondary);
        const bool bound = data.value(QStringLiteral("bound")).toBool();
        auto *hwid = label(data.value(QStringLiteral("hwid")).toString(), 11,
                           bound ? c.textSecondary : c.textMuted);
        if (bound)
            hwid->setFont(Theme::font(11, QFont::Medium, 0.5)); // Tracked, monospace-ish.

        const QString status = data.value(QStringLiteral("status")).toString();
        auto *pill = new StatusPill(status, statusColour(status), this);

        auto *view = new IconButton(QStringLiteral("eye"), this);
        view->setFixedSize(22, 22);
        view->setIconSize(14);
        view->setColors(c.textSecondary, c.primaryBright);
        view->setToolTip(QObject::tr("View details"));

        auto *revoke = new IconButton(QStringLiteral("ban"), this);
        revoke->setFixedSize(22, 22);
        revoke->setIconSize(14);
        revoke->setColors(c.textSecondary, c.danger);
        revoke->setToolTip(QObject::tr("Revoke this licence"));

        QObject::connect(view, &IconButton::clicked, page, [this, page] { page->showRow(m_data); });
        QObject::connect(revoke, &IconButton::clicked, page,
                         [this, page] { page->revokeRow(m_keyId); });

        auto *actions = new QWidget(this);
        auto *al = new QHBoxLayout(actions);
        al->setContentsMargins(0, 0, 0, 0);
        al->setSpacing(4);
        al->addWidget(view);
        al->addWidget(revoke);
        al->addStretch(1);

        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        row->addWidget(m_check, 0);
        row->addWidget(num, kColNum);
        row->addWidget(user, kColUser);
        row->addWidget(key, kColKey);
        row->addWidget(hwid, kColHwid);
        row->addWidget(pill, kColStatus);
        row->addWidget(actions, kColActions);
    }

    QString keyId() const { return m_keyId; }
    bool isChecked() const { return m_check->isChecked(); }
    ModernCheckBox *checkBox() const { return m_check; }

private:
    QString m_keyId;
    QJsonObject m_data;
    ModernCheckBox *m_check = nullptr;
};

// ============================================================================
//  AdminPage
// ============================================================================

AdminPage::AdminPage(QWidget *parent)
    : QWidget(parent)
    , m_admin(new AdminClient(this))
{
    const Theme::Metrics &m = Theme::metrics();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(m.sectionPadding, m.titleBarHeight - 10, m.sectionPadding, 16);
    outer->setSpacing(kGap);
    outer->addWidget(buildHeader());
    outer->addWidget(buildStatsRow());

    auto *middle = new QHBoxLayout;
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(kGap);
    middle->addWidget(buildRemoteManagement(), 26);
    middle->addWidget(buildPushUpdates(), 28);
    middle->addWidget(buildLicenseManagement(), 46);
    outer->addLayout(middle, 1);

    auto *bottom = new QHBoxLayout;
    bottom->setContentsMargins(0, 0, 0, 0);
    bottom->setSpacing(kGap);
    bottom->addWidget(buildSystemStatus(), 30);
    bottom->addWidget(buildUserActivity(), 38);
    bottom->addWidget(buildSecurityTools(), 32);
    outer->addLayout(bottom, 1);

    m_clock = new QTimer(this);
    m_clock->setInterval(1000);
    connect(m_clock, &QTimer::timeout, this, [this] {
        m_dateTime->setText(QDateTime::currentDateTime().toString(
            QStringLiteral("ddd, dd MMM yyyy    hh:mm AP")));
    });

    // Typing in the search box reloads shortly after the user stops.
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(320);
    connect(m_searchDebounce, &QTimer::timeout, this, [this] {
        m_page = 1;
        loadLicenses();
    });

    // While the console is on screen it refreshes itself, so the activity feed,
    // online count and service status stay current without a manual reload.
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(15000);
    connect(m_refreshTimer, &QTimer::timeout, this, [this] {
        loadOverview();
        loadLicenses();
    });
}

void AdminPage::paintEvent(QPaintEvent *)
{
    const Theme::Palette &c = Theme::colors();
    QPainter p(this);
    QLinearGradient base(rect().topLeft(), rect().bottomRight());
    base.setColorAt(0.0, c.formPanelTop);
    base.setColorAt(1.0, c.formPanelBottom);
    p.fillRect(rect(), base);
}

void AdminPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_clock->start();
    m_refreshTimer->start();
    m_dateTime->setText(
        QDateTime::currentDateTime().toString(QStringLiteral("ddd, dd MMM yyyy    hh:mm AP")));
    refresh();
    emit statusChanged(tr("Owner console"), Theme::colors().primaryBright);
}

void AdminPage::hideEvent(QHideEvent *event)
{
    m_clock->stop();
    m_refreshTimer->stop();
    QWidget::hideEvent(event);
}

void AdminPage::refresh()
{
    loadOverview();
    loadLicenses();
}

// --- Header -----------------------------------------------------------------
QWidget *AdminPage::buildHeader()
{
    const Theme::Palette &c = Theme::colors();
    auto *header = new QWidget;
    header->setFixedHeight(kHeaderHeight);

    auto *title = new QLabel(header);
    title->setText(QStringLiteral("<span style='color:%1;'>Admin </span>"
                                  "<span style='color:%2;'>Panel</span>")
                       .arg(c.textPrimary.name(), c.primaryBright.name()));
    title->setFont(Theme::font(26, QFont::Bold));

    auto *subtitle = new QLabel(tr("Manage, monitor and secure Nextgen Tweaks."), header);
    subtitle->setFont(Theme::font(12));
    subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    auto *titleCol = new QVBoxLayout;
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(0);
    titleCol->addWidget(title);
    titleCol->addWidget(subtitle);

    // "Secure Access / Owner Only" pill.
    auto *badge = new QWidget(header);
    badge->setFixedHeight(42);
    badge->setStyleSheet(
        QStringLiteral("background: %1; border: 1px solid %2; border-radius: 11px;")
            .arg(Theme::alpha(c.success, 26).name(QColor::HexArgb),
                 Theme::alpha(c.success, 90).name(QColor::HexArgb)));
    auto *lock = new QLabel(badge);
    lock->setPixmap(
        IconProvider::pixmap(QStringLiteral("lock"), 18, c.success, devicePixelRatioF()));
    auto *secure = new QLabel(tr("Secure Access"), badge);
    secure->setFont(Theme::font(12, QFont::DemiBold));
    secure->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none;")
                              .arg(c.success.name()));
    auto *ownerOnly = new QLabel(tr("Owner Only"), badge);
    ownerOnly->setFont(Theme::font(10));
    ownerOnly->setStyleSheet(QStringLiteral("color: %1; background: transparent; border: none;")
                                 .arg(c.textSecondary.name()));
    auto *badgeText = new QVBoxLayout;
    badgeText->setContentsMargins(0, 0, 0, 0);
    badgeText->setSpacing(0);
    badgeText->addWidget(secure);
    badgeText->addWidget(ownerOnly);
    auto *badgeRow = new QHBoxLayout(badge);
    badgeRow->setContentsMargins(12, 0, 14, 0);
    badgeRow->setSpacing(9);
    badgeRow->addWidget(lock, 0, Qt::AlignVCenter);
    badgeRow->addLayout(badgeText);

    m_dateTime = new QLabel(header);
    m_dateTime->setFont(Theme::font(11, QFont::Medium));
    m_dateTime->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));
    m_dateTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *row = new QHBoxLayout(header);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(14);
    row->addLayout(titleCol, 1);
    row->addWidget(badge, 0, Qt::AlignVCenter);
    row->addWidget(m_dateTime, 0, Qt::AlignVCenter);
    return header;
}

// --- Stats ------------------------------------------------------------------
QWidget *AdminPage::buildStatsRow()
{
    const Theme::Palette &c = Theme::colors();
    auto *row = new QWidget;
    row->setFixedHeight(kStatCardHeight);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kGap);

    m_totalUsers = new StatCard(QStringLiteral("nodes"), tr("Total Users"),
                                tr("registered accounts"), c.primaryBright, row);
    m_activeLicenses = new StatCard(QStringLiteral("key"), tr("Active Licenses"),
                                    tr("currently valid"), c.cyan, row);
    m_onlineNow = new StatCard(QStringLiteral("monitor"), tr("Online Now"),
                               tr("in the last 5 minutes"), c.success, row);
    m_appVersion = new StatCard(QStringLiteral("box"), tr("App Version"), tr("latest build"),
                                QColor(0xA8, 0x6C, 0xF0), row);
    for (StatCard *card : {m_totalUsers, m_activeLicenses, m_onlineNow, m_appVersion})
        layout->addWidget(card, 1);
    return row;
}

// --- Remote management ------------------------------------------------------
QWidget *AdminPage::buildRemoteManagement()
{
    auto *card = new SectionCard(tr("Remote Management"),
                                 tr("Take control of user clients if required."),
                                 QStringLiteral("monitor"));
    struct Spec
    {
        const char *icon, *title, *sub, *kind;
        AdminActionRow::Tone tone;
        bool destructive;
    };
    const Spec specs[] = {
        {"trash", "NUKE CLIENT", "Permanently wipes the application from the device.", "nuke",
         AdminActionRow::Tone::Danger, true},
        {"ban", "DISABLE CLIENT", "Disables the application (cannot be used).", "disable",
         AdminActionRow::Tone::Danger, true},
        {"refresh", "RESET CLIENT", "Resets app data, settings and licence.", "reset",
         AdminActionRow::Tone::Accent, true},
        {"chat", "SEND MESSAGE", "Send a message to a user's client.", "message",
         AdminActionRow::Tone::Neutral, false},
    };
    for (const Spec &s : specs) {
        auto *actionRow = new AdminActionRow(QString::fromLatin1(s.icon), tr(s.title), tr(s.sub),
                                             s.tone, card);
        const QString kind = QString::fromLatin1(s.kind);
        const QString title = tr(s.title);
        const bool destructive = s.destructive;

        if (kind == QLatin1String("nuke")) {
            // The nuke row is a toggle: after nuking it turns green and reads
            // "Restore Client"; pressing it again restores the clients.
            m_nukeRow = actionRow;
            connect(actionRow, &QAbstractButton::clicked, this, [this] {
                if (m_nukeActive) {
                    if (!MessageDialog::confirm(window(), tr("Restore Client"),
                                                tr("Restore every nuked client?"),
                                                tr("Nuked clients will be able to run again the "
                                                   "next time they are opened."),
                                                tr("Restore"), MessageDialog::Tone::Success,
                                                ModernButton::Variant::Primary))
                        return;
                    m_admin->command(QStringLiteral("restore"), QStringLiteral("all"), QString(),
                                     [this](const QJsonObject &r) {
                                         if (r.value(QStringLiteral("ok")).toBool()) {
                                             toast(tr("Clients restored."));
                                             applyNukeState(false);
                                         } else {
                                             toast(r.value(QStringLiteral("reason")).toString(),
                                                   false);
                                         }
                                     });
                } else {
                    if (!MessageDialog::confirm(
                            window(), tr("Nuke Client"), tr("Nuke every client?"),
                            tr("Every client will refuse to run - it will close instantly on "
                               "launch with nothing shown - until you restore it. Your owner "
                               "account is never affected."),
                            tr("Nuke"), MessageDialog::Tone::Danger,
                            ModernButton::Variant::Danger))
                        return;
                    m_admin->command(QStringLiteral("nuke"), QStringLiteral("all"), QString(),
                                     [this](const QJsonObject &r) {
                                         if (r.value(QStringLiteral("ok")).toBool()) {
                                             toast(tr("Clients nuked."));
                                             applyNukeState(true);
                                         } else {
                                             toast(r.value(QStringLiteral("reason")).toString(),
                                                   false);
                                         }
                                     });
                }
            });
            card->contentLayout()->addWidget(actionRow);
            card->contentLayout()->addSpacing(kActionRowGap);
            continue;
        }

        connect(actionRow, &QAbstractButton::clicked, this, [this, kind, title, destructive] {
            if (kind == QLatin1String("message")) {
                bool ok = false;
                const QString text = MessageDialog::getText(
                    window(), tr("Send Message"), tr("Message to send to every client:"),
                    tr("Type your message"), QString(), &ok);
                if (ok && !text.isEmpty())
                    sendCommand(kind, title, text, false);
                return;
            }
            sendCommand(kind, title, QString(), destructive);
        });
        card->contentLayout()->addWidget(actionRow);
        card->contentLayout()->addSpacing(kActionRowGap);
    }
    card->contentLayout()->addStretch(1);
    return card;
}

// --- Push updates -----------------------------------------------------------
QWidget *AdminPage::buildPushUpdates()
{
    const Theme::Palette &c = Theme::colors();
    auto *card = new SectionCard(tr("Push Updates"), tr("Deploy new updates to users."),
                                 QStringLiteral("cloud"));

    m_updateVersionLabel = new QLabel(tr("Version 1.0.0"), card);
    m_updateVersionLabel->setFont(Theme::font(13, QFont::DemiBold));
    m_updateVersionLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

    auto *editNotes = new QLabel(card);
    editNotes->setText(QStringLiteral("<a href='#' style='color:%1; text-decoration:none;'>%2</a>")
                           .arg(c.link.name(), tr("Edit Notes")));
    editNotes->setFont(Theme::font(11, QFont::Medium));
    editNotes->setCursor(Qt::PointingHandCursor);
    connect(editNotes, &QLabel::linkActivated, this, [this] { m_updateNotes->setFocus(); });

    auto *versionRow = new QHBoxLayout;
    versionRow->setContentsMargins(0, 0, 0, 0);
    versionRow->addWidget(m_updateVersionLabel, 1);
    versionRow->addWidget(editNotes, 0);

    m_updateNotes = new QPlainTextEdit(card);
    m_updateNotes->setPlaceholderText(tr("Bug fixes\nPerformance improvements\nStability updates"));
    m_updateNotes->setFixedHeight(58);
    m_updateNotes->setFont(Theme::font(11));
    m_updateNotes->setFrameShape(QFrame::NoFrame);
    m_updateNotes->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: %1; border: 1px solid %2; border-radius: 9px;"
                       " color: %3; padding: 7px 13px; }")
            .arg(c.fieldBackground.name(), c.fieldBorder.name(), c.textSecondary.name()));

    // ModernLineEdit insets its visible field by 5px to leave room for its focus
    // glow, so the notes box is inset to match - otherwise the two boxes in this
    // card would not share an edge.
    auto *notesRow = new QHBoxLayout;
    notesRow->setContentsMargins(5, 0, 5, 0);
    notesRow->addWidget(m_updateNotes);

    m_updateVersion = new ModernLineEdit(card);
    m_updateVersion->setCompact(true);
    m_updateVersion->setPlaceholderText(tr("New version, e.g. 1.0.1"));

    m_updateUrl = new ModernLineEdit(card);
    m_updateUrl->setCompact(true);
    m_updateUrl->setPlaceholderText(tr("Download URL (.exe) - optional"));

    auto *targetLabel = new QLabel(tr("Target Users"), card);
    targetLabel->setFont(Theme::font(11, QFont::Medium));
    targetLabel->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));

    m_updateTarget = new ModernComboBox(card);
    m_updateTarget->addItem(tr("All Users"));

    auto *push = new ModernButton(tr("Push Update"), ModernButton::Variant::Primary, card);
    push->setIconName(QStringLiteral("cloud"));
    push->setFont(Theme::font(13, QFont::DemiBold));
    push->setFixedHeight(42);
    connect(push, &ModernButton::clicked, this, [this] {
        const QString version = m_updateVersion->text().trimmed();
        if (version.isEmpty()) {
            toast(tr("Enter a version number first."), false);
            return;
        }
        m_admin->pushUpdate(version, m_updateNotes->toPlainText().trimmed(),
                            QStringLiteral("all"), m_updateUrl->text().trimmed(), QString(),
                            [this, version](const QJsonObject &r) {
                                if (r.value(QStringLiteral("ok")).toBool()) {
                                    toast(tr("Update %1 pushed to all users.").arg(version));
                                    loadOverview();
                                } else {
                                    toast(r.value(QStringLiteral("reason")).toString(), false);
                                }
                            });
    });

    auto *l = card->contentLayout();
    l->addLayout(versionRow);
    l->addSpacing(6);
    l->addLayout(notesRow);
    l->addSpacing(8);
    l->addWidget(m_updateVersion);
    l->addSpacing(6);
    l->addWidget(m_updateUrl);
    l->addSpacing(8);
    l->addWidget(targetLabel);
    l->addSpacing(3);
    l->addWidget(m_updateTarget);
    l->addStretch(1);
    l->addWidget(push);
    return card;
}

// --- Licence management -----------------------------------------------------
QWidget *AdminPage::buildLicenseManagement()
{
    const Theme::Palette &c = Theme::colors();
    auto *card = new SectionCard(tr("License Management"), tr("View and manage all licences."),
                                 QStringLiteral("key"));

    // --- Controls: one row, all the same height so nothing looks bolted on ---
    constexpr int kControlHeight = 38;

    m_licenseSearch = new ModernLineEdit(card);
    m_licenseSearch->setLeadingIcon(QStringLiteral("search"));
    m_licenseSearch->setPlaceholderText(tr("Search user or key"));
    m_licenseSearch->setCompact(true);
    m_licenseSearch->setFixedHeight(kControlHeight);
    connect(m_licenseSearch, &ModernLineEdit::textChanged, this,
            [this] { m_searchDebounce->start(); });
    connect(m_licenseSearch, &ModernLineEdit::returnPressed, this, [this] {
        m_page = 1;
        loadLicenses();
    });

    m_statusFilter = new ModernComboBox(card);
    m_statusFilter->addItem(tr("All Statuses"));
    m_statusFilter->addItem(tr("Active"));
    m_statusFilter->addItem(tr("Expired"));
    m_statusFilter->addItem(tr("Revoked"));
    m_statusFilter->setFixedSize(126, kControlHeight);
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        m_page = 1;
        loadLicenses();
    });

    auto *add = new ModernButton(tr("Add License"), ModernButton::Variant::Primary, card);
    add->setIconName(QStringLiteral("plus"));
    add->setFont(Theme::font(12, QFont::Medium));
    add->setFixedSize(128, kControlHeight);
    add->setCornerRadius(9);
    connect(add, &ModernButton::clicked, this, &AdminPage::addLicense);

    auto *controls = new QHBoxLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(9);
    controls->addWidget(m_licenseSearch, 1);
    controls->addWidget(m_statusFilter, 0);
    controls->addWidget(add, 0);

    // --- Table header: same stretch weights as the rows, plus select-all -----
    auto *headerRow = new QWidget(card);
    headerRow->setFixedHeight(22);
    auto *hr = new QHBoxLayout(headerRow);
    hr->setContentsMargins(0, 0, 0, 0);
    hr->setSpacing(6);

    m_selectAll = new ModernCheckBox(QString(), headerRow);
    m_selectAll->setFixedWidth(kCheckColumnWidth);
    m_selectAll->setToolTip(tr("Select every licence on this page"));
    connect(m_selectAll, &QCheckBox::toggled, this, [this](bool on) {
        for (LicenseTableRow *row : m_rows)
            row->checkBox()->setChecked(on);
    });
    hr->addWidget(m_selectAll, 0);

    const auto headerLabel = [&](const QString &text, int stretch) {
        auto *l = new QLabel(text, headerRow);
        l->setFont(Theme::font(10, QFont::DemiBold, 0.4));
        l->setStyleSheet(QStringLiteral("color: %1;").arg(c.textMuted.name()));
        hr->addWidget(l, stretch);
    };
    headerLabel(tr("#"), kColNum);
    headerLabel(tr("USER"), kColUser);
    headerLabel(tr("LICENSE KEY"), kColKey);
    headerLabel(tr("HWID"), kColHwid);
    headerLabel(tr("STATUS"), kColStatus);
    headerLabel(tr("ACTIONS"), kColActions);

    const auto divider = [&](QWidget *owner) {
        auto *line = new QFrame(owner);
        line->setFixedHeight(1);
        line->setStyleSheet(
            QStringLiteral("background: %1; border: none;").arg(c.cardBorder.name()));
        return line;
    };

    m_licenseRows = new QVBoxLayout;
    m_licenseRows->setContentsMargins(0, 0, 0, 0);
    m_licenseRows->setSpacing(1);

    // --- Footer -------------------------------------------------------------
    m_licenseCount = new QLabel(card);
    m_licenseCount->setFont(Theme::font(10));
    m_licenseCount->setStyleSheet(QStringLiteral("color: %1;").arg(c.textMuted.name()));

    m_pager = new QWidget(card);
    auto *pagerLayout = new QHBoxLayout(m_pager);
    pagerLayout->setContentsMargins(0, 0, 0, 0);
    pagerLayout->setSpacing(5);

    m_revokeSelected = new ModernButton(tr("Revoke Selected"), ModernButton::Variant::Danger,
                                        card);
    m_revokeSelected->setIconName(QStringLiteral("ban"));
    m_revokeSelected->setFont(Theme::font(11, QFont::Medium));
    m_revokeSelected->setFixedSize(142, 30);
    m_revokeSelected->setCornerRadius(9);
    m_revokeSelected->setEnabled(false); // Until something is ticked.
    connect(m_revokeSelected, &ModernButton::clicked, this, [this] {
        QStringList ids;
        for (LicenseTableRow *row : m_rows)
            if (row->isChecked())
                ids << row->keyId();
        if (ids.isEmpty())
            return;
        if (!MessageDialog::confirm(window(), tr("Revoke Selected"),
                                    tr("Revoke %1 licence(s)?").arg(ids.size()),
                                    tr("The users holding them lose access immediately."),
                                    tr("Revoke"), MessageDialog::Tone::Danger,
                                    ModernButton::Variant::Danger))
            return;
        m_admin->revoke(ids, [this, ids](const QJsonObject &r) {
            if (r.value(QStringLiteral("ok")).toBool()) {
                toast(tr("Revoked %1 licence(s).").arg(ids.size()));
                loadLicenses();
                loadOverview();
            } else {
                toast(r.value(QStringLiteral("reason")).toString(), false);
            }
        });
    });

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(10);
    footer->addWidget(m_licenseCount, 0);
    footer->addStretch(1);
    footer->addWidget(m_pager, 0);
    footer->addWidget(m_revokeSelected, 0);

    auto *l = card->contentLayout();
    l->addLayout(controls);
    l->addSpacing(12);
    l->addWidget(headerRow);
    l->addWidget(divider(card));
    l->addSpacing(5);
    l->addLayout(m_licenseRows);
    l->addStretch(1);
    l->addWidget(divider(card));
    l->addSpacing(8);
    l->addLayout(footer);
    return card;
}

void AdminPage::updateSelectionState()
{
    int checked = 0;
    for (LicenseTableRow *row : m_rows)
        if (row->isChecked())
            ++checked;
    m_revokeSelected->setEnabled(checked > 0);
}

// --- System status ----------------------------------------------------------
QWidget *AdminPage::buildSystemStatus()
{
    const Theme::Palette &c = Theme::colors();
    auto *card = new SectionCard(tr("System Status"), tr("Live status of backend services."),
                                 QStringLiteral("shield_check"));

    m_allOperational = new QLabel(card);
    m_allOperational->setFont(Theme::font(10, QFont::Medium));
    m_allOperational->setStyleSheet(QStringLiteral("color: %1;").arg(c.success.name()));
    m_allOperational->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *badgeRow = new QHBoxLayout;
    badgeRow->setContentsMargins(0, 0, 0, 0);
    badgeRow->addStretch(1);
    badgeRow->addWidget(m_allOperational);

    m_serviceRows = new QVBoxLayout;
    m_serviceRows->setContentsMargins(0, 0, 0, 0);
    m_serviceRows->setSpacing(8);

    card->contentLayout()->addLayout(badgeRow);
    card->contentLayout()->addSpacing(4);
    card->contentLayout()->addLayout(m_serviceRows);
    card->contentLayout()->addStretch(1);
    return card;
}

// --- User activity ----------------------------------------------------------
QWidget *AdminPage::buildUserActivity()
{
    auto *card = new SectionCard(tr("User Activity"), tr("Recent activity across all users."),
                                 QStringLiteral("pulse"));

    auto *viewAll = new ModernButton(tr("View All"), ModernButton::Variant::Secondary, card);
    viewAll->setFont(Theme::font(11, QFont::Medium));
    viewAll->setFixedSize(76, 26);
    viewAll->setCornerRadius(8);
    connect(viewAll, &ModernButton::clicked, this, [this] {
        m_admin->request(QStringLiteral("/v1/admin/overview"), {}, [this](const QJsonObject &r) {
            QString body;
            for (const QJsonValue &v : r.value(QStringLiteral("activity")).toArray()) {
                const QJsonObject a = v.toObject();
                const qint64 unix = qint64(a.value(QStringLiteral("unix")).toDouble());
                body += QStringLiteral("%1 — %2 (%3)<br>")
                            .arg(a.value(QStringLiteral("username")).toString(),
                                 a.value(QStringLiteral("detail")).toString(),
                                 QDateTime::fromSecsSinceEpoch(unix).toString(
                                     QStringLiteral("dd MMM hh:mm AP")));
            }
            if (body.isEmpty())
                body = tr("No activity recorded yet.");
            MessageDialog::information(window(), tr("User Activity"), tr("Recent activity"), body,
                                       MessageDialog::Tone::Info);
        });
    });

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->addStretch(1);
    headerRow->addWidget(viewAll);

    m_activityRows = new QVBoxLayout;
    m_activityRows->setContentsMargins(0, 0, 0, 0);
    m_activityRows->setSpacing(6);

    card->contentLayout()->addLayout(headerRow);
    card->contentLayout()->addSpacing(4);
    card->contentLayout()->addLayout(m_activityRows);
    card->contentLayout()->addStretch(1);
    return card;
}

// --- Security & tools -------------------------------------------------------
QWidget *AdminPage::buildSecurityTools()
{
    auto *card = new SectionCard(tr("Security & Tools"), tr("Additional administrative tools."),
                                 QStringLiteral("shield_check"));
    struct Tool
    {
        const char *icon, *title, *sub;
    };
    const Tool tools[] = {
        {"trash", "Clear All Licenses", "Revoke every licence (cannot be undone)."},
        {"download", "Export User Data", "Download a full user/licence list."},
        {"list", "View Audit Logs", "See all important admin actions."},
        {"settings", "Application Settings", "Configure app-wide settings."},
    };
    int index = 0;
    for (const Tool &t : tools) {
        auto *actionRow = new AdminActionRow(QString::fromLatin1(t.icon), tr(t.title), tr(t.sub),
                                             AdminActionRow::Tone::Neutral, card);
        const int which = index++;
        connect(actionRow, &QAbstractButton::clicked, this, [this, which] { runTool(which); });
        card->contentLayout()->addWidget(actionRow);
        card->contentLayout()->addSpacing(kActionRowGap);
    }
    card->contentLayout()->addStretch(1);
    return card;
}

// --- Actions ----------------------------------------------------------------
void AdminPage::addLicense()
{
    AddLicenseDialog dialog(window());
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString kind = dialog.kind();
    const int amount = dialog.amount();
    m_admin->createLicense(kind, kind == QLatin1String("duration") ? (amount > 0 ? amount : 30) : 0,
                           kind == QLatin1String("uses") ? (amount > 0 ? amount : 25) : 0,
                           dialog.note(), [this](const QJsonObject &r) {
                               if (!r.value(QStringLiteral("ok")).toBool()) {
                                   toast(r.value(QStringLiteral("reason")).toString(), false);
                                   return;
                               }
                               toast(tr("Licence created."));
                               loadLicenses();
                               loadOverview();
                               MessageDialog::information(
                                   window(), tr("Licence Created"), tr("New licence key"),
                                   QStringLiteral("<span style='font-size:11px;'>%1</span>")
                                       .arg(r.value(QStringLiteral("key")).toString()),
                                   MessageDialog::Tone::Success);
                           });
}

void AdminPage::showRow(const QJsonObject &data)
{
    MessageDialog::information(
        window(), tr("Licence Details"), data.value(QStringLiteral("user")).toString(),
        tr("Key id: %1<br>Type: %2<br>Status: %3<br>Machine: %4")
            .arg(data.value(QStringLiteral("keyId")).toString(),
                 data.value(QStringLiteral("type")).toString(),
                 data.value(QStringLiteral("status")).toString(),
                 data.value(QStringLiteral("hwid")).toString()),
        MessageDialog::Tone::Info);
}

void AdminPage::revokeRow(const QString &keyId)
{
    if (!MessageDialog::confirm(window(), tr("Revoke Licence"), tr("Revoke this licence?"),
                                tr("The user holding it loses access immediately."), tr("Revoke"),
                                MessageDialog::Tone::Danger, ModernButton::Variant::Danger))
        return;
    m_admin->revoke({keyId}, [this](const QJsonObject &r) {
        if (r.value(QStringLiteral("ok")).toBool()) {
            toast(tr("Licence revoked."));
            loadLicenses();
            loadOverview();
        } else {
            toast(r.value(QStringLiteral("reason")).toString(), false);
        }
    });
}

void AdminPage::runTool(int which)
{
    switch (which) {
    case 0:
        if (MessageDialog::confirm(window(), tr("Clear All Licenses"),
                                   tr("Revoke every licence?"),
                                   tr("This deletes all licences from the server and cannot be "
                                      "undone. Owner access is not affected."),
                                   tr("Clear Everything"), MessageDialog::Tone::Danger,
                                   ModernButton::Variant::Danger)) {
            m_admin->clearLicenses([this](const QJsonObject &r) {
                if (r.value(QStringLiteral("ok")).toBool()) {
                    toast(tr("All licences cleared."));
                    loadLicenses();
                    loadOverview();
                } else {
                    toast(r.value(QStringLiteral("reason")).toString(), false);
                }
            });
        }
        break;
    case 1:
        m_admin->exportUsers([this](const QJsonObject &r) {
            if (!r.value(QStringLiteral("ok")).toBool()) {
                toast(r.value(QStringLiteral("reason")).toString(), false);
                return;
            }
            const QJsonArray users = r.value(QStringLiteral("users")).toArray();
            QString body;
            for (const QJsonValue &v : users) {
                const QJsonObject u = v.toObject();
                body += QStringLiteral("%1 — %2%3<br>")
                            .arg(u.value(QStringLiteral("username")).toString(),
                                 u.value(QStringLiteral("license")).toString().isEmpty()
                                     ? tr("no licence")
                                     : u.value(QStringLiteral("license")).toString(),
                                 u.value(QStringLiteral("disabled")).toInt() ? tr("  (disabled)")
                                                                             : QString());
            }
            MessageDialog::information(window(), tr("Export User Data"),
                                       tr("%1 users").arg(users.size()), body,
                                       MessageDialog::Tone::Info);
        });
        break;
    case 2:
        m_admin->audit([this](const QJsonObject &r) {
            QString body;
            for (const QJsonValue &v : r.value(QStringLiteral("entries")).toArray()) {
                const QJsonObject e = v.toObject();
                const qint64 unix = qint64(e.value(QStringLiteral("unix")).toDouble());
                body += QStringLiteral("%1 — %2 %3 (%4)<br>")
                            .arg(e.value(QStringLiteral("actor")).toString(),
                                 e.value(QStringLiteral("action")).toString(),
                                 e.value(QStringLiteral("detail")).toString(),
                                 QDateTime::fromSecsSinceEpoch(unix).toString(
                                     QStringLiteral("dd MMM hh:mm AP")));
            }
            if (body.isEmpty())
                body = tr("No admin actions recorded yet.");
            MessageDialog::information(window(), tr("Audit Logs"), tr("Recent admin actions"),
                                       body, MessageDialog::Tone::Info);
        });
        break;
    default:
        emit settingsRequested();
        break;
    }
}

// --- Data loading -----------------------------------------------------------
void AdminPage::loadOverview()
{
    m_admin->overview([this](const QJsonObject &r) {
        if (!r.value(QStringLiteral("ok")).toBool()) {
            toast(r.value(QStringLiteral("reason")).toString(), false);
            return;
        }
        const Theme::Palette &c = Theme::colors();
        const QJsonObject stats = r.value(QStringLiteral("stats")).toObject();
        m_totalUsers->setValue(
            QString::number(stats.value(QStringLiteral("totalUsers")).toInt()));
        m_activeLicenses->setValue(
            QString::number(stats.value(QStringLiteral("activeLicenses")).toInt()));
        m_onlineNow->setValue(QString::number(stats.value(QStringLiteral("onlineNow")).toInt()));
        m_appVersion->setValue(stats.value(QStringLiteral("appVersion")).toString());

        applyNukeState(r.value(QStringLiteral("nukeActive")).toBool());

        const QJsonObject update = r.value(QStringLiteral("update")).toObject();
        m_updateVersionLabel->setText(
            tr("Version %1").arg(update.value(QStringLiteral("version")).toString()));
        const QString notes = update.value(QStringLiteral("notes")).toString();
        if (!notes.isEmpty() && m_updateNotes->toPlainText().isEmpty())
            m_updateNotes->setPlainText(notes);

        // --- Services ---
        while (QLayoutItem *item = m_serviceRows->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        int online = 0, total = 0;
        for (const QJsonValue &v : r.value(QStringLiteral("services")).toArray()) {
            const QJsonObject s = v.toObject();
            const bool isOnline = s.value(QStringLiteral("online")).toBool();
            ++total;
            if (isOnline)
                ++online;

            auto *serviceRow = new QWidget;
            serviceRow->setFixedHeight(18);
            auto *rl = new QHBoxLayout(serviceRow);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(8);

            auto *dot = new QLabel(serviceRow);
            dot->setFixedSize(8, 8);
            dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;")
                                   .arg((isOnline ? c.success : c.danger).name()));

            auto *name = new QLabel(s.value(QStringLiteral("name")).toString(), serviceRow);
            name->setFont(Theme::font(12));
            name->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));

            auto *state = new QLabel(isOnline ? tr("Online") : tr("Offline"), serviceRow);
            state->setFont(Theme::font(11, QFont::DemiBold));
            state->setStyleSheet(
                QStringLiteral("color: %1;").arg((isOnline ? c.success : c.danger).name()));

            rl->addWidget(dot, 0, Qt::AlignVCenter);
            rl->addWidget(name, 1);
            rl->addWidget(state, 0);
            m_serviceRows->addWidget(serviceRow);
        }
        const bool allUp = online == total && total > 0;
        m_allOperational->setText(allUp ? tr("All Systems Operational")
                                        : tr("%1 of %2 services online").arg(online).arg(total));
        m_allOperational->setStyleSheet(
            QStringLiteral("color: %1;").arg((allUp ? c.success : QColor(0xFF, 0xB0, 0x3A)).name()));

        // --- Activity ---
        while (QLayoutItem *item = m_activityRows->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        const QJsonArray activity = r.value(QStringLiteral("activity")).toArray();
        int shown = 0;
        for (const QJsonValue &v : activity) {
            if (shown++ >= 6)
                break; // Keep the card inside its height budget.
            const QJsonObject a = v.toObject();
            const qint64 unix = qint64(a.value(QStringLiteral("unix")).toDouble());

            auto *activityRow = new QWidget;
            activityRow->setFixedHeight(18);
            auto *rl = new QHBoxLayout(activityRow);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(7);

            auto *icon = new QLabel(activityRow);
            icon->setPixmap(IconProvider::pixmap(QStringLiteral("user"), 13, c.primaryBright,
                                                 devicePixelRatioF()));
            auto *who = new QLabel(a.value(QStringLiteral("username")).toString(), activityRow);
            who->setFont(Theme::font(11, QFont::Medium));
            who->setStyleSheet(QStringLiteral("color: %1;").arg(c.textPrimary.name()));
            auto *what = new QLabel(a.value(QStringLiteral("detail")).toString(), activityRow);
            what->setFont(Theme::font(11));
            what->setStyleSheet(QStringLiteral("color: %1;").arg(c.textSecondary.name()));
            auto *when = new QLabel(
                unix > 0 ? QDateTime::fromSecsSinceEpoch(unix).toString(QStringLiteral("hh:mm AP"))
                         : QString(),
                activityRow);
            when->setFont(Theme::font(10));
            when->setStyleSheet(QStringLiteral("color: %1;").arg(c.textMuted.name()));

            rl->addWidget(icon, 0);
            rl->addWidget(who, 0);
            rl->addWidget(what, 1);
            rl->addWidget(when, 0);
            m_activityRows->addWidget(activityRow);
        }
    });
}

void AdminPage::loadLicenses()
{
    QString status;
    switch (m_statusFilter ? m_statusFilter->currentIndex() : 0) {
    case 1: status = QStringLiteral("active"); break;
    case 2: status = QStringLiteral("expired"); break;
    case 3: status = QStringLiteral("revoked"); break;
    default: break;
    }
    m_admin->licenses(m_licenseSearch ? m_licenseSearch->text().trimmed() : QString(), status,
                      m_page, [this](const QJsonObject &r) {
                          if (!r.value(QStringLiteral("ok")).toBool())
                              return;
                          m_totalLicenses = r.value(QStringLiteral("total")).toInt();
                          applyLicenseRows(r.value(QStringLiteral("licenses")).toArray());
                      });
}

void AdminPage::applyLicenseRows(const QJsonArray &licenses)
{
    m_rows.clear();
    while (QLayoutItem *item = m_licenseRows->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    constexpr int kPageSize = 5;
    int index = (m_page - 1) * kPageSize + 1;
    for (const QJsonValue &v : licenses) {
        auto *row = new LicenseTableRow(index++, v.toObject(), this);
        m_rows.append(row);
        m_licenseRows->addWidget(row);
        connect(row->checkBox(), &QCheckBox::toggled, this, &AdminPage::updateSelectionState);
    }
    if (m_rows.isEmpty()) {
        auto *empty = new QLabel(tr("No licences yet — use “Add License” to create one."));
        empty->setFont(Theme::font(11));
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(64);
        empty->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::colors().textMuted.name()));
        m_licenseRows->addWidget(empty);
    }

    // A new page starts with nothing ticked.
    {
        const QSignalBlocker block(m_selectAll);
        m_selectAll->setChecked(false);
    }
    m_selectAll->setEnabled(!m_rows.isEmpty());
    updateSelectionState();

    const int from = m_totalLicenses == 0 ? 0 : (m_page - 1) * kPageSize + 1;
    const int to = qMin(m_totalLicenses, m_page * kPageSize);
    m_licenseCount->setText(tr("Showing %1-%2 of %3").arg(from).arg(to).arg(m_totalLicenses));

    while (QLayoutItem *item = m_pager->layout()->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    const int pages = qMax(1, (m_totalLicenses + kPageSize - 1) / kPageSize);
    const auto addPageButton = [this](const QString &text, int target, bool active, bool enabled) {
        auto *b = new ModernButton(text,
                                   active ? ModernButton::Variant::Primary
                                          : ModernButton::Variant::Secondary,
                                   m_pager);
        b->setFont(Theme::font(11, QFont::Medium));
        b->setFixedSize(28, 28);
        b->setCornerRadius(9);
        b->setEnabled(enabled);
        connect(b, &ModernButton::clicked, this, [this, target] {
            m_page = target;
            loadLicenses();
        });
        m_pager->layout()->addWidget(b);
    };
    addPageButton(QStringLiteral("‹"), qMax(1, m_page - 1), false, m_page > 1);
    for (int p = 1; p <= qMin(pages, 3); ++p)
        addPageButton(QString::number(p), p, p == m_page, true);
    if (pages > 4)
        addPageButton(QStringLiteral("…"), m_page, false, false);
    if (pages > 3)
        addPageButton(QString::number(pages), pages, m_page == pages, true);
    addPageButton(QStringLiteral("›"), qMin(pages, m_page + 1), false, m_page < pages);
}

void AdminPage::sendCommand(const QString &kind, const QString &title, const QString &body,
                            bool destructive)
{
    if (destructive
        && !MessageDialog::confirm(window(), title, tr("%1 for every client?").arg(title),
                                   tr("This command is sent to every signed-in client and takes "
                                      "effect the next time they check in. Your own owner "
                                      "account is not affected."),
                                   title, MessageDialog::Tone::Danger,
                                   ModernButton::Variant::Danger))
        return;

    m_admin->command(kind, QStringLiteral("all"), body, [this, title](const QJsonObject &r) {
        if (r.value(QStringLiteral("ok")).toBool())
            toast(tr("%1 sent to all clients.").arg(title));
        else
            toast(r.value(QStringLiteral("reason")).toString(), false);
    });
}

void AdminPage::applyNukeState(bool active)
{
    m_nukeActive = active;
    if (!m_nukeRow)
        return;
    if (active)
        m_nukeRow->setContent(QStringLiteral("refresh"), tr("RESTORE CLIENT"),
                              tr("Let nuked clients run again."),
                              AdminActionRow::Tone::Success);
    else
        m_nukeRow->setContent(QStringLiteral("trash"), tr("NUKE CLIENT"),
                              tr("Permanently wipes the application from the device."),
                              AdminActionRow::Tone::Danger);
}

void AdminPage::toast(const QString &text, bool ok)
{
    emit statusChanged(text, ok ? Theme::colors().success : Theme::colors().danger);
}
