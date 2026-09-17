#include "nglicense/LicenseService.h"

#include "nglicense/License.h"

#include <cctype>
#include <cstdlib>
#include <ctime>

namespace ngl {

std::int64_t nowUnix()
{
    return static_cast<std::int64_t>(std::time(nullptr));
}

std::optional<std::int64_t> parseDuration(const std::string &text)
{
    if (text.empty())
        return std::nullopt;

    std::size_t i = 0;
    std::int64_t value = 0;
    bool sawDigit = false;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        value = value * 10 + (text[i] - '0');
        sawDigit = true;
        ++i;
    }
    if (!sawDigit)
        return std::nullopt;

    if (i >= text.size())
        return value; // Bare number means seconds.

    const char unit = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    switch (unit) {
    case 's':
        return value;
    case 'm':
        return value * 60;
    case 'h':
        return value * 3600;
    case 'd':
        return value * 86400;
    case 'w':
        return value * 604800;
    case 'y':
        return value * 31536000;
    default:
        return std::nullopt;
    }
}

LicenseService::LicenseService(Database &db, std::array<std::uint8_t, 32> publicKey)
    : m_db(db)
    , m_publicKey(publicKey)
{
}

void LicenseService::setIssuerSeed(const std::array<std::uint8_t, 32> &seed)
{
    m_issuerSeed = seed;
    m_hasSeed = true;
}

IssueResult LicenseService::issue(const IssueSpec &spec)
{
    IssueResult result;
    if (!m_hasSeed) {
        result.message = "This component cannot mint licences (no issuer seed loaded).";
        return result;
    }

    LicensePayload payload;
    payload.type = spec.type;
    const Bytes id = crypto::randomBytes(payload.keyId.size());
    std::copy(id.begin(), id.end(), payload.keyId.begin());
    payload.issuedUnix = nowUnix();

    if (spec.type == LicenseType::Duration) {
        if (spec.durationSeconds <= 0) {
            result.message = "A duration licence needs a positive duration.";
            return result;
        }
        payload.expiryUnix = payload.issuedUnix + spec.durationSeconds;
    } else if (spec.type == LicenseType::Uses) {
        if (spec.maxUses == 0) {
            result.message = "A use-limited licence needs a positive use count.";
            return result;
        }
        payload.maxUses = spec.maxUses;
    }

    const std::string keyString = license::issue(m_issuerSeed, payload);
    const std::string keyId = payload.keyIdHex();

    std::string error;
    const bool inserted = m_db.execute(
        "INSERT INTO licenses (key_id, type, issued_unix, expiry_unix, max_uses, uses_consumed,"
        " status, issued_by, discord_user_id, notes, key_string, created_unix)"
        " VALUES (?,?,?,?,?,0,0,?,?,?,?,?)",
        {keyId, static_cast<int>(spec.type), payload.issuedUnix, payload.expiryUnix,
         static_cast<std::int64_t>(payload.maxUses), spec.issuedBy, spec.discordUserId,
         spec.notes, keyString, payload.issuedUnix},
        &error);

    if (!inserted) {
        result.message = "Database insert failed: " + error;
        return result;
    }

    result.ok = true;
    result.keyString = keyString;
    result.keyId = keyId;
    result.message = "Licence issued.";
    return result;
}

bool LicenseService::revoke(const std::string &keyId, std::string *error)
{
    if (!m_db.execute("UPDATE licenses SET status=1 WHERE key_id=?", {keyId}, error))
        return false;
    return m_db.changes() > 0;
}

LicenseInfo LicenseService::lookup(const std::string &keyId)
{
    LicenseInfo info;
    m_db.query(
        "SELECT key_id, type, issued_unix, expiry_unix, max_uses, uses_consumed, status,"
        " discord_user_id, notes, bound_account_id, bound_machine FROM licenses WHERE key_id=?",
        {keyId},
        [&](const Row &row) {
            info.exists = true;
            info.keyId = row.text(0);
            info.type = static_cast<LicenseType>(row.integer(1));
            info.issuedUnix = row.integer(2);
            info.expiryUnix = row.integer(3);
            info.maxUses = static_cast<std::uint32_t>(row.integer(4));
            info.usesConsumed = static_cast<std::uint32_t>(row.integer(5));
            info.revoked = row.integer(6) != 0;
            info.discordUserId = row.text(7);
            info.notes = row.text(8);
            if (!row.isNull(9))
                info.boundAccountId = row.integer(9);
            info.boundMachine = row.text(10);
        });
    return info;
}

