// LicenseService.h - The shared licensing brain.
//
// The Discord bot, the CLI and the validation server all drive this one class,
// so licence issuing, redemption, account creation and validation behave
// identically no matter which front end is used. That is the "hand in hand"
// contract: one code path, one database, one set of rules.
//
// The issuer private seed is optional: only the components that mint licences
// (bot, CLI) load it. The public key is always present so any component can
// verify a key offline before trusting the database.
#pragma once

#include "Crypto.h"
#include "Database.h"
#include "License.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ngl {

/// What a licence is granted for when issuing.
struct IssueSpec
{
    LicenseType type = LicenseType::Duration;
    std::int64_t durationSeconds = 0; ///< For Duration.
    std::uint32_t maxUses = 0;        ///< For Uses.
    std::string discordUserId;        ///< Optional: who it was minted for.
    std::string issuedBy;             ///< Admin / command that minted it.
    std::string notes;
};

struct IssueResult
{
    bool ok = false;
    std::string message;
    std::string keyString; ///< The licence key to hand to the customer.
    std::string keyId;     ///< Hex id, used by /revoke, /info, etc.
};

/// Public snapshot of a licence's live state.
struct LicenseInfo
{
    bool exists = false;
    std::string keyId;
    LicenseType type = LicenseType::Duration;
    std::int64_t issuedUnix = 0;
    std::int64_t expiryUnix = 0;
    std::uint32_t maxUses = 0;
    std::uint32_t usesConsumed = 0;
    bool revoked = false;
    std::string discordUserId;
    std::string notes;
    std::optional<std::int64_t> boundAccountId;
    std::string boundMachine;
};

/// Result of a validation / redemption / sign-in attempt.
struct ValidationResult
{
    bool ok = false;             ///< Authenticated (account exists, password ok / created).
    bool licensed = false;       ///< Has a live licence unlocking the features.
    std::string reason;          ///< Human message, safe to show the user.
    std::string keyId;
    LicenseType type = LicenseType::Duration;
    std::int64_t expiryUnix = 0; ///< 0 = not time-limited.
    std::int64_t usesRemaining = -1; ///< -1 = not use-limited.
    std::int64_t accountId = 0;
    std::string username;
};

class LicenseService
{
public:
    LicenseService(Database &db, std::array<std::uint8_t, 32> publicKey);

    /// Supplies the secret seed so this instance can mint licences.
    void setIssuerSeed(const std::array<std::uint8_t, 32> &seed);
    bool canIssue() const { return m_hasSeed; }

    const std::array<std::uint8_t, 32> &publicKey() const { return m_publicKey; }

    // --- Issuing (bot / CLI) ------------------------------------------------

    IssueResult issue(const IssueSpec &spec);
    bool revoke(const std::string &keyId, std::string *error = nullptr);
    LicenseInfo lookup(const std::string &keyId);

    /// Resolves a full licence key string (NGTL-...) to its hex key id, or an
    /// empty string if the key does not verify against the issuer public key.
    /// Also accepts a bare hex key id and returns it unchanged if it exists.
    std::string resolveKeyId(const std::string &keyOrId);

    // --- Blacklisting (bot) -------------------------------------------------

    /// Blacklists a licence key id or a machine id. A blacklisted licence is
    /// also revoked. \a kind is "license" or "machine".
    bool addToBlacklist(const std::string &kind, const std::string &value,
                        const std::string &reason, const std::string &by,
                        std::string *error = nullptr);
    /// Removes a blacklist entry. Returns true when a row was removed.
    bool removeFromBlacklist(const std::string &kind, const std::string &value,
                             std::string *error = nullptr);
    /// True when the given licence key id or machine id is blacklisted.
    bool isBlacklisted(const std::string &kind, const std::string &value);

    /// All licences, newest first; when  discordUserId is set, only those
    /// minted for that user.
    std::vector<LicenseInfo> listLicenses(const std::string &discordUserId = {}, int limit = 25);

    /// Attaches (or changes) the Discord user a licence is recorded for.
    bool setDiscordUser(const std::string &keyId, const std::string &discordUserId,
                        std::string *error = nullptr);

    // --- Accounts + licence redemption (server) -----------------------------

    /// Creates an account and binds \a licenseKey to it. The key must verify
    /// offline, exist, be active, and not already belong to another account.
    ValidationResult createAccount(const std::string &username, const std::string &email,
                                   const std::string &password, const std::string &licenseKey,
                                   const std::string &machineId);

    /// Authenticates an existing account and re-checks its bound licence. When
    /// \a licenseKey is non-empty it is (re)bound - used when a user signs in
    /// with a new key.
    ValidationResult signIn(const std::string &usernameOrEmail, const std::string &password,
                            const std::string &licenseKey, const std::string &machineId,
                            bool consumeUse);

    /// Re-validates an already bound licence without credentials (session
    /// refresh from the desktop app). Consumes a use when requested.
    ValidationResult validateForAccount(std::int64_t accountId, const std::string &machineId,
                                        bool consumeUse);

    /// Redeems a licence key for an already-authenticated account (the in-app
    /// activation prompt). Verifies the key offline, binds it, and returns the
    /// entitlement.
    ValidationResult redeemLicense(std::int64_t accountId, const std::string &licenseKey,
                                   const std::string &machineId);

private:
    /// Core licence-state check shared by every path. Optionally consumes a use.
    ValidationResult evaluate(const std::string &keyId, const std::string &machineId,
                              bool consumeUse);
    bool bindLicenceToAccount(const std::string &keyId, std::int64_t accountId,
                              const std::string &machineId, std::string *error);
    void recordActivation(const std::string &keyId, std::int64_t accountId,
                          const std::string &machineId);

    Database &m_db;
    std::array<std::uint8_t, 32> m_publicKey{};
    std::array<std::uint8_t, 32> m_issuerSeed{};
    bool m_hasSeed = false;
};

// --- Small shared helpers ---------------------------------------------------

/// Parses a human duration like "30d", "12h", "90m", "1y" into seconds.
/// Returns nullopt on malformed input.
std::optional<std::int64_t> parseDuration(const std::string &text);

/// Current wall-clock time as a unix timestamp.
std::int64_t nowUnix();

} // namespace ngl
