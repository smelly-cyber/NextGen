// ngtauthd - the NextGen Tweaks validation + account server.
//
// The desktop app never touches the licence database directly. It calls this
// server, which is the single online authority for account credentials, licence
// binding, use counting, expiry and revocation. The Discord bot writes licences
// into the same database (via libnglicense), so a key minted in Discord is
// immediately redeemable here - the two halves are literally the same core.
//
// Endpoints (all JSON):
//   GET  /v1/health                      -> { ok, service, publicKey }
//   POST /v1/account/create              { username, email, password, license, machine }
//   POST /v1/account/signin              { identifier, password, license?, machine }
//   POST /v1/session/validate            { token, machine }
//
// A successful create/signin returns a short, HMAC-signed session token the app
// stores and replays to /v1/session/validate, so credentials are sent only at
// sign-in time.
//
// Run behind a TLS-terminating reverse proxy (Caddy, Cloudflare Tunnel, nginx)
// in production; the app talks HTTPS to that. For local testing it serves plain
// HTTP on 127.0.0.1.
#include "nglicense/Crypto.h"
#include "nglicense/Database.h"
#include "nglicense/License.h"
#include "nglicense/LicenseService.h"

#include "httplib.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

using json = nlohmann::json;
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

/// Signs a session claim with an HMAC so the app cannot forge or edit it.
class SessionSigner
{
public:
    explicit SessionSigner(Bytes secret) : m_secret(std::move(secret)) {}

    std::string issue(std::int64_t accountId, const std::string &machine, std::int64_t ttlSeconds)
    {
        json claim{{"a", accountId}, {"m", machine}, {"exp", nowUnix() + ttlSeconds}};
        const std::string body = claim.dump();
        const std::string b64 = crypto::toBase64(Bytes(body.begin(), body.end()));
        const auto tag = crypto::hmacSha256(m_secret, Bytes(b64.begin(), b64.end()));
        return b64 + "." + crypto::toHex(Bytes(tag.begin(), tag.end()));
    }