std::vector<LicenseInfo> LicenseService::listLicenses(const std::string &discordUserId,
                                                  int limit)
{
    std::vector<LicenseInfo> out;
    const std::string sql =
        "SELECT key_id, type, issued_unix, expiry_unix, max_uses, uses_consumed, status,"
        " discord_user_id, notes, bound_account_id, bound_machine FROM licenses"
        + std::string(discordUserId.empty() ? "" : " WHERE discord_user_id=?")
        + " ORDER BY created_unix DESC LIMIT ?";

    std::vector<SqlValue> params;
    if (!discordUserId.empty())
        params.emplace_back(discordUserId);
    params.emplace_back(limit);

    m_db.query(sql, params, [&](const Row &row) {
        LicenseInfo info;
        info.exists = true;
        info.keyId = row.text(0);
        info.type = static_cast<LicenseType>(row.integer(1));
        info.issuedUnix = row.integer(2);
        info.expiryUnix = row.integer(3);
        info.maxUses = static_cast<std::uint32_t>(row.integer(4));
        info.usesConsumed = static_cast<std::uint32_t>(row.integer(5));
        info.revoked = row.integer(6) != 0;
        info.discordUserId = row.text(7);
        info.notes = row.text(8);
        if (!row.isNull(9))
            info.boundAccountId = row.integer(9);
        info.boundMachine = row.text(10);
        out.push_back(info);
    });
    return out;
}

bool LicenseService::setDiscordUser(const std::string &keyId, const std::string &discordUserId,
                                    std::string *error)
{
    if (!m_db.execute("UPDATE licenses SET discord_user_id=? WHERE key_id=?",
                      {discordUserId, keyId}, error))
        return false;
    return m_db.changes() > 0;
}

std::string LicenseService::resolveKeyId(const std::string &keyOrId)
{
    // A full licence key verifies to a key id offline.
    auto parsed = license::parseAndVerify(m_publicKey, keyOrId);
    if (parsed)
        return parsed->payload.keyIdHex();

    // Otherwise treat the input as a bare key id and confirm it exists.
    LicenseInfo info = lookup(keyOrId);
    return info.exists ? info.keyId : std::string();
}

bool LicenseService::addToBlacklist(const std::string &kind, const std::string &value,
                                    const std::string &reason, const std::string &by,
                                    std::string *error)
{
    if (value.empty()) {
        if (error)
            *error = "Nothing to blacklist.";
        return false;
    }
    const bool ok = m_db.execute(
        "INSERT OR REPLACE INTO blacklist (value, kind, reason, created_by, created_unix)"
        " VALUES (?,?,?,?,?)",
        {value, kind, reason, by, nowUnix()}, error);
    if (!ok)
        return false;
    // Blacklisting a licence also revokes it so it can never be redeemed again.
    if (kind == "license")
        m_db.execute("UPDATE licenses SET status=1 WHERE key_id=?", {value});
    return true;
}

bool LicenseService::removeFromBlacklist(const std::string &kind, const std::string &value,
                                         std::string *error)
{
    if (!m_db.execute("DELETE FROM blacklist WHERE value=? AND kind=?", {value, kind}, error))
        return false;
    return m_db.changes() > 0;
}

bool LicenseService::isBlacklisted(const std::string &kind, const std::string &value)
{
    if (value.empty())
        return false;
    bool found = false;
    m_db.query("SELECT 1 FROM blacklist WHERE value=? AND kind=? LIMIT 1", {value, kind},
               [&](const Row &) { found = true; });
    return found;
}

