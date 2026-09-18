// NextGen Tweaks - Discord licensing bot.
//
// The admin-facing half of the licensing system. Its slash commands drive the
// exact same libnglicense core and SQLite database that the validation server
// reads, so a key generated here in Discord is immediately redeemable in the
// desktop app. That is the "hand in hand" contract.
//
// Commands (all gated to members with the Manage Server permission, a role id
// set via NGT_ADMIN_ROLE, or a user id set via NGT_ADMIN_USERS):
//
//   /license create duration:<30d|12h|1y> [for:@user] [note:<text>]
//   /license create uses:<n>              [for:@user] [note:<text>]
//   /license create lifetime:true         [for:@user] [note:<text>]
//   /license info    keyid:<hex>
//   /license revoke  keyid:<hex>
//   /license list    [for:@user]
//   /license bind    keyid:<hex> user:@user
//
// The generated key is always sent ephemerally (only the admin sees it).
//
// Environment:
//   DISCORD_TOKEN   bot token (required)
//   NGT_DATA_DIR    data directory holding issuer_key.bin + licenses.db
//   NGT_GUILD_ID    optional: register commands to one guild for instant dev
//   NGT_ADMIN_ROLE  optional: role id(s) allowed to run commands. One id, or
//                   several separated by commas (e.g. "12345,67890").
//   NGT_ADMIN_USERS optional: user id(s) allowed to run commands, regardless of
//                   roles. One id, or several separated by commas.
#include "nglicense/Crypto.h"
#include "nglicense/Database.h"
#include "nglicense/License.h"
#include "nglicense/LicenseService.h"

#include <dpp/dpp.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

using namespace ngl;

