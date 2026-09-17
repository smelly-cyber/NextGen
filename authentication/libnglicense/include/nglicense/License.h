// License.h - The signed licence token: format, encoding and verification.
//
// A licence key is a self-authenticating token:
//
//     NGTL-XXXXXXXX-XXXXXXXX-... (Crockford base32, grouped for readability)
//
// It decodes to a fixed 35-byte payload followed by a 64-byte Ed25519
// signature over that payload. The desktop app embeds the issuer public key,
// so it can prove a key is genuine and untampered entirely offline, before it
// ever contacts the server. Only the holder of the private seed (your bot /
// server) can produce a key that verifies.
//
// The payload carries just enough to make the key meaningful on its own; the
// authoritative state (uses consumed, binding, revocation) lives in the server
// database.
#pragma once

#include "Crypto.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace ngl {

enum class LicenseType : std::uint8_t {
    Duration = 0, ///< Valid until an absolute expiry timestamp.
    Uses = 1,     ///< Valid for a fixed number of validations.
    Lifetime = 2  ///< Never expires, unlimited uses.
};

std::string toString(LicenseType type);

/// The signed body of a licence key.
struct LicensePayload
{
    std::uint8_t version = 1;
    LicenseType type = LicenseType::Duration;
    std::uint8_t flags = 0;

    /// 12 random bytes; the primary identifier of the licence in the database.
    std::array<std::uint8_t, 12> keyId{};

    std::int64_t issuedUnix = 0;
    std::int64_t expiryUnix = 0; ///< Meaningful for Duration.
    std::uint32_t maxUses = 0;   ///< Meaningful for Uses.

    /// Lower-case hex of keyId, used as the database key and in commands.
    std::string keyIdHex() const;

    /// Fixed 35-byte big-endian serialisation that gets signed.
    Bytes serialise() const;
    static std::optional<LicensePayload> deserialise(const Bytes &data);
};

/// A decoded licence key: its payload plus the signature that authenticates it.
struct ParsedLicense
{
    LicensePayload payload;
    std::array<std::uint8_t, 64> signature{};
    Bytes signedBytes; ///< Exactly the bytes the signature covers (the payload).
};

namespace license {

/// Serialised payload size in bytes.
constexpr std::size_t kPayloadSize = 35;
/// Human-facing prefix on every key.
inline constexpr char kPrefix[] = "NGTL";

/// Signs \a payload with \a privateSeed and returns the grouped key string.
std::string issue(const std::array<std::uint8_t, 32> &privateSeed,
                  const LicensePayload &payload);

/// Decodes a key string (dashes / spaces / case are ignored). No signature
/// check is performed here - use verify() for that.
std::optional<ParsedLicense> parse(const std::string &keyString);

/// True when \a parsed carries a valid signature for \a publicKey.
bool verify(const std::array<std::uint8_t, 32> &publicKey, const ParsedLicense &parsed);

/// Convenience: parse + verify in one call.
std::optional<ParsedLicense> parseAndVerify(const std::array<std::uint8_t, 32> &publicKey,
                                            const std::string &keyString);

/// Formats raw key bytes as the grouped display string (for tooling).
std::string format(const Bytes &raw);

} // namespace license
} // namespace ngl