ValidationResult LicenseService::evaluate(const std::string &keyId, const std::string &machineId,
                                          bool consumeUse)
{
    ValidationResult result;

    // Blacklist is a hard stop, whether the licence key or the device is banned.
    if (isBlacklisted("license", keyId)) {
        result.reason = "This licence has been blacklisted.";
        return result;
    }
    if (isBlacklisted("machine", machineId)) {
        result.reason = "This device has been blacklisted.";
        return result;
    }

    LicenseInfo info = lookup(keyId);

    if (!info.exists) {
        result.reason = "This licence is not recognised.";
        return result;
    }
    if (info.revoked) {
        result.reason = "This licence has been revoked.";
        return result;
    }

    // HWID lock: once a licence is bound to a motherboard it only works on that
    // machine. A different machine id is refused rather than silently rebound.
    if (!info.boundMachine.empty() && !machineId.empty() && info.boundMachine != machineId) {
        result.reason = "This licence is locked to a different device (motherboard).";
        return result;
    }

    result.keyId = keyId;
    result.type = info.type;
    result.expiryUnix = info.expiryUnix;

    const std::int64_t now = nowUnix();

    if (info.type == LicenseType::Duration && info.expiryUnix > 0 && now >= info.expiryUnix) {
        result.reason = "This licence has expired.";
        return result;
    }

    if (info.type == LicenseType::Uses) {
        const std::int64_t remaining =
            static_cast<std::int64_t>(info.maxUses) - static_cast<std::int64_t>(info.usesConsumed);
        if (remaining <= 0) {
            result.usesRemaining = 0;
            result.reason = "This licence has no uses left.";
            return result;
        }

        if (consumeUse) {
            std::string error;
            // Atomic decrement guarded by the current count, so two racing
            // servers can never over-spend the last use.
            const bool spent = m_db.execute(
                "UPDATE licenses SET uses_consumed = uses_consumed + 1"
                " WHERE key_id=? AND uses_consumed < max_uses AND status=0",
                {keyId}, &error);
            if (!spent || m_db.changes() == 0) {
                result.reason = "This licence has no uses left.";
                result.usesRemaining = 0;
                return result;
            }
            result.usesRemaining = remaining - 1;
        } else {
            result.usesRemaining = remaining;
        }
    }

    result.ok = true;
    result.reason = "Licence valid.";
    return result;
}

void LicenseService::recordActivation(const std::string &keyId, std::int64_t accountId,
                                      const std::string &machineId)
{
    const std::int64_t now = nowUnix();
    // One row per (key, machine); refresh last_seen if it already exists.
    bool updated = false;
    m_db.execute(
        "UPDATE activations SET last_seen_unix=?, account_id=? WHERE key_id=? AND machine_id=?",
        {now, accountId, keyId, machineId});
    updated = m_db.changes() > 0;
    if (!updated) {
        m_db.execute(
            "INSERT INTO activations (key_id, account_id, machine_id, activated_unix, "
            "last_seen_unix) VALUES (?,?,?,?,?)",
            {keyId, accountId, machineId, now, now});
    }
}

bool LicenseService::bindLicenceToAccount(const std::string &keyId, std::int64_t accountId,
                                          const std::string &machineId, std::string *error)
{
    if (!m_db.execute("UPDATE licenses SET bound_account_id=?, bound_machine=? WHERE key_id=?",
                      {accountId, machineId, keyId}, error))
        return false;
    recordActivation(keyId, accountId, machineId);
    return true;
}

