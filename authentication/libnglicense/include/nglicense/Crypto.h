// Crypto.h - Cryptographic primitives for the NextGen Tweaks licensing system.
//
// Thin, hard-to-misuse wrappers over OpenSSL:
//   * Ed25519 key generation, detached signatures and verification. The issuer
//     (bot / server / CLI) holds the 32-byte private seed; the desktop app
//     embeds only the 32-byte public key, so it can verify a licence is genuine
//     but can never mint one.
//   * A CSPRNG, SHA-256, HMAC-SHA256 and PBKDF2 for account password hashing.
//
// Everything is std::-only and Qt-free so the same object files link into the
// backend tools and (for verification) into the Qt desktop app.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ngl {

using Bytes = std::vector<std::uint8_t>;

/// Ed25519 raw key material. The seed is the secret; keep it off client machines.
struct KeyPair
{
    std::array<std::uint8_t, 32> publicKey{};
    std::array<std::uint8_t, 32> privateSeed{};
};

namespace crypto {

/// Fills \a out with cryptographically secure random bytes. Throws on failure.
Bytes randomBytes(std::size_t count);

/// Generates a fresh Ed25519 issuer key pair.
KeyPair generateKeyPair();

/// Detached Ed25519 signature (64 bytes) of \a message under \a privateSeed.
std::array<std::uint8_t, 64> sign(const std::array<std::uint8_t, 32> &privateSeed,
                                  const Bytes &message);

/// Verifies a 64-byte Ed25519 \a signature of \a message against \a publicKey.
bool verify(const std::array<std::uint8_t, 32> &publicKey, const Bytes &message,
            const std::array<std::uint8_t, 64> &signature);

/// SHA-256 digest.
std::array<std::uint8_t, 32> sha256(const Bytes &data);

/// HMAC-SHA256 of \a data under \a key.
std::array<std::uint8_t, 32> hmacSha256(const Bytes &key, const Bytes &data);

/// PBKDF2-HMAC-SHA256 password stretch (used for account credentials).
std::array<std::uint8_t, 32> pbkdf2(const std::string &password, const Bytes &salt,
                                    int iterations);

/// Constant-time equality; use for comparing secrets / tags.
bool constantTimeEquals(const Bytes &a, const Bytes &b);

// --- Encoding helpers ------------------------------------------------------

std::string toHex(const Bytes &data);
Bytes fromHex(const std::string &hex);

std::string toBase64(const Bytes &data);
Bytes fromBase64(const std::string &base64);

} // namespace crypto
} // namespace ngl
