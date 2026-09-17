#include "NavList.h"

#include "IconProvider.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace {
constexpr int kRowHeight = 50;
constexpr int kRowSpacing = 9;
constexpr int kIconSize = 24;
constexpr int kIconLeft = 22;
constexpr int kLabelGap = 20;
constexpr int kRadius = 12;
constexpr int kLabelSize = 17;

/// Rows offered by the rail, in order. Home is handled separately.
struct SectionRow
{
    MenuSection section;
    const char *icon;
    const char *label;
};

const SectionRow kRows[] = {
    {MenuSection::Optimize, "rocket", QT_TRANSLATE_NOOP("NavList", "Optimise")},
    {MenuSection::Enhance, "sparkles", QT_TRANSLATE_NOOP("NavList", "Enhance")},
    {MenuSection::Perform, "chart", QT_TRANSLATE_NOOP("NavList", "Perform")},
    {MenuSection::SystemInfo, "monitor", QT_TRANSLATE_NOOP("NavList", "System Info")},
    {MenuSection::Settings, "settings", QT_TRANSLATE_NOOP("NavList", "Settings")},
};
} // namespace

// ------------------------------------------------------------- NavItem -----

NavItem::NavItem(const QString &iconName, const QString &label, QWidget *parent)
    : QAbstractButton(parent)
    , m_iconName(iconName)
{
    setText(label);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kRowHeight);
    setAccessibleName(label);

    m_hoverAnimation = new QVariantAnimation(this);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { m_hover = v.toReal(); update(); });

    m_activeAnimation = new QVariantAnimation(this);
    m_activeAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_activeAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { m_activeProgress = v.toReal(); update(); });
}

void NavItem::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;

    m_activeAnimation->stop();
    m_activeAnimation->setDuration(Theme::durations().state);
    m_activeAnimation->setStartValue(m_activeProgress);
    m_activeAnimation->setEndValue(active ? 1.0 : 0.0);
    m_activeAnimation->start();
}

void NavItem::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    update();
}

QSize NavItem::sizeHint() const
{
    const QFontMetrics fm(Theme::font(kLabelSize, QFont::Medium));
    return QSize(kIconLeft + kIconSize + kLabelGap + fm.horizontalAdvance(text()) + 24, kRowHeight);
}

QSize NavItem::minimumSizeHint() const
{
    return QSize(170, kRowHeight);
}

void NavItem::animateHover(qreal to)
{
    m_hoverAnimation->stop();
    m_hoverAnimation->setDuration(Theme::durations().hover);
    m_hoverAnimation->setStartValue(m_hover);
    m_hoverAnimation->setEndValue(to);
    m_hoverAnimation->start();
}

void NavItem::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    const Theme::Palette &c = Theme::colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_locked)
        painter.setOpacity(0.45);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // --- Highlight ----------------------------------------------------------
    // A softly raised slate panel - not a saturated blue pill - that animates in
    // on hover and sits solid on the active row.
    const qreal lift = qMax(m_activeProgress, m_hover * 0.62);
    if (lift > 0.001) {
        QLinearGradient fill(body.topLeft(), body.bottomLeft());
        fill.setColorAt(0.0, Theme::alpha(c.cardHoverTop, int(235 * lift)));
        fill.setColorAt(1.0, Theme::alpha(c.cardBottom, int(215 * lift)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(body, kRadius, kRadius);

        QPen border(Theme::alpha(Theme::mix(c.cardBorder, c.primary, 0.5),
                                 int(190 * m_activeProgress + 90 * m_hover)));
        border.setWidthF(1.0);
        painter.setPen(border);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(body, kRadius, kRadius);
    }

    // --- Icon --------------------------------------------------------------
    // Icons keep the brand accent even when the row is idle, which is what makes
    // the rail feel like part of the product rather than a grey list.
    const QColor iconColor = m_locked
                                 ? c.textMuted
                                 : Theme::mix(c.primary, c.primaryBright,
                                              qMax(m_activeProgress, m_hover));
    const QString glyphName = m_locked ? QStringLiteral("lock") : m_iconName;
    const QPixmap glyph = IconProvider::pixmap(glyphName, kIconSize, iconColor,
                                               devicePixelRatioF());
    if (!glyph.isNull()) {
        painter.drawPixmap(QPointF(kIconLeft, body.center().y() - kIconSize / 2.0), glyph);
    }

    // --- Label -------------------------------------------------------------
    painter.setFont(Theme::font(kLabelSize, m_active ? QFont::DemiBold : QFont::Medium));
    painter.setPen(Theme::mix(Theme::mix(c.textSecondary, c.textPrimary, 0.55), c.textPrimary,
                              qMax(m_activeProgress, m_hover * 0.8)));
    const qreal textLeft = kIconLeft + kIconSize + kLabelGap;
    painter.drawText(QRectF(textLeft, body.top(), body.width() - textLeft - 12, body.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, text());
}

void NavItem::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    animateHover(1.0);
}

void NavItem::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    animateHover(0.0);
}

// ------------------------------------------------------------- NavList -----

NavList::NavList(QWidget *parent)
    : QWidget(parent)
{
    // Fixed vertically: the rows have fixed heights, so if the surrounding
    // layout ever squeezed this widget below its sizeHint the last row would be
    // clipped along the bottom (its highlight box losing its lower edge).
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kRowSpacing);

    m_home = new NavItem(QStringLiteral("home"), tr("Home"), this);
    layout->addWidget(m_home);
    connect(m_home, &QAbstractButton::clicked, this, [this] {
        setHomeActive();
        emit homeRequested();
    });

    for (const SectionRow &row : kRows) {
        auto *item = new NavItem(QString::fromLatin1(row.icon), tr(row.label), this);
        layout->addWidget(item);
        m_sections.append({row.section, item});

        const MenuSection section = row.section;
        connect(item, &QAbstractButton::clicked, this, [this, section] {
            if (m_locked) {
                emit activateRequested();
                return;
            }
            setActiveSection(section);
            emit sectionRequested(section);
        });
    }

    // Owner-only. Created up front but hidden; setAdminVisible() reveals it.
    m_admin = new NavItem(QStringLiteral("shield_check"), tr("Admin"), this);
    m_admin->hide();
    layout->addWidget(m_admin);
    connect(m_admin, &QAbstractButton::clicked, this, [this] {
        setActiveSection(MenuSection::Admin);
        emit sectionRequested(MenuSection::Admin);
    });

    setHomeActive();
}

void NavList::setAdminVisible(bool visible)
{
    if (m_admin)
        m_admin->setVisible(visible);
}

void NavList::clearActive()
{
    m_home->setActive(false);
    if (m_admin)
        m_admin->setActive(false);
    for (const auto &entry : m_sections)
        entry.second->setActive(false);
}

void NavList::setHomeActive()
{
    clearActive();
    m_home->setActive(true);
}

void NavList::setActiveSection(MenuSection section)
{
    clearActive();
    if (section == MenuSection::Admin && m_admin) {
        m_admin->setActive(true);
        return;
    }
    for (const auto &entry : m_sections) {
        if (entry.first == section)
            entry.second->setActive(true);
    }
}

void NavList::setLocked(bool locked)
{
    m_locked = locked;
    for (const auto &entry : m_sections)
        entry.second->setLocked(locked);
}