ValidationResult LicenseService::createAccount(const std::string &username,
                                               const std::string &email,
                                               const std::string &password,
                                               const std::string &licenseKey,
                                               const std::string &machineId)
{
    ValidationResult result;

    if (username.size() < 3) {
        result.reason = "Choose a username of at least 3 characters.";
        return result;
    }
    if (password.size() < 6) {
        result.reason = "Choose a password of at least 6 characters.";
        return result;
    }

    // A licence is OPTIONAL at account creation. If one is supplied it must be
    // genuine and live; if not, the account is created unlicensed and the app
    // prompts for a key later.
    std::string keyId;
    if (!licenseKey.empty()) {
        auto parsed = license::parseAndVerify(m_publicKey, licenseKey);
        if (!parsed) {
            result.reason = "That licence key is not valid.";
            return result;
        }
        keyId = parsed->payload.keyIdHex();

        ValidationResult state = evaluate(keyId, machineId, /*consumeUse=*/false);
        if (!state.ok) {
            result.reason = state.reason;
            return result;
        }
        LicenseInfo info = lookup(keyId);
        if (info.boundAccountId) {
            result.reason = "That licence is already tied to another account.";
            return result;
        }
    }

    // Username / email must be free.
    bool taken = false;
    m_db.query("SELECT id FROM accounts WHERE username=? OR (email IS NOT NULL AND email=?)",
               {username, email}, [&](const Row &) { taken = true; });
    if (taken) {
        result.reason = "That username or email is already registered.";
        return result;
    }

    // Create the account with a stretched password.
    const Bytes salt = crypto::randomBytes(16);
    const int iterations = 200000;
    const auto hash = crypto::pbkdf2(password, salt, iterations);

    std::string error;
    const bool created = m_db.execute(
        "INSERT INTO accounts (username, email, pass_hash, pass_salt, iterations, license_key_id,"
        " created_unix) VALUES (?,?,?,?,?,?,?)",
        {username, email.empty() ? SqlValue::null() : SqlValue(email),
         crypto::toHex(Bytes(hash.begin(), hash.end())),
         crypto::toHex(salt), iterations,
         keyId.empty() ? SqlValue::null() : SqlValue(keyId), nowUnix()},
        &error);
    if (!created) {
        result.reason = "Could not create the account: " + error;
        return result;
    }

    const std::int64_t accountId = m_db.lastInsertId();
    result.ok = true; // Authenticated.
    result.accountId = accountId;
    result.username = username;

    if (keyId.empty()) {
        result.reason = "Account created. Enter a licence key to unlock the features.";
        return result;
    }

    if (!bindLicenceToAccount(keyId, accountId, machineId, &error)) {
        result.reason = "Account created but binding the licence failed: " + error;
        return result;
    }

    ValidationResult state = evaluate(keyId, machineId, /*consumeUse=*/false);
    state.ok = true;
    state.licensed = true;
    state.reason = "Account created and licence activated.";
    state.accountId = accountId;
    state.username = username;
    return state;
}

ValidationResult LicenseService::signIn(const std::string &usernameOrEmail,
                                        const std::string &password,
                                        const std::string &licenseKey,
                                        const std::string &machineId, bool consumeUse)
{
    ValidationResult result;

    std::int64_t accountId = 0;
    std::string storedHash;
    std::string storedSalt;
    int iterations = 0;
    std::string boundKeyId;
    std::string username;

    m_db.query(
        "SELECT id, pass_hash, pass_salt, iterations, license_key_id, username FROM accounts"
        " WHERE username=? OR email=?",
        {usernameOrEmail, usernameOrEmail},
        [&](const Row &row) {
            accountId = row.integer(0);
            storedHash = row.text(1);
            storedSalt = row.text(2);
            iterations = static_cast<int>(row.integer(3));
            boundKeyId = row.text(4);
            username = row.text(5);
        });

    if (accountId == 0) {
        result.reason = "No account matches those details.";
        return result;
    }

    const Bytes salt = crypto::fromHex(storedSalt);
    const auto computed = crypto::pbkdf2(password, salt, iterations);
    if (!crypto::constantTimeEquals(Bytes(computed.begin(), computed.end()),
                                    crypto::fromHex(storedHash))) {
        result.reason = "Incorrect password.";
        return result;
    }

    // If the user supplied a (new) licence key, verify and rebind it.
    if (!licenseKey.empty()) {
        auto parsed = license::parseAndVerify(m_publicKey, licenseKey);
        if (!parsed) {
            result.reason = "That licence key is not valid.";
            return result;
        }
        const std::string keyId = parsed->payload.keyIdHex();

        LicenseInfo info = lookup(keyId);
        if (info.boundAccountId && *info.boundAccountId != accountId) {
            result.reason = "That licence belongs to another account.";
            return result;
        }

        ValidationResult state = evaluate(keyId, machineId, /*consumeUse=*/false);
        if (!state.ok) {
            result.reason = state.reason;
            return result;
        }

        std::string error;
        m_db.execute("UPDATE accounts SET license_key_id=? WHERE id=?", {keyId, accountId},
                     &error);
        bindLicenceToAccount(keyId, accountId, machineId, &error);
        boundKeyId = keyId;
    }

    // Authentication succeeded. Whether the account is *licensed* is separate:
    // an unlicensed sign-in still gets in, just with the features locked.
    if (boundKeyId.empty()) {
        result.ok = true;
        result.accountId = accountId;
        result.username = username;
        result.reason = "Signed in. Enter a licence key to unlock the features.";
        return result;
    }

    ValidationResult state = evaluate(boundKeyId, machineId, consumeUse);
    state.accountId = accountId;
    state.username = username;
    if (!state.ok) {
        // The bound licence is dead (expired / revoked / exhausted). Still let
        // them in, but locked, with the reason shown.
        state.ok = true;
        state.licensed = false;
        return state;
    }
    recordActivation(boundKeyId, accountId, machineId);
    state.licensed = true;
    return state;
}