namespace {

std::string envOr(const char *name, const std::string &fallback)
{
    const char *value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

Bytes readFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::optional<std::array<std::uint8_t, 32>> load32(const std::string &path)
{
    Bytes raw = readFile(path);
    if (raw.size() != 32)
        return std::nullopt;
    std::array<std::uint8_t, 32> out{};
    std::copy(raw.begin(), raw.end(), out.begin());
    return out;
}

/// Splits a config string of ids into the individual ids. Ids may be separated
/// by commas or whitespace, so "111, 222 333" -> {"111","222","333"}.
std::vector<std::string> splitIds(const std::string &list)
{
    std::vector<std::string> out;
    std::string current;
    for (const char c : list) {
        if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty())
        out.push_back(current);
    return out;
}

/// True when the member may run licensing commands.
///
/// adminUsers is NGT_ADMIN_USERS: user ids granted access outright, whatever
/// roles they hold. adminRoles is NGT_ADMIN_ROLE: role ids that grant access.
/// Both take one id or several separated by commas/spaces; any match qualifies,
/// as does the guild owner and anyone with Manage Server / Administrator.
bool isAdmin(const dpp::slashcommand_t &event, const std::string &adminRoles,
             const std::string &adminUsers)
{
    // An explicitly allow-listed user id always qualifies, regardless of roles
    // or server permissions.
    const std::string userId =
        std::to_string(static_cast<std::uint64_t>(event.command.get_issuing_user().id));
    for (const std::string &want : splitIds(adminUsers)) {
        if (userId == want)
            return true;
    }

    // Guild owner and anyone with Manage Server always qualifies.
    const dpp::permission perms = event.command.get_resolved_permission(
        event.command.get_issuing_user().id);
    if (perms.has(dpp::p_manage_guild) || perms.has(dpp::p_administrator))
        return true;

    const std::vector<std::string> allowedRoles = splitIds(adminRoles);
    if (!allowedRoles.empty()) {
        for (const dpp::snowflake &role : event.command.member.get_roles()) {
            const std::string roleId = std::to_string(role);
            for (const std::string &want : allowedRoles) {
                if (roleId == want)
                    return true;
            }
        }
    }
    return false;
}

std::string formatTime(std::int64_t unix)
{
    if (unix == 0)
        return "-";
    std::time_t t = static_cast<std::time_t>(unix);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", std::gmtime(&t));
    return buffer;
}

/// Brand blue from the NextGen Tweaks logo, used on every embed.
constexpr std::uint32_t kBrandBlue = 0x1E88FF;

/// The logo bytes, loaded once at startup (empty if not found).
std::string g_logoBytes;

/// A branded embed: blue, with the logo as thumbnail + author icon when present.
dpp::embed brandEmbed(const std::string &title)
{
    dpp::embed embed;
    embed.set_title(title).set_color(kBrandBlue);
    if (!g_logoBytes.empty()) {
        embed.set_thumbnail("attachment://logo.png");
        embed.set_author("NextGen Tweaks", "", "attachment://logo.png");
    }
    embed.set_footer(dpp::embed_footer().set_text("NextGen Tweaks licensing"));
    return embed;
}

/// Wraps an embed in an ephemeral message, attaching the logo so the embed's
/// thumbnail resolves.
dpp::message brandMessage(const dpp::embed &embed)
{
    dpp::message msg;
    if (!g_logoBytes.empty())
        msg.add_file("logo.png", g_logoBytes);
    msg.add_embed(embed);
    msg.set_flags(dpp::m_ephemeral);
    return msg;
}

/// A simple ephemeral error embed (red), still branded.
dpp::message errorMessage(const std::string &text)
{
    dpp::embed embed;
    embed.set_title("Something went wrong").set_color(0xE0354B).set_description(text);
    dpp::message msg;
    msg.add_embed(embed);
    msg.set_flags(dpp::m_ephemeral);
    return msg;
}

// --- Option readers over a flat option list --------------------------------

std::string optString(const std::vector<dpp::command_data_option> &opts, const std::string &name)
{
    for (const auto &o : opts) {
        if (o.name == name && std::holds_alternative<std::string>(o.value))
            return std::get<std::string>(o.value);
    }
    return {};
}

std::int64_t optInt(const std::vector<dpp::command_data_option> &opts, const std::string &name)
{
    for (const auto &o : opts) {
        if (o.name == name && std::holds_alternative<std::int64_t>(o.value))
            return std::get<std::int64_t>(o.value);
    }
    return 0;
}

bool optBool(const std::vector<dpp::command_data_option> &opts, const std::string &name)
{
    for (const auto &o : opts) {
        if (o.name == name && std::holds_alternative<bool>(o.value))
            return std::get<bool>(o.value);
    }
    return false;
}

std::string optUser(const std::vector<dpp::command_data_option> &opts, const std::string &name)
{
    for (const auto &o : opts) {
        if (o.name == name && std::holds_alternative<dpp::snowflake>(o.value))
            return std::to_string(std::get<dpp::snowflake>(o.value));
    }
    return {};
}

/// Shared licence-issuing path for both /gen and /license create. Fills the
/// branded reply and, when minted "for" a user, DMs them the key.
/// Writes one line into the shared audit log the app's admin panel reads, so a
/// licence generated or revoked from Discord shows up there with the Discord
/// user's name and id. The caller holds the database mutex.
void logAudit(Database &db, const std::string &actor, const std::string &action,
              const std::string &detail)
{
    db.execute("INSERT INTO audit (actor, action, detail, created_unix) VALUES (?,?,?,?)",
               {actor, action, detail, nowUnix()});
}

/// The bot may start before the server has created its extra tables, so it
/// makes sure the audit table exists (and its heartbeat row can be written).
void ensureAdminTables(Database &db)
{
    db.execute("CREATE TABLE IF NOT EXISTS audit ("
               "  id INTEGER PRIMARY KEY AUTOINCREMENT, actor TEXT, action TEXT NOT NULL,"
               "  detail TEXT, created_unix INTEGER NOT NULL)");
    db.execute("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)");
}

/// "blu (123456789012345678)" - what every Discord action is attributed to.
std::string actorOf(const dpp::slashcommand_t &event)
{
    const auto &user = event.command.get_issuing_user();
    return user.username + " (" + std::to_string(static_cast<std::uint64_t>(user.id)) + ")";
}

/// True when a licence is bound to the owner account. Owner licences are hidden
/// from /viewlicenses and protected from /revoke, /clearlicenses and /resethwid,
/// so the bot can never touch the owner's own access. The caller holds the lock.
bool isOwnerLicence(Database &db, const std::string &keyId)
{
    bool owner = false;
    db.query("SELECT COALESCE(a.is_owner, 0) FROM licenses l"
             " LEFT JOIN accounts a ON a.id = l.bound_account_id WHERE l.key_id=?",
             {keyId}, [&](const Row &row) { owner = row.integer(0) != 0; });
    return owner;
}

/// A readable random password for an admin-driven reset. Avoids ambiguous
/// characters (0/O, 1/l/I) so it can be typed or read aloud without confusion.
std::string randomPassword(std::size_t length = 12)
{
    static const char charset[] = "ABCDEFGHJKMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    const Bytes bytes = crypto::randomBytes(length);
    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i)
        out.push_back(charset[bytes[i] % (sizeof(charset) - 1)]);
    return out;
}

/// Finds the account id behind a username, or a licence key / key id bound to
/// it. Returns 0 when nothing matches; fills \a username and \a isOwner. The
/// caller holds the db lock.
std::int64_t resolveAccount(Database &db, LicenseService &service, const std::string &username,
                            const std::string &licenseIn, std::string *outUsername, bool *isOwner)
{
    std::int64_t accountId = 0;
    if (!username.empty()) {
        db.query("SELECT id, username, COALESCE(is_owner,0) FROM accounts WHERE username=?",
                 {username}, [&](const Row &row) {
                     accountId = row.integer(0);
                     if (outUsername)
                         *outUsername = row.text(1);
                     if (isOwner)
                         *isOwner = row.integer(2) != 0;
                 });
        return accountId;
    }
    if (!licenseIn.empty()) {
        const std::string keyId = service.resolveKeyId(licenseIn);
        if (keyId.empty())
            return 0;
        db.query("SELECT a.id, a.username, COALESCE(a.is_owner,0) FROM licenses l"
                 " JOIN accounts a ON a.id = l.bound_account_id WHERE l.key_id=?",
                 {keyId}, [&](const Row &row) {
                     accountId = row.integer(0);
                     if (outUsername)
                         *outUsername = row.text(1);
                     if (isOwner)
                         *isOwner = row.integer(2) != 0;
                 });
    }
    return accountId;
}

dpp::message issueLicence(dpp::cluster &bot, LicenseService &service, const std::string &issuedBy,
                          const std::string &duration, std::int64_t uses, bool lifetime,
                          const std::string &forUser, const std::string &note)
{
    IssueSpec spec;
    spec.issuedBy = issuedBy;
    spec.discordUserId = forUser;
    spec.notes = note;

    if (!duration.empty()) {
        auto seconds = parseDuration(duration);
        if (!seconds)
            return errorMessage("Bad duration. Use e.g. 30d, 12h, 1y.");
        spec.type = LicenseType::Duration;
        spec.durationSeconds = *seconds;
    } else if (uses > 0) {
        spec.type = LicenseType::Uses;
        spec.maxUses = static_cast<std::uint32_t>(uses);
    } else if (lifetime) {
        spec.type = LicenseType::Lifetime;
    } else {
        return errorMessage("Choose duration:, uses: or lifetime:.");
    }

    IssueResult result = service.issue(spec);
    if (!result.ok)
        return errorMessage("Could not issue: " + result.message);

    dpp::embed embed = brandEmbed("Licence Generated");
    embed.add_field("Type", toString(spec.type), true);
    embed.add_field("Key id", result.keyId, true);
    if (!forUser.empty())
        embed.add_field("For", "<@" + forUser + ">", true);
    embed.add_field("Key", "```" + result.keyString + "```", false);
    embed.set_description("Send this key to the customer. They activate it inside NextGen Tweaks.");

    if (!forUser.empty()) {
        bot.direct_message_create(
            dpp::snowflake(std::stoull(forUser)),
            dpp::message("Your NextGen Tweaks licence key:\n```" + result.keyString
                         + "```\nSign in to the app, then paste it into the activation prompt."));
    }
    return brandMessage(embed);
}

} // namespace