    /// Returns the account id if the token is authentic and unexpired.
    std::optional<std::pair<std::int64_t, std::string>> verify(const std::string &token)
    {
        const auto dot = token.find('.');
        if (dot == std::string::npos)
            return std::nullopt;
        const std::string b64 = token.substr(0, dot);
        const std::string tagHex = token.substr(dot + 1);

        const auto expected = crypto::hmacSha256(m_secret, Bytes(b64.begin(), b64.end()));
        if (!crypto::constantTimeEquals(Bytes(expected.begin(), expected.end()),
                                        crypto::fromHex(tagHex)))
            return std::nullopt;

        try {
            const Bytes bodyBytes = crypto::fromBase64(b64);
            json claim = json::parse(std::string(bodyBytes.begin(), bodyBytes.end()));
            if (claim.value("exp", std::int64_t{0}) < nowUnix())
                return std::nullopt;
            return std::make_pair(claim.value("a", std::int64_t{0}),
                                  claim.value("m", std::string{}));
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    Bytes m_secret;
};

/// Reads a string field, treating an explicit JSON null (or a missing key) as
/// empty rather than throwing.
inline std::string strField(const json &j, const char *key)
{
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

json validationToJson(const ValidationResult &r)
{
    json out{{"ok", r.ok}, {"licensed", r.licensed}, {"reason", r.reason}};
    if (r.ok) {
        out["accountId"] = r.accountId;
        out["username"] = r.username;
    }
    if (r.licensed) {
        out["type"] = toString(r.type);
        if (r.expiryUnix > 0)
            out["expiryUnix"] = r.expiryUnix;
        if (r.usesRemaining >= 0)
            out["usesRemaining"] = r.usesRemaining;
    }
    return out;
}

} // namespace

int main()
{
    const std::string dataDir = envOr("NGT_DATA_DIR", "data");
    const std::string host = envOr("NGT_HOST", "127.0.0.1");
    const int port = std::stoi(envOr("NGT_PORT", "8787"));

    auto publicKey = load32(dataDir + "/issuer_public.bin");
    if (!publicKey) {
        std::cerr << "No issuer public key at " << dataDir
                  << "/issuer_public.bin. Run 'ngtlicense keygen' first.\n";
        return 1;
    }

    // The session-signing secret is separate from the issuer key and generated
    // on first run.
    const std::string secretPath = dataDir + "/session_secret.bin";
    Bytes sessionSecret = readFile(secretPath);
    if (sessionSecret.size() != 32) {
        sessionSecret = crypto::randomBytes(32);
        std::ofstream out(secretPath, std::ios::binary);
        out.write(reinterpret_cast<const char *>(sessionSecret.data()),
                  static_cast<std::streamsize>(sessionSecret.size()));
    }
    SessionSigner sessions(sessionSecret);

    Database db;
    std::string error;
    if (!db.open(dataDir + "/licenses.db", &error)) {
        std::cerr << "Database error: " << error << "\n";
        return 1;
    }

    LicenseService service(db, *publicKey);

    // The admin panel can mint licences, which needs the secret issuer seed -
    // the same one the CLI and bot use. It is optional: without it, everything
    // except "Add License" still works.
    if (auto seed = load32(dataDir + "/issuer_key.bin"))
        service.setIssuerSeed(*seed);

    std::mutex dbMutex; // libnglicense/SQLite access is serialised.

    // --- Admin / owner migrations -------------------------------------------
    // Added on top of the shared schema. execute() failures are ignored on
    // purpose: "duplicate column" just means the migration already ran.
    // Database::execute() prepares a single statement, so each table gets its
    // own call rather than one multi-statement string.
    db.execute("ALTER TABLE accounts ADD COLUMN is_owner INTEGER NOT NULL DEFAULT 0");
    db.execute("CREATE TABLE IF NOT EXISTS activity ("
               "  id INTEGER PRIMARY KEY AUTOINCREMENT, account_id INTEGER, username TEXT,"
               "  kind TEXT NOT NULL, detail TEXT, created_unix INTEGER NOT NULL)");
    db.execute("CREATE TABLE IF NOT EXISTS audit ("
               "  id INTEGER PRIMARY KEY AUTOINCREMENT, actor TEXT, action TEXT NOT NULL,"
               "  detail TEXT, created_unix INTEGER NOT NULL)");
    db.execute("CREATE TABLE IF NOT EXISTS client_commands ("
               "  id INTEGER PRIMARY KEY AUTOINCREMENT, target TEXT NOT NULL, kind TEXT NOT NULL,"
               "  payload TEXT, created_by TEXT, created_unix INTEGER NOT NULL)");
    db.execute("CREATE TABLE IF NOT EXISTS command_receipts ("
               "  command_id INTEGER NOT NULL, account_id INTEGER NOT NULL,"
               "  done_unix INTEGER NOT NULL, PRIMARY KEY (command_id, account_id))");
    db.execute("CREATE TABLE IF NOT EXISTS client_state ("
               "  account_id INTEGER PRIMARY KEY, disabled INTEGER NOT NULL DEFAULT 0)");
    db.execute("ALTER TABLE client_state ADD COLUMN nuked INTEGER NOT NULL DEFAULT 0");
    db.execute("CREATE TABLE IF NOT EXISTS presence ("
               "  account_id INTEGER PRIMARY KEY, username TEXT, machine TEXT,"
               "  last_seen_unix INTEGER NOT NULL)");

    // The owner licence: one memorable key that grants the admin panel. It is
    // stored server-side (never shipped in the app) so it cannot be forged by
    // editing the client. Seeded once; change it any time with:
    //   UPDATE meta SET value='...' WHERE key='owner_license';
    std::string ownerKey;
    db.query("SELECT value FROM meta WHERE key='owner_license'", {},
             [&](const Row &row) { ownerKey = row.text(0); });
    if (ownerKey.empty()) {
        ownerKey = "NGT-OWNER-ACCESS";
        db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('owner_license', ?)",
                   {ownerKey});
    }

    // All helpers below assume the caller already holds dbMutex.
    auto isOwnerAccount = [&](std::int64_t accountId) {
        bool owner = false;
        db.query("SELECT is_owner FROM accounts WHERE id=?", {accountId},
                 [&](const Row &row) { owner = row.integer(0) != 0; });
        return owner;
    };
    auto markOwner = [&](std::int64_t accountId) {
        db.execute("UPDATE accounts SET is_owner=1 WHERE id=?", {accountId});
    };
    auto isDisabled = [&](std::int64_t accountId) {
        bool disabled = false;
        db.query("SELECT disabled FROM client_state WHERE account_id=?", {accountId},
                 [&](const Row &row) { disabled = row.integer(0) != 0; });
        return disabled;
    };
    auto isNuked = [&](std::int64_t accountId) {
        bool nuked = false;
        db.query("SELECT nuked FROM client_state WHERE account_id=?", {accountId},
                 [&](const Row &row) { nuked = row.integer(0) != 0; });
        return nuked;
    };
    auto logActivity = [&](std::int64_t accountId, const std::string &username,
                           const std::string &kind, const std::string &detail) {
        db.execute("INSERT INTO activity (account_id, username, kind, detail, created_unix)"
                   " VALUES (?,?,?,?,?)",
                   {accountId, username, kind, detail, nowUnix()});
    };
    auto logAudit = [&](const std::string &actor, const std::string &action,
                        const std::string &detail) {
        db.execute("INSERT INTO audit (actor, action, detail, created_unix) VALUES (?,?,?,?)",
                   {actor, action, detail, nowUnix()});
    };
    auto touchPresence = [&](std::int64_t accountId, const std::string &username,
                             const std::string &machine) {
        db.execute("INSERT INTO presence (account_id, username, machine, last_seen_unix)"
                   " VALUES (?,?,?,?) ON CONFLICT(account_id) DO UPDATE SET"
                   " username=excluded.username, machine=excluded.machine,"
                   " last_seen_unix=excluded.last_seen_unix",
                   {accountId, username, machine, nowUnix()});
    };
    // Turns a completed licence sign-in into the owner grant when the owner key
    // was presented. Mutates the response JSON in place.
    auto applyOwnerGrant = [&](json &out, const std::string &presentedLicense) {
        if (!out.value("ok", false) || presentedLicense != ownerKey)
            return;
        const std::int64_t accountId = out.value("accountId", std::int64_t{0});
        markOwner(accountId);
        out["licensed"] = true;
        out["type"] = "lifetime";
        out["owner"] = true;
        out["reason"] = "Owner access granted.";
    };
    // Verifies an admin request: valid token + the account is the owner.
    auto ownerFromToken = [&](const std::string &token) -> std::optional<std::int64_t> {
        auto claim = sessions.verify(token);
        if (!claim || !isOwnerAccount(claim->first))
            return std::nullopt;
        return claim->first;
    };
    auto usernameOf = [&](std::int64_t accountId) {
        std::string name;
        db.query("SELECT username FROM accounts WHERE id=?", {accountId},
                 [&](const Row &row) { name = row.text(0); });
        return name;
    };

    httplib::Server server;

    server.set_default_headers({{"Access-Control-Allow-Origin", "*"}});

    server.Get("/v1/health", [&](const httplib::Request &, httplib::Response &res) {
        json body{{"ok", true},
                  {"service", "ngtauthd"},
                  {"publicKey", crypto::toHex(Bytes(publicKey->begin(), publicKey->end()))}};
        res.set_content(body.dump(), "application/json");
    });

    server.Post("/v1/account/create", [&](const httplib::Request &req, httplib::Response &res) {
        json out;
        try {
            json in = json::parse(req.body);
            std::lock_guard<std::mutex> lock(dbMutex);
            const std::string license = in.value("license", "");
            const std::string machine = in.value("machine", "");
            // The owner key is not an Ed25519 licence, so create the account
            // unlicensed and let applyOwnerGrant below turn on owner access.
            ValidationResult r = service.createAccount(
                in.value("username", ""), in.value("email", ""), in.value("password", ""),
                license == ownerKey ? std::string() : license, machine);
            out = validationToJson(r);
            applyOwnerGrant(out, license);
            if (r.ok) {
                out["token"] = sessions.issue(r.accountId, machine, 7 * 86400);
                touchPresence(r.accountId, r.username, machine);
                logActivity(r.accountId, r.username, "account", "Created account");
            }
        } catch (const std::exception &ex) {
            out = {{"ok", false}, {"reason", std::string("Bad request: ") + ex.what()}};
        }
        res.set_content(out.dump(), "application/json");
    });

    server.Post("/v1/account/signin", [&](const httplib::Request &req, httplib::Response &res) {
        json out;
        try {
            json in = json::parse(req.body);
            std::lock_guard<std::mutex> lock(dbMutex);
            const std::string license = in.value("license", "");
            const std::string machine = in.value("machine", "");
            // A use is consumed at sign-in for use-limited licences.
            ValidationResult r = service.signIn(in.value("identifier", ""),
                                                in.value("password", ""),
                                                license == ownerKey ? std::string() : license,
                                                machine, /*consumeUse=*/true);
            out = validationToJson(r);
            applyOwnerGrant(out, license);
            // An owner is always the owner, whichever key they typed this time.
            if (r.ok && isOwnerAccount(r.accountId)) {
                out["licensed"] = true;
                out["owner"] = true;
                if (!out.contains("type"))
                    out["type"] = "lifetime";
            }
            if (r.ok) {
                // A disabled client is refused at the door.
                if (isDisabled(r.accountId) && !out.value("owner", false)) {
                    out = {{"ok", false},
                           {"reason", "This client has been disabled by an administrator."}};
                } else {
                    out["token"] = sessions.issue(r.accountId, machine, 7 * 86400);
                    touchPresence(r.accountId, r.username, machine);
                    logActivity(r.accountId, r.username, "signin", "Logged in");
                }
            }
        } catch (const std::exception &ex) {
            out = {{"ok", false}, {"reason", std::string("Bad request: ") + ex.what()}};
        }
        res.set_content(out.dump(), "application/json");
    });

    server.Post("/v1/session/validate", [&](const httplib::Request &req, httplib::Response &res) {
        json out;
        try {
            json in = json::parse(req.body);
            auto claim = sessions.verify(strField(in, "token"));
            if (!claim) {
                out = {{"ok", false}, {"reason", "Session expired. Please sign in again."}};
            } else {
                std::lock_guard<std::mutex> lock(dbMutex);
                // A refresh does not consume a use; it only checks the licence
                // is still live (not revoked / expired).
                ValidationResult r = service.validateForAccount(claim->first, claim->second,
                                                                /*consumeUse=*/false);
                out = validationToJson(r);
                if (r.ok && isOwnerAccount(claim->first)) {
                    out["licensed"] = true;
                    out["owner"] = true;
                    if (!out.contains("type"))
                        out["type"] = "lifetime";
                }
                if (r.ok)
                    touchPresence(claim->first, r.username, claim->second);
            }
        } catch (const std::exception &ex) {
            out = {{"ok", false}, {"reason", std::string("Bad request: ") + ex.what()}};
        }
        res.set_content(out.dump(), "application/json");
    });

    // The in-app activation prompt: an authenticated (token-holding) but
    // unlicensed user redeems a key to unlock the features.
    server.Post("/v1/license/redeem", [&](const httplib::Request &req, httplib::Response &res) {
        json out;
        try {
            json in = json::parse(req.body);
            auto claim = sessions.verify(strField(in, "token"));
            if (!claim) {
                out = {{"ok", false}, {"reason", "Session expired. Please sign in again."}};
            } else {
                std::lock_guard<std::mutex> lock(dbMutex);
                const std::string license = in.value("license", "");
                ValidationResult r;
                if (license == ownerKey) {
                    // Owner key redeemed from the in-app activation prompt.
                    r.ok = true;
                    r.accountId = claim->first;
                    r.username = usernameOf(claim->first);
                    out = validationToJson(r);
                    applyOwnerGrant(out, license);
                    logActivity(claim->first, r.username, "owner", "Activated owner access");
                } else {
                    r = service.redeemLicense(claim->first, license, claim->second);
                    out = validationToJson(r);
                    if (r.ok && isOwnerAccount(claim->first)) {
                        out["licensed"] = true;
                        out["owner"] = true;
                    }
                }
                // Refresh the token so the entitlement summary stays current.
                if (r.ok)
                    out["token"] = sessions.issue(r.accountId, claim->second, 7 * 86400);
            }
        } catch (const std::exception &ex) {
            out = {{"ok", false}, {"reason", std::string("Bad request: ") + ex.what()}};
        }
        res.set_content(out.dump(), "application/json");
    });

    // ========================================================================
    //  Owner-only admin API. Every endpoint requires a session token whose
    //  account has is_owner = 1; anyone else gets 403.
    // ========================================================================
    const std::string appVersion = "1.0.0";
    constexpr std::int64_t kOnlineWindow = 300; // 5 minutes.

    // Masked, display-friendly view of one licence row.
    auto licenseRowJson = [&](const Row &row) {
        // key_id, type, expiry_unix, status, bound_account_id, bound_machine, notes
        const std::string keyId = row.text(0);
        std::string masked = "NG-XXXX-XXXX";
        if (keyId.size() >= 4)
            masked = "NG-XXXX-" + keyId.substr(0, 4);
        std::string user = "-";
        if (!row.isNull(4)) {
            const std::int64_t acc = row.integer(4);
            user = usernameOf(acc);
            if (user.empty())
                user = "acct#" + std::to_string(acc);
        }
        // The bound machine is a 24-hex-char hash; show a short, readable slice
        // in upper case as the HWID. An unbound key has none yet.
        std::string boundMachine = row.text(5);
        std::string hwid;
        if (boundMachine.size() >= 8) {
            hwid = boundMachine.substr(0, 8);
            for (char &ch : hwid)
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        const std::int64_t expiry = row.integer(2);
        std::string status = "Active";
        if (row.integer(3) != 0)
            status = "Revoked";
        else if (expiry > 0 && expiry < nowUnix())
            status = "Expired";
        return json{{"keyId", keyId},
                    {"key", masked},
                    {"user", user},
                    {"hwid", hwid.empty() ? std::string("Not activated") : hwid},
                    {"bound", !hwid.empty()},
                    {"status", status},
                    {"type", toString(static_cast<LicenseType>(row.integer(1)))}};
    };

    // Rejects the request unless the token belongs to the owner. Returns the
    // owner account id on success.
    auto guard = [&](const httplib::Request &req, httplib::Response &res,
                     json &in) -> std::optional<std::int64_t> {
        try {
            in = json::parse(req.body);
        } catch (...) {
            res.status = 400;
            res.set_content("{\"ok\":false,\"reason\":\"Bad request\"}", "application/json");
            return std::nullopt;
        }
        auto owner = ownerFromToken(strField(in, "token"));
        if (!owner) {
            res.status = 403;
            res.set_content("{\"ok\":false,\"reason\":\"Owner access required.\"}",
                            "application/json");
            return std::nullopt;
        }
        return owner;
    };

    server.Post("/v1/admin/overview", [&](const httplib::Request &req, httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;

        json out{{"ok", true}};

        std::int64_t totalUsers = 0, activeLicenses = 0, onlineNow = 0;
        db.query("SELECT COUNT(*) FROM accounts", {},
                 [&](const Row &r) { totalUsers = r.integer(0); });
        db.query("SELECT COUNT(*) FROM licenses l LEFT JOIN accounts a ON a.id=l.bound_account_id"
                 " WHERE l.status=0 AND COALESCE(a.is_owner,0)=0", {},
                 [&](const Row &r) { activeLicenses = r.integer(0); });
        db.query("SELECT COUNT(*) FROM presence WHERE last_seen_unix > ?",
                 {nowUnix() - kOnlineWindow}, [&](const Row &r) { onlineNow = r.integer(0); });

        out["stats"] = {{"totalUsers", totalUsers},
                        {"activeLicenses", activeLicenses},
                        {"onlineNow", onlineNow},
                        {"appVersion", appVersion}};

        // Backend service health. This process serves auth, licence and update
        // checks, so those are Online whenever it answers; the database is
        // checked directly; the Discord bot reports a heartbeat into meta.
        std::int64_t botBeat = 0;
        db.query("SELECT value FROM meta WHERE key='bot_heartbeat'", {},
                 [&](const Row &r) { botBeat = std::stoll(r.text(0).empty() ? "0" : r.text(0)); });
        out["services"] = json::array(
            {{{"name", "Authentication Server"}, {"online", true}},
             {{"name", "License Server"}, {"online", true}},
             {{"name", "Update Server"}, {"online", true}},
             {{"name", "Database"}, {"online", db.isOpen()}},
             {{"name", "Discord Bot"}, {"online", botBeat > nowUnix() - 120}}});

        json activity = json::array();
        db.query("SELECT username, kind, detail, created_unix FROM activity"
                 " ORDER BY id DESC LIMIT 8",
                 {}, [&](const Row &r) {
                     activity.push_back({{"username", r.text(0)},
                                         {"kind", r.text(1)},
                                         {"detail", r.text(2)},
                                         {"unix", r.integer(3)}});
                 });
        out["activity"] = activity;

        json licenses = json::array();
        db.query("SELECT l.key_id, l.type, l.expiry_unix, l.status, l.bound_account_id,"
                 " l.bound_machine, l.notes FROM licenses l"
                 " LEFT JOIN accounts a ON a.id=l.bound_account_id"
                 " WHERE COALESCE(a.is_owner,0)=0 ORDER BY l.created_unix DESC LIMIT 5",
                 {}, [&](const Row &r) { licenses.push_back(licenseRowJson(r)); });
        out["licenses"] = licenses;

        std::string uv, un;
        db.query("SELECT value FROM meta WHERE key='update_version'", {},
                 [&](const Row &r) { uv = r.text(0); });
        db.query("SELECT value FROM meta WHERE key='update_notes'", {},
                 [&](const Row &r) { un = r.text(0); });
        std::string uurl;
        db.query("SELECT value FROM meta WHERE key='update_url'", {},
                 [&](const Row &r) { uurl = r.text(0); });
        out["update"] = {{"version", uv.empty() ? appVersion : uv}, {"notes", un}, {"url", uurl}};

        std::string nukeAll = "0";
        db.query("SELECT value FROM meta WHERE key='nuke_all'", {},
                 [&](const Row &r) { nukeAll = r.text(0); });
        out["nukeActive"] = nukeAll == "1";

        res.set_content(out.dump(), "application/json");
    });

    server.Post("/v1/admin/licenses", [&](const httplib::Request &req, httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;

        const std::string queryText = in.value("query", "");
        const std::string statusFilter = in.value("status", "");
        const int page = std::max(1, in.value("page", 1));
        const int pageSize = 5;

        // Owner licences are never listed - the owner's own access is not
        // something to manage in this table.
        std::string where = "WHERE COALESCE(a.is_owner, 0) = 0";
        std::vector<SqlValue> params;
        if (!queryText.empty()) {
            where += " AND (l.key_id LIKE ? OR a.username LIKE ?)";
            params.push_back("%" + queryText + "%");
            params.push_back("%" + queryText + "%");
        }
        if (statusFilter == "active")
            where += " AND l.status=0 AND (l.expiry_unix=0 OR l.expiry_unix>" + std::to_string(nowUnix()) + ")";
        else if (statusFilter == "revoked")
            where += " AND l.status=1";
        else if (statusFilter == "expired")
            where += " AND l.status=0 AND l.expiry_unix>0 AND l.expiry_unix<" + std::to_string(nowUnix());

        std::int64_t total = 0;
        db.query("SELECT COUNT(*) FROM licenses l LEFT JOIN accounts a ON a.id=l.bound_account_id "
                     + where,
                 params, [&](const Row &r) { total = r.integer(0); });

        auto pageParams = params;
        pageParams.push_back(pageSize);
        pageParams.push_back((page - 1) * pageSize);
        json licenses = json::array();
        db.query("SELECT l.key_id, l.type, l.expiry_unix, l.status, l.bound_account_id,"
                 " l.bound_machine, l.notes FROM licenses l"
                 " LEFT JOIN accounts a ON a.id=l.bound_account_id " + where
                     + " ORDER BY l.created_unix DESC LIMIT ? OFFSET ?",
                 pageParams, [&](const Row &r) { licenses.push_back(licenseRowJson(r)); });

        res.set_content(json{{"ok", true},
                             {"licenses", licenses},
                             {"total", total},
                             {"page", page},
                             {"pageSize", pageSize}}
                            .dump(),
                        "application/json");
    });

    server.Post("/v1/admin/license/create", [&](const httplib::Request &req,
                                                 httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;
        if (!service.canIssue()) {
            res.set_content("{\"ok\":false,\"reason\":\"Server has no issuer key to mint licences.\"}",
                            "application/json");
            return;
        }

        IssueSpec spec;
        const std::string kind = in.value("kind", "lifetime");
        spec.issuedBy = "admin:" + usernameOf(*owner);
        spec.notes = in.value("note", "");
        if (kind == "duration") {
            spec.type = LicenseType::Duration;
            spec.durationSeconds = in.value("durationDays", 30) * 86400;
        } else if (kind == "uses") {
            spec.type = LicenseType::Uses;
            spec.maxUses = static_cast<std::uint32_t>(in.value("uses", 25));
        } else {
            spec.type = LicenseType::Lifetime;
        }
        IssueResult r = service.issue(spec);
        json out{{"ok", r.ok}, {"reason", r.message}};
        if (r.ok) {
            out["key"] = r.keyString;
            logAudit(usernameOf(*owner), "Created licence", kind);
        }
        res.set_content(out.dump(), "application/json");
    });

    server.Post("/v1/admin/license/revoke", [&](const httplib::Request &req,
                                                 httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;
        int revoked = 0;
        for (const auto &id : in.value("keyIds", json::array())) {
            const std::string keyId = id.get<std::string>();
            // An owner-bound licence is never revocable from the panel.
            bool ownerBound = false;
            db.query("SELECT COALESCE(a.is_owner,0) FROM licenses l"
                     " LEFT JOIN accounts a ON a.id=l.bound_account_id WHERE l.key_id=?",
                     {keyId}, [&](const Row &r) { ownerBound = r.integer(0) != 0; });
            if (ownerBound)
                continue;
            if (service.revoke(keyId))
                ++revoked;
        }
        logAudit(usernameOf(*owner), "Revoked licences", std::to_string(revoked));
        res.set_content(json{{"ok", true}, {"revoked", revoked}}.dump(), "application/json");
    });

    server.Post("/v1/admin/clear-licenses", [&](const httplib::Request &req,
                                                 httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;
        db.execute("DELETE FROM licenses WHERE key_id NOT IN"
                   " (SELECT l.key_id FROM licenses l JOIN accounts a"
                   "  ON a.id=l.bound_account_id WHERE a.is_owner=1)");
        db.execute("DELETE FROM activations WHERE key_id NOT IN (SELECT key_id FROM licenses)");
        db.execute("UPDATE accounts SET license_key_id=NULL WHERE is_owner=0");
        logAudit(usernameOf(*owner), "Cleared all licences", "");
        res.set_content("{\"ok\":true}", "application/json");
    });

    server.Post("/v1/admin/command", [&](const httplib::Request &req, httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;
        const std::string kind = in.value("kind", "");
        const std::string target = in.value("target", "all"); // "all" or a username
        const std::string payload = in.value("message", "");
        if (kind != "nuke" && kind != "restore" && kind != "disable" && kind != "reset"
            && kind != "enable" && kind != "message") {
            res.set_content("{\"ok\":false,\"reason\":\"Unknown command.\"}", "application/json");
            return;
        }
        db.execute("INSERT INTO client_commands (target, kind, payload, created_by, created_unix)"
                   " VALUES (?,?,?,?,?)",
                   {target, kind, payload, usernameOf(*owner), nowUnix()});

        // disable / enable also flips persistent client state so a signed-out
        // user stays blocked (or unblocked) next time they sign in.
        if (kind == "disable" || kind == "enable") {
            const int disabled = kind == "disable" ? 1 : 0;
            if (target == "all") {
                db.execute("INSERT INTO client_state (account_id, disabled)"
                           " SELECT id, ? FROM accounts WHERE is_owner=0"
                           " ON CONFLICT(account_id) DO UPDATE SET disabled=excluded.disabled",
                           {disabled});
            } else {
                db.execute("INSERT INTO client_state (account_id, disabled)"
                           " SELECT id, ? FROM accounts WHERE username=? AND is_owner=0"
                           " ON CONFLICT(account_id) DO UPDATE SET disabled=excluded.disabled",
                           {disabled, target});
            }
        }

        // nuke / restore is the reversible kill switch: a nuked client refuses to
        // run at all (it exits before any window). Restore lifts it. Owner
        // accounts are never nuked. A meta flag records whether an "all" nuke is
        // in effect so the admin panel can show a green "Restore Client" button.
        if (kind == "nuke" || kind == "restore") {
            const int nuked = kind == "nuke" ? 1 : 0;
            if (target == "all") {
                db.execute("INSERT INTO client_state (account_id, nuked)"
                           " SELECT id, ? FROM accounts WHERE is_owner=0"
                           " ON CONFLICT(account_id) DO UPDATE SET nuked=excluded.nuked",
                           {nuked});
                db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('nuke_all', ?)",
                           {std::to_string(nuked)});
            } else {
                db.execute("INSERT INTO client_state (account_id, nuked)"
                           " SELECT id, ? FROM accounts WHERE username=? AND is_owner=0"
                           " ON CONFLICT(account_id) DO UPDATE SET nuked=excluded.nuked",
                           {nuked, target});
            }
            // A restore also lifts any disable, so the client is fully usable.
            if (kind == "restore") {
                if (target == "all")
                    db.execute("UPDATE client_state SET disabled=0");
                else
                    db.execute("UPDATE client_state SET disabled=0 WHERE account_id IN"
                               " (SELECT id FROM accounts WHERE username=?)", {target});
            }
        }
        logAudit(usernameOf(*owner), "Client command: " + kind, target);
        res.set_content("{\"ok\":true}", "application/json");
    });

    server.Post("/v1/admin/push-update", [&](const httplib::Request &req, httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;
        db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('update_version', ?)",
                   {in.value("version", appVersion)});
        db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('update_notes', ?)",
                   {in.value("notes", "")});
        db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('update_target', ?)",
                   {in.value("target", "all")});
        // The download URL + checksum drive the client auto-updater. Both are
        // optional: with no URL the client just shows the "update available"
        // banner instead of installing silently.
        db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('update_url', ?)",
                   {in.value("url", "")});
        db.execute("INSERT OR REPLACE INTO meta (key, value) VALUES ('update_sha256', ?)",
                   {in.value("sha256", "")});
        // No extra "message" command here: the update itself is delivered to
        // clients through the update field of /v1/client/poll, and queueing a
        // message as well made every client show two popups for one update.
        logAudit(usernameOf(*owner), "Pushed update", in.value("version", appVersion));
        res.set_content("{\"ok\":true}", "application/json");
    });

    server.Post("/v1/admin/audit", [&](const httplib::Request &req, httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;
        json entries = json::array();
        db.query("SELECT actor, action, detail, created_unix FROM audit ORDER BY id DESC LIMIT 100",
                 {}, [&](const Row &r) {
                     entries.push_back({{"actor", r.text(0)},
                                        {"action", r.text(1)},
                                        {"detail", r.text(2)},
                                        {"unix", r.integer(3)}});
                 });
        res.set_content(json{{"ok", true}, {"entries", entries}}.dump(), "application/json");
    });

    server.Post("/v1/admin/export", [&](const httplib::Request &req, httplib::Response &res) {
        json in;
        std::lock_guard<std::mutex> lock(dbMutex);
        auto owner = guard(req, res, in);
        if (!owner)
            return;
        json users = json::array();
        db.query("SELECT a.id, a.username, a.email, a.license_key_id, a.created_unix,"
                 " p.last_seen_unix, s.disabled"
                 " FROM accounts a"
                 " LEFT JOIN presence p ON p.account_id=a.id"
                 " LEFT JOIN client_state s ON s.account_id=a.id ORDER BY a.id",
                 {}, [&](const Row &r) {
                     users.push_back({{"id", r.integer(0)},
                                      {"username", r.text(1)},
                                      {"email", r.text(2)},
                                      {"license", r.isNull(3) ? "" : r.text(3)},
                                      {"createdUnix", r.integer(4)},
                                      {"lastSeenUnix", r.isNull(5) ? 0 : r.integer(5)},
                                      {"disabled", r.isNull(6) ? 0 : r.integer(6)}});
                 });
        logAudit(usernameOf(*owner), "Exported user data", "");
        res.set_content(json{{"ok", true}, {"users", users}}.dump(), "application/json");
    });

    // ---- Client polling: how a running app receives admin commands ----------
    server.Post("/v1/client/poll", [&](const httplib::Request &req, httplib::Response &res) {
        json out{{"ok", false}};
        try {
            json in = json::parse(req.body);
            auto claim = sessions.verify(strField(in, "token"));
            if (!claim) {
                res.set_content(out.dump(), "application/json");
                return;
            }
            const std::int64_t accountId = claim->first;
            std::lock_guard<std::mutex> lock(dbMutex);

            const std::string username = usernameOf(accountId);
            touchPresence(accountId, username, claim->second);

            // Acknowledge commands the client says it has carried out.
            for (const auto &id : in.value("ack", json::array())) {
                db.execute("INSERT OR IGNORE INTO command_receipts"
                           " (command_id, account_id, done_unix) VALUES (?,?,?)",
                           {id.get<std::int64_t>(), accountId, nowUnix()});
            }

            const bool owner = isOwnerAccount(accountId);
            out["ok"] = true;
            out["disabled"] = owner ? false : isDisabled(accountId);
            out["nuked"] = owner ? false : isNuked(accountId);

            // Pending commands: targeted at everyone or this user, not yet acked.
            // The owner is never a target of destructive commands - only
            // messages reach the owner, so an owner's own running client can
            // never nuke, disable or reset itself.
            json commands = json::array();
            db.query("SELECT c.id, c.kind, c.payload FROM client_commands c"
                     " WHERE (c.target='all' OR c.target=?)"
                     " AND NOT EXISTS (SELECT 1 FROM command_receipts r"
                     "   WHERE r.command_id=c.id AND r.account_id=?)"
                     " ORDER BY c.id ASC LIMIT 20",
                     {username, accountId}, [&](const Row &r) {
                         const std::string kind = r.text(1);
                         if (owner && kind != "message")
                             return; // Owner is immune to nuke/disable/reset.
                         commands.push_back({{"id", r.integer(0)},
                                             {"kind", kind},
                                             {"payload", r.text(2)}});
                     });
            out["commands"] = commands;

            std::string uv, un;
            db.query("SELECT value FROM meta WHERE key='update_version'", {},
                     [&](const Row &r) { uv = r.text(0); });
            if (!uv.empty()) {
                db.query("SELECT value FROM meta WHERE key='update_notes'", {},
                         [&](const Row &r) { un = r.text(0); });
                std::string uurl, usha;
                db.query("SELECT value FROM meta WHERE key='update_url'", {},
                         [&](const Row &r) { uurl = r.text(0); });
                db.query("SELECT value FROM meta WHERE key='update_sha256'", {},
                         [&](const Row &r) { usha = r.text(0); });
                out["update"] = {{"version", uv}, {"notes", un}, {"url", uurl}, {"sha256", usha}};
            }
        } catch (const std::exception &ex) {
            out = {{"ok", false}, {"reason", std::string("Bad request: ") + ex.what()}};
        }
        res.set_content(out.dump(), "application/json");
    });

    server.Post("/v1/client/nukestate", [&](const httplib::Request &req, httplib::Response &res) {
        json out{{"ok", true}, {"nuked", true}}; // Fail closed: unknown => stay nuked.
        try {
            json in = json::parse(req.body);
            auto claim = sessions.verify(strField(in, "token"));
            if (claim) {
                std::lock_guard<std::mutex> lock(dbMutex);
                out["nuked"] = isOwnerAccount(claim->first) ? false : isNuked(claim->first);
            }
        } catch (...) {
        }
        res.set_content(out.dump(), "application/json");
    });

    server.Post("/v1/client/activity", [&](const httplib::Request &req, httplib::Response &res) {
        try {
            json in = json::parse(req.body);
            auto claim = sessions.verify(strField(in, "token"));
            if (claim) {
                std::lock_guard<std::mutex> lock(dbMutex);
                logActivity(claim->first, usernameOf(claim->first), in.value("kind", "activity"),
                            in.value("detail", ""));
            }
        } catch (...) {
        }
        res.set_content("{\"ok\":true}", "application/json");
    });

    std::cout << "ngtauthd listening on http://" << host << ":" << port << "\n"
              << "  data dir: " << dataDir << "\n"
              << "  (put a TLS proxy in front of this for production)\n";

    if (!server.listen(host, port)) {
        std::cerr << "Could not bind " << host << ":" << port << "\n";
        return 1;
    }
    return 0;
}