ValidationResult LicenseService::validateForAccount(std::int64_t accountId,
                                                    const std::string &machineId, bool consumeUse)
{
    ValidationResult result;

    std::string keyId;
    std::string username;
    m_db.query("SELECT license_key_id, username FROM accounts WHERE id=?", {accountId},
               [&](const Row &row) {
                   keyId = row.text(0);
                   username = row.text(1);
               });

    if (keyId.empty()) {
        // Authenticated session, but no licence bound yet: locked, not an error.
        result.ok = true;
        result.licensed = false;
        result.accountId = accountId;
        result.username = username;
        result.reason = "No licence on this account yet.";
        return result;
    }

    result = evaluate(keyId, machineId, consumeUse);
    result.accountId = accountId;
    result.username = username;
    if (result.ok) {
        recordActivation(keyId, accountId, machineId);
        result.licensed = true;
    } else {
        // Session still valid, licence no longer is.
        result.ok = true;
        result.licensed = false;
    }
    return result;
}

ValidationResult LicenseService::redeemLicense(std::int64_t accountId,
                                               const std::string &licenseKey,
                                               const std::string &machineId)
{
    ValidationResult result;
    result.accountId = accountId;

    std::string username;
    bool accountExists = false;
    m_db.query("SELECT username FROM accounts WHERE id=?", {accountId}, [&](const Row &row) {
        accountExists = true;
        username = row.text(0);
    });
    if (!accountExists) {
        result.reason = "Unknown account.";
        return result;
    }
    result.username = username;

    auto parsed = license::parseAndVerify(m_publicKey, licenseKey);
    if (!parsed) {
        result.ok = true; // Still authenticated, just not licensed.
        result.reason = "That licence key is not valid.";
        return result;
    }
    const std::string keyId = parsed->payload.keyIdHex();

    LicenseInfo info = lookup(keyId);
    if (info.boundAccountId && *info.boundAccountId != accountId) {
        result.ok = true;
        result.reason = "That licence belongs to another account.";
        return result;
    }

    // Explicit activation is how the owning account (re)binds a licence to the
    // machine in front of them - a new PC, or a motherboard swap. Clearing any
    // stale machine binding first lets the HWID lock move to this motherboard
    // instead of refusing the legitimate owner. Other accounts are still blocked
    // above, and revocation / blacklist / expiry are still enforced by evaluate.
    m_db.execute("UPDATE licenses SET bound_machine='' WHERE key_id=?", {keyId});

    ValidationResult state = evaluate(keyId, machineId, /*consumeUse=*/false);
    if (!state.ok) {
        state.ok = true;
        state.licensed = false;
        state.accountId = accountId;
        state.username = username;
        return state;
    }

    std::string error;
    m_db.execute("UPDATE accounts SET license_key_id=? WHERE id=?", {keyId, accountId}, &error);
    bindLicenceToAccount(keyId, accountId, machineId, &error);

    state.ok = true;
    state.licensed = true;
    state.accountId = accountId;
    state.username = username;
    state.reason = "Licence activated.";
    return state;
}

} // namespace ngl