int main()
{
    const std::string token = envOr("DISCORD_TOKEN", "");
    if (token.empty()) {
        std::cerr << "Set DISCORD_TOKEN to your bot token.\n";
        return 1;
    }

    const std::string dataDir = envOr("NGT_DATA_DIR", "data");
    const std::string guildId = envOr("NGT_GUILD_ID", "");
    // Who may run the commands. Defaults are baked in so the bot works before
    // any env var is set; NGT_ADMIN_ROLE / NGT_ADMIN_USERS override them (each a
    // single id, or several separated by commas). Role or user, either grants
    // access - as does Manage Server / Administrator.
    const std::string adminRole = envOr("NGT_ADMIN_ROLE", "1210087427109298220");
    const std::string adminUsers = envOr("NGT_ADMIN_USERS", "1235828735803133972");

    // The logo shown in every embed. Defaults to the repo asset; override with
    // NGT_LOGO_PATH if the bot runs from elsewhere.
    const std::string logoPath =
        envOr("NGT_LOGO_PATH", "../assets/logo/nextgen_tweaks_mark.png");
    {
        Bytes raw = readFile(logoPath);
        if (raw.empty())
            raw = readFile("assets/logo/nextgen_tweaks_mark.png");
        g_logoBytes.assign(raw.begin(), raw.end());
    }

    auto seed = load32(dataDir + "/issuer_key.bin");
    auto pub = load32(dataDir + "/issuer_public.bin");
    if (!seed || !pub) {
        std::cerr << "Missing issuer key in " << dataDir
                  << ". Run 'ngtlicense keygen' first.\n";
        return 1;
    }

    // The database and service are shared, so a mutex serialises access from the
    // bot's event threads.
    auto db = std::make_shared<Database>();
    std::string error;
    if (!db->open(dataDir + "/licenses.db", &error)) {
        std::cerr << "Database error: " << error << "\n";
        return 1;
    }
    auto service = std::make_shared<LicenseService>(*db, *pub);
    service->setIssuerSeed(*seed);
    auto dbMutex = std::make_shared<std::mutex>();

    // The admin panel's audit log and "Discord Bot: Online" indicator both live
    // in the shared database, so make sure their tables exist even if the bot
    // started first.
    {
        std::lock_guard<std::mutex> lock(*dbMutex);
        ensureAdminTables(*db);
    }

    dpp::cluster bot(token, dpp::i_default_intents);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_ready([&bot, guildId](const dpp::ready_t &) {
        if (dpp::run_once<struct register_commands>()) {
            // One /license command with create / info / revoke subcommands.
            dpp::slashcommand cmd("license", "Manage NextGen Tweaks licences", bot.me.id);

            dpp::command_option create(dpp::co_sub_command, "create", "Generate a new licence");
            create.add_option(
                dpp::command_option(dpp::co_string, "duration",
                                    "Time-limited, e.g. 30d, 12h, 1y", false));
            create.add_option(
                dpp::command_option(dpp::co_integer, "uses", "Use-limited count", false));
            create.add_option(
                dpp::command_option(dpp::co_boolean, "lifetime", "Never expires", false));
            create.add_option(
                dpp::command_option(dpp::co_user, "for", "Who this licence is for", false));
            create.add_option(
                dpp::command_option(dpp::co_string, "note", "Freeform note", false));
            cmd.add_option(create);

            dpp::command_option info(dpp::co_sub_command, "info", "Show a licence's state");
            info.add_option(
                dpp::command_option(dpp::co_string, "keyid", "Licence key id", true));
            cmd.add_option(info);

            dpp::command_option revoke(dpp::co_sub_command, "revoke", "Revoke a licence");
            revoke.add_option(
                dpp::command_option(dpp::co_string, "keyid", "Licence key id", true));
            cmd.add_option(revoke);

            dpp::command_option list(dpp::co_sub_command, "list", "List recent licences");
            list.add_option(
                dpp::command_option(dpp::co_user, "for", "Only this user's licences", false));
            cmd.add_option(list);

            dpp::command_option bind(dpp::co_sub_command, "bind",
                                     "Record which user a licence is for");
            bind.add_option(dpp::command_option(dpp::co_string, "keyid", "Licence key id", true));
            bind.add_option(dpp::command_option(dpp::co_user, "user", "The user", true));
            cmd.add_option(bind);

            // --- /gen : generate a licence -----------------------------------
            dpp::slashcommand gen("gen", "Generate a NextGen Tweaks licence", bot.me.id);
            gen.add_option(dpp::command_option(dpp::co_string, "duration",
                                               "Time-limited, e.g. 30d, 12h, 1y", false));
            gen.add_option(
                dpp::command_option(dpp::co_integer, "uses", "Use-limited count", false));
            gen.add_option(
                dpp::command_option(dpp::co_boolean, "lifetime", "Never expires", false));
            gen.add_option(
                dpp::command_option(dpp::co_user, "for", "Who this licence is for", false));
            gen.add_option(dpp::command_option(dpp::co_string, "note", "Freeform note", false));

            // --- /revoke : revoke by pasted key or key id --------------------
            dpp::slashcommand rev("revoke", "Revoke a licence (paste the key or key id)",
                                  bot.me.id);
            rev.add_option(dpp::command_option(dpp::co_string, "license",
                                               "The licence key (NGTL-...) or its key id", true));

            // --- /blacklist : ban a licence or a device ----------------------
            dpp::slashcommand bl("blacklist", "Blacklist a licence key or a machine id",
                                 bot.me.id);
            bl.add_option(dpp::command_option(dpp::co_string, "license",
                                              "Licence key (NGTL-...) or key id", false));
            bl.add_option(
                dpp::command_option(dpp::co_string, "machine", "Machine id to ban", false));
            bl.add_option(dpp::command_option(dpp::co_string, "reason", "Why", false));

            // --- /unblacklist : lift a ban -----------------------------------
            dpp::slashcommand unbl("unblacklist", "Remove a licence or machine from the blacklist",
                                   bot.me.id);
            unbl.add_option(dpp::command_option(dpp::co_string, "license",
                                                "Licence key (NGTL-...) or key id", false));
            unbl.add_option(
                dpp::command_option(dpp::co_string, "machine", "Machine id to unban", false));

            // --- /resethwid : unbind a licence so it can move to a new machine
            dpp::slashcommand rhw("resethwid", "Reset a licence's HWID so it can be re-activated",
                                  bot.me.id);
            rhw.add_option(dpp::command_option(dpp::co_string, "license",
                                               "The licence key (NGTL-...) or its key id", true));

            // --- /clearlicenses : wipe every (non-owner) licence -------------
            dpp::slashcommand clr("clearlicenses", "Revoke and delete every licence (not yours)",
                                  bot.me.id);

            // --- /viewlicenses : list every (non-owner) licence --------------
            dpp::slashcommand viewl("viewlicenses", "List all licences (yours is never shown)",
                                    bot.me.id);

            // --- /resetpassword : set a new password for a user's account ----
            dpp::slashcommand rpw("resetpassword",
                                  "Reset an account's password by username or licence", bot.me.id);
            rpw.add_option(dpp::command_option(dpp::co_string, "username",
                                               "The account username", false));
            rpw.add_option(dpp::command_option(dpp::co_string, "license",
                                               "A licence key (NGTL-...) or key id bound to the "
                                               "account",
                                               false));
            rpw.add_option(dpp::command_option(dpp::co_string, "password",
                                               "New password (leave blank to auto-generate one)",
                                               false));

            if (!guildId.empty()) {
                const dpp::snowflake gid(std::stoull(guildId));
                bot.guild_command_create(cmd, gid);
                bot.guild_command_create(gen, gid);
                bot.guild_command_create(rev, gid);
                bot.guild_command_create(bl, gid);
                bot.guild_command_create(unbl, gid);
                bot.guild_command_create(rhw, gid);
                bot.guild_command_create(clr, gid);
                bot.guild_command_create(viewl, gid);
                bot.guild_command_create(rpw, gid);
            } else {
                bot.global_command_create(cmd);
                bot.global_command_create(gen);
                bot.global_command_create(rev);
                bot.global_command_create(bl);
                bot.global_command_create(unbl);
                bot.global_command_create(rhw);
                bot.global_command_create(clr);
                bot.global_command_create(viewl);
                bot.global_command_create(rpw);
            }
        }
    });

    bot.on_slashcommand([&bot, db, service, dbMutex, adminRole,
                         adminUsers](const dpp::slashcommand_t &event) {
        const std::string cmdName = event.command.get_command_name();
        if (cmdName != "license" && cmdName != "gen" && cmdName != "revoke"
            && cmdName != "blacklist" && cmdName != "unblacklist" && cmdName != "resethwid"
            && cmdName != "clearlicenses" && cmdName != "viewlicenses"
            && cmdName != "resetpassword")
            return;

        if (!isAdmin(event, adminRole, adminUsers)) {
            event.reply(errorMessage("You need the Manage Server permission to do that."));
            return;
        }

        std::lock_guard<std::mutex> lock(*dbMutex);

        const std::string issuedBy = actorOf(event);
        const auto &topOpts = event.command.get_command_interaction().options;

        // ---- /gen : generate a licence -------------------------------------
        if (cmdName == "gen") {
            event.reply(issueLicence(bot, *service, issuedBy, optString(topOpts, "duration"),
                                     optInt(topOpts, "uses"), optBool(topOpts, "lifetime"),
                                     optUser(topOpts, "for"), optString(topOpts, "note")));
            logAudit(*db, issuedBy, "Generated licence (Discord /gen)", optString(topOpts, "note"));
            return;
        }

        // ---- /revoke : paste a key or key id -------------------------------
        if (cmdName == "revoke") {
            const std::string keyId = service->resolveKeyId(optString(topOpts, "license"));
            if (keyId.empty()) {
                event.reply(errorMessage("That licence key or id was not recognised."));
                return;
            }
            if (isOwnerLicence(*db, keyId)) {
                event.reply(errorMessage("That is an owner licence and cannot be revoked."));
                return;
            }
            std::string err;
            const bool ok = service->revoke(keyId, &err);
            if (ok)
                logAudit(*db, issuedBy, "Revoked licence (Discord /revoke)", keyId);
            dpp::embed e = brandEmbed(ok ? "Licence Revoked" : "Licence Not Found");
            if (!ok)
                e.set_color(0xE0354B);
            e.add_field("Key id", keyId, true);
            e.set_description(ok ? "This licence can no longer be used or activated."
                                 : "No licence matched that key.");
            event.reply(brandMessage(e));
            return;
        }

        // ---- /blacklist : ban a licence and/or a device --------------------
        if (cmdName == "blacklist") {
            const std::string licenseIn = optString(topOpts, "license");
            const std::string machine = optString(topOpts, "machine");
            const std::string reason = optString(topOpts, "reason");
            if (licenseIn.empty() && machine.empty()) {
                event.reply(errorMessage("Provide a licence or a machine id to blacklist."));
                return;
            }
            dpp::embed e = brandEmbed("Blacklisted");
            std::string err;
            if (!licenseIn.empty()) {
                const std::string keyId = service->resolveKeyId(licenseIn);
                if (keyId.empty()) {
                    event.reply(errorMessage("That licence key or id was not recognised."));
                    return;
                }
                if (isOwnerLicence(*db, keyId)) {
                    event.reply(errorMessage("That is an owner licence and cannot be blacklisted."));
                    return;
                }
                service->addToBlacklist("license", keyId, reason, issuedBy, &err);
                e.add_field("Licence", keyId + " (revoked)", true);
            }
            if (!machine.empty()) {
                service->addToBlacklist("machine", machine, reason, issuedBy, &err);
                e.add_field("Machine", machine, true);
            }
            if (!reason.empty())
                e.add_field("Reason", reason, false);
            e.set_description("Blocked from validating or activating in the app.");
            logAudit(*db, issuedBy, "Blacklisted (Discord /blacklist)",
                     (licenseIn.empty() ? machine : licenseIn) + (reason.empty() ? "" : " - " + reason));
            event.reply(brandMessage(e));
            return;
        }

        // ---- /unblacklist : lift a ban -------------------------------------
        if (cmdName == "unblacklist") {
            const std::string licenseIn = optString(topOpts, "license");
            const std::string machine = optString(topOpts, "machine");
            if (licenseIn.empty() && machine.empty()) {
                event.reply(errorMessage("Provide a licence or a machine id to unban."));
                return;
            }
            dpp::embed e = brandEmbed("Removed from Blacklist");
            bool any = false;
            std::string err;
            if (!licenseIn.empty()) {
                const std::string keyId = service->resolveKeyId(licenseIn);
                const std::string target = keyId.empty() ? licenseIn : keyId;
                any = service->removeFromBlacklist("license", target, &err) || any;
                e.add_field("Licence", target, true);
            }
            if (!machine.empty()) {
                any = service->removeFromBlacklist("machine", machine, &err) || any;
                e.add_field("Machine", machine, true);
            }
            e.set_description(any ? "It can validate again (a revoked licence stays revoked)."
                                  : "Nothing matched — it was not on the blacklist.");
            if (any)
                logAudit(*db, issuedBy, "Removed from blacklist (Discord /unblacklist)",
                         licenseIn.empty() ? machine : licenseIn);
            event.reply(brandMessage(e));
            return;
        }

        // ---- /resethwid : unbind so the licence can move to a new machine --
        if (cmdName == "resethwid") {
            const std::string keyId = service->resolveKeyId(optString(topOpts, "license"));
            if (keyId.empty()) {
                event.reply(errorMessage("That licence key or id was not recognised."));
                return;
            }
            if (isOwnerLicence(*db, keyId)) {
                event.reply(errorMessage("That is an owner licence and cannot be reset."));
                return;
            }
            // Clear the machine + account binding and drop activation history, so
            // the next person to activate the key binds it to their machine.
            db->execute("UPDATE licenses SET bound_machine='', bound_account_id=NULL"
                        " WHERE key_id=?", {keyId});
            db->execute("UPDATE accounts SET license_key_id=NULL WHERE license_key_id=?", {keyId});
            db->execute("DELETE FROM activations WHERE key_id=?", {keyId});
            logAudit(*db, issuedBy, "Reset HWID (Discord /resethwid)", keyId);

            dpp::embed e = brandEmbed("HWID Reset");
            e.add_field("Key id", keyId, true);
            e.set_description("The licence is unbound and can be activated on a new machine.");
            event.reply(brandMessage(e));
            return;
        }

        // ---- /resetpassword : set a new password for an account ------------
        if (cmdName == "resetpassword") {
            const std::string username = optString(topOpts, "username");
            const std::string licenseIn = optString(topOpts, "license");
            std::string password = optString(topOpts, "password");

            if (username.empty() && licenseIn.empty()) {
                event.reply(errorMessage("Give a username: or a license: to reset."));
                return;
            }

            std::string foundUser;
            bool owner = false;
            const std::int64_t accountId =
                resolveAccount(*db, *service, username, licenseIn, &foundUser, &owner);
            if (accountId == 0) {
                event.reply(errorMessage(username.empty()
                                             ? "No account is bound to that licence."
                                             : "No account matches that username."));
                return;
            }
            if (owner) {
                event.reply(errorMessage("That is the owner account and cannot be reset here."));
                return;
            }

            // No password given -> mint a temporary one to hand to the customer.
            const bool generated = password.empty();
            if (generated)
                password = randomPassword();
            if (password.size() < 6) {
                event.reply(errorMessage("Choose a password of at least 6 characters."));
                return;
            }

            std::string err;
            if (!service->resetPassword(accountId, password, &err)) {
                event.reply(errorMessage("Could not reset the password: " + err));
                return;
            }
            logAudit(*db, issuedBy, "Reset account password (Discord /resetpassword)", foundUser);

            dpp::embed e = brandEmbed("Password Reset");
            e.add_field("Account", foundUser, true);
            e.add_field("New password", "```" + password + "```", false);
            e.set_description(generated
                                  ? "Send this temporary password to the user. They can sign in "
                                    "with it and change it later."
                                  : "The account's password has been changed to the one you "
                                    "supplied.");
            event.reply(brandMessage(e));
            return;
        }

        // ---- /clearlicenses : delete every non-owner licence ---------------
        if (cmdName == "clearlicenses") {
            std::int64_t removed = 0;
            db->query("SELECT COUNT(*) FROM licenses WHERE key_id NOT IN"
                      " (SELECT l.key_id FROM licenses l JOIN accounts a"
                      "  ON a.id=l.bound_account_id WHERE a.is_owner=1)",
                      {}, [&](const Row &row) { removed = row.integer(0); });
            // Delete every licence except those bound to an owner account.
            db->execute("DELETE FROM licenses WHERE key_id NOT IN"
                        " (SELECT l.key_id FROM licenses l JOIN accounts a"
                        "  ON a.id=l.bound_account_id WHERE a.is_owner=1)");
            db->execute("DELETE FROM activations WHERE key_id NOT IN (SELECT key_id FROM licenses)");
            db->execute("UPDATE accounts SET license_key_id=NULL WHERE is_owner=0");
            logAudit(*db, issuedBy, "Cleared all licences (Discord /clearlicenses)",
                     std::to_string(removed) + " removed");

            dpp::embed e = brandEmbed("Licences Cleared");
            e.add_field("Removed", std::to_string(removed), true);
            e.set_description("Every licence has been deleted. Owner access is untouched.");
            event.reply(brandMessage(e));
            return;
        }

        // ---- /viewlicenses : list all non-owner licences -------------------
        if (cmdName == "viewlicenses") {
            dpp::embed e = brandEmbed("Licences");
            int shown = 0;
            std::int64_t total = 0;
            db->query(
                "SELECT l.key_id, l.type, l.status, l.expiry_unix, a.username, l.bound_machine"
                " FROM licenses l LEFT JOIN accounts a ON a.id=l.bound_account_id"
                " WHERE COALESCE(a.is_owner,0)=0 ORDER BY l.created_unix DESC",
                {}, [&](const Row &row) {
                    ++total;
                    if (shown >= 15)
                        return; // Embeds cap at 25 fields; keep well under.
                    ++shown;
                    const std::string keyId = row.text(0);
                    const bool revoked = row.integer(2) != 0;
                    const std::int64_t expiry = row.integer(3);
                    std::string status = revoked ? "revoked"
                                         : (expiry > 0 && expiry < nowUnix() ? "expired" : "active");
                    const std::string user = row.text(4).empty() ? "unbound" : row.text(4);
                    const std::string hwid = row.text(5).empty() ? "not activated"
                                                                 : row.text(5).substr(0, 8);
                    e.add_field(keyId.substr(0, 12),
                                toString(static_cast<LicenseType>(row.integer(1))) + " | " + status
                                    + " | " + user + " | " + hwid,
                                false);
                });
            e.set_description(total == 0
                                  ? "No licences yet."
                                  : "Showing " + std::to_string(shown) + " of "
                                        + std::to_string(total) + " licence(s). Your owner "
                                        + "licence is never listed.");
            event.reply(brandMessage(e));
            return;
        }

        // ---- /license <subcommand> -----------------------------------------
        const auto sub = event.command.get_command_interaction().options[0];
        const std::string subName = sub.name;

        if (subName == "create") {
            IssueSpec spec;
            spec.issuedBy = actorOf(event);

            auto getString = [&](const std::string &name) -> std::string {
                for (const auto &opt : sub.options) {
                    if (opt.name == name && std::holds_alternative<std::string>(opt.value))
                        return std::get<std::string>(opt.value);
                }
                return {};
            };
            auto getInt = [&](const std::string &name) -> std::int64_t {
                for (const auto &opt : sub.options) {
                    if (opt.name == name && std::holds_alternative<std::int64_t>(opt.value))
                        return std::get<std::int64_t>(opt.value);
                }
                return 0;
            };
            auto getBool = [&](const std::string &name) -> bool {
                for (const auto &opt : sub.options) {
                    if (opt.name == name && std::holds_alternative<bool>(opt.value))
                        return std::get<bool>(opt.value);
                }
                return false;
            };
            auto getUser = [&](const std::string &name) -> std::string {
                for (const auto &opt : sub.options) {
                    if (opt.name == name && std::holds_alternative<dpp::snowflake>(opt.value))
                        return std::to_string(std::get<dpp::snowflake>(opt.value));
                }
                return {};
            };

            const std::string duration = getString("duration");
            const std::int64_t uses = getInt("uses");
            const bool lifetime = getBool("lifetime");
            spec.discordUserId = getUser("for");
            spec.notes = getString("note");

            if (!duration.empty()) {
                auto seconds = parseDuration(duration);
                if (!seconds) {
                    event.reply(dpp::message("Bad duration. Use e.g. 30d, 12h, 1y.")
                                    .set_flags(dpp::m_ephemeral));
                    return;
                }
                spec.type = LicenseType::Duration;
                spec.durationSeconds = *seconds;
            } else if (uses > 0) {
                spec.type = LicenseType::Uses;
                spec.maxUses = static_cast<std::uint32_t>(uses);
            } else if (lifetime) {
                spec.type = LicenseType::Lifetime;
            } else {
                event.reply(dpp::message("Choose duration:, uses: or lifetime:")
                                .set_flags(dpp::m_ephemeral));
                return;
            }

            IssueResult result = service->issue(spec);
            if (!result.ok) {
                event.reply(dpp::message("Could not issue: " + result.message)
                                .set_flags(dpp::m_ephemeral));
                return;
            }
            logAudit(*db, actorOf(event), "Created licence (Discord /license create)",
                     result.keyId);

            dpp::embed embed;
            embed.set_title("Licence created")
                .set_color(0x1E88FF)
                .add_field("Type", toString(spec.type), true)
                .add_field("Key id", result.keyId, true)
                .add_field("Key", "```" + result.keyString + "```", false)
                .set_footer(dpp::embed_footer().set_text(
                    "Send this key to the customer. They paste it into NextGen Tweaks."));
            if (!spec.discordUserId.empty())
                embed.add_field("For", "<@" + spec.discordUserId + ">", true);

            event.reply(dpp::message().add_embed(embed).set_flags(dpp::m_ephemeral));

            // Best-effort: DM the key to the user it was minted for.
            if (!spec.discordUserId.empty()) {
                bot.direct_message_create(
                    dpp::snowflake(std::stoull(spec.discordUserId)),
                    dpp::message("Your NextGen Tweaks licence key:\n```" + result.keyString
                                 + "```\nPaste it into the app when you sign in or create an "
                                   "account."));
            }
            return;
        }

        if (subName == "list") {
            std::string forUser;
            for (const auto &opt : sub.options) {
                if (opt.name == "for" && std::holds_alternative<dpp::snowflake>(opt.value))
                    forUser = std::to_string(std::get<dpp::snowflake>(opt.value));
            }

            const std::vector<LicenseInfo> licences = service->listLicenses(forUser, 20);
            if (licences.empty()) {
                event.reply(dpp::message("No licences found.").set_flags(dpp::m_ephemeral));
                return;
            }

            std::string body;
            for (const LicenseInfo &info : licences) {
                body += "`" + info.keyId + "`  " + toString(info.type);
                if (info.revoked)
                    body += " (revoked)";
                else if (info.type == LicenseType::Uses)
                    body += "  " + std::to_string(info.usesConsumed) + "/"
                            + std::to_string(info.maxUses) + " used";
                else if (info.type == LicenseType::Duration)
                    body += "  expires " + formatTime(info.expiryUnix);
                if (!info.discordUserId.empty())
                    body += "  <@" + info.discordUserId + ">";
                body += info.boundAccountId ? "  [bound]\n" : "  [unbound]\n";
            }

            dpp::embed embed;
            embed.set_title("Licences").set_color(0x1E88FF).set_description(body);
            event.reply(dpp::message().add_embed(embed).set_flags(dpp::m_ephemeral));
            return;
        }

        if (subName == "bind") {
            std::string keyId, user;
            for (const auto &opt : sub.options) {
                if (opt.name == "keyid" && std::holds_alternative<std::string>(opt.value))
                    keyId = std::get<std::string>(opt.value);
                if (opt.name == "user" && std::holds_alternative<dpp::snowflake>(opt.value))
                    user = std::to_string(std::get<dpp::snowflake>(opt.value));
            }
            std::string err;
            const bool ok = service->setDiscordUser(keyId, user, &err);
            event.reply(dpp::message(ok ? "Recorded <@" + user + "> for licence " + keyId
                                        : "No such licence: " + keyId)
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        // info / revoke both take a keyid.
        std::string keyId;
        for (const auto &opt : sub.options) {
            if (opt.name == "keyid" && std::holds_alternative<std::string>(opt.value))
                keyId = std::get<std::string>(opt.value);
        }

        if (subName == "revoke") {
            std::string err;
            const bool ok = service->revoke(keyId, &err);
            if (ok)
                logAudit(*db, actorOf(event), "Revoked licence (Discord /license revoke)", keyId);
            event.reply(dpp::message(ok ? "Revoked licence " + keyId
                                        : "No such licence: " + keyId)
                            .set_flags(dpp::m_ephemeral));
            return;
        }

        if (subName == "info") {
            LicenseInfo info = service->lookup(keyId);
            if (!info.exists) {
                event.reply(dpp::message("No licence with id " + keyId)
                                .set_flags(dpp::m_ephemeral));
                return;
            }
            dpp::embed embed;
            embed.set_title("Licence " + info.keyId)
                .set_color(info.revoked ? 0xE0354B : 0x35D08A)
                .add_field("Type", toString(info.type), true)
                .add_field("Status", info.revoked ? "REVOKED" : "active", true)
                .add_field("Issued", formatTime(info.issuedUnix), true);
            if (info.type == LicenseType::Duration)
                embed.add_field("Expires", formatTime(info.expiryUnix), true);
            if (info.type == LicenseType::Uses)
                embed.add_field("Uses", std::to_string(info.usesConsumed) + " / "
                                            + std::to_string(info.maxUses),
                                true);
            embed.add_field("Bound", info.boundAccountId
                                         ? ("account " + std::to_string(*info.boundAccountId))
                                         : std::string("unbound"),
                            true);
            event.reply(dpp::message().add_embed(embed).set_flags(dpp::m_ephemeral));
            return;
        }
    });

    // Heartbeat: the app's System Status panel reports the Discord Bot as online
    // when this timestamp is recent (see the /v1/admin/overview handler).
    bot.on_ready([&bot, db, dbMutex](const dpp::ready_t &) {
        auto beat = [db, dbMutex] {
            std::lock_guard<std::mutex> lock(*dbMutex);
            db->execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('bot_heartbeat', ?)",
                        {std::to_string(nowUnix())});
        };
        beat();
        bot.start_timer([beat](dpp::timer) { beat(); }, 30);
    });

    bot.start(dpp::st_wait);
    return 0;
}
