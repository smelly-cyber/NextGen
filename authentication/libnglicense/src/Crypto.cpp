#include "nglicense/Crypto.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <cstring>
#include <stdexcept>

namespace ngl::crypto {

namespace {

struct EvpMdCtx
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    ~EvpMdCtx() { EVP_MD_CTX_free(ctx); }
};

struct EvpPkey
{
    EVP_PKEY *key = nullptr;
    ~EvpPkey() { EVP_PKEY_free(key); }
};

} // namespace

Bytes randomBytes(std::size_t count)
{
    Bytes out(count);
    if (count > 0 && RAND_bytes(out.data(), static_cast<int>(count)) != 1)
        throw std::runtime_error("randomBytes: OpenSSL RAND_bytes failed");
    return out;
}

KeyPair generateKeyPair()
{
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!pctx)
        throw std::runtime_error("generateKeyPair: context allocation failed");

    EvpPkey pkey;
    if (EVP_PKEY_keygen_init(pctx) != 1 || EVP_PKEY_keygen(pctx, &pkey.key) != 1) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("generateKeyPair: Ed25519 keygen failed");
    }
    EVP_PKEY_CTX_free(pctx);

    KeyPair pair;
    std::size_t len = pair.publicKey.size();
    if (EVP_PKEY_get_raw_public_key(pkey.key, pair.publicKey.data(), &len) != 1 || len != 32)
        throw std::runtime_error("generateKeyPair: could not export public key");

    len = pair.privateSeed.size();
    if (EVP_PKEY_get_raw_private_key(pkey.key, pair.privateSeed.data(), &len) != 1 || len != 32)
        throw std::runtime_error("generateKeyPair: could not export private seed");

    return pair;
}

std::array<std::uint8_t, 64> sign(const std::array<std::uint8_t, 32> &privateSeed,
                                  const Bytes &message)
{
    EvpPkey pkey;
    pkey.key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, privateSeed.data(),
                                            privateSeed.size());
    if (!pkey.key)
        throw std::runtime_error("sign: could not import private key");

    EvpMdCtx md;
    if (EVP_DigestSignInit(md.ctx, nullptr, nullptr, nullptr, pkey.key) != 1)
        throw std::runtime_error("sign: DigestSignInit failed");

    std::array<std::uint8_t, 64> signature{};
    std::size_t sigLen = signature.size();
    if (EVP_DigestSign(md.ctx, signature.data(), &sigLen, message.data(), message.size()) != 1
        || sigLen != 64) {
        throw std::runtime_error("sign: DigestSign failed");
    }
    return signature;
}

bool verify(const std::array<std::uint8_t, 32> &publicKey, const Bytes &message,
            const std::array<std::uint8_t, 64> &signature)
{
    EvpPkey pkey;
    pkey.key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, publicKey.data(),
                                           publicKey.size());
    if (!pkey.key)
        return false;

    EvpMdCtx md;
    if (EVP_DigestVerifyInit(md.ctx, nullptr, nullptr, nullptr, pkey.key) != 1)
        return false;

    return EVP_DigestVerify(md.ctx, signature.data(), signature.size(), message.data(),
                            message.size())
           == 1;
}

std::array<std::uint8_t, 32> sha256(const Bytes &data)
{
    std::array<std::uint8_t, 32> out{};
    SHA256(data.data(), data.size(), out.data());
    return out;
}

std::array<std::uint8_t, 32> hmacSha256(const Bytes &key, const Bytes &data)
{
    std::array<std::uint8_t, 32> out{};
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data.data(), data.size(),
         out.data(), &len);
    return out;
}

std::array<std::uint8_t, 32> pbkdf2(const std::string &password, const Bytes &salt,
                                    int iterations)
{
    std::array<std::uint8_t, 32> out{};
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()), salt.data(),
                          static_cast<int>(salt.size()), iterations, EVP_sha256(),
                          static_cast<int>(out.size()), out.data())
        != 1) {
        throw std::runtime_error("pbkdf2: derivation failed");
    }
    return out;
}

bool constantTimeEquals(const Bytes &a, const Bytes &b)
{
    if (a.size() != b.size())
        return false;
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

// --- Encoding helpers ------------------------------------------------------

std::string toHex(const Bytes &data)
{
    static const char *digits = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (std::uint8_t byte : data) {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

Bytes fromHex(const std::string &hex)
{
    auto value = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };

    Bytes out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        const int hi = value(hex[i]);
        const int lo = value(hex[i + 1]);
        if (hi < 0 || lo < 0)
            break;
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::string toBase64(const Bytes &data)
{
    static const char *table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);

    std::size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        const std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(table[(n >> 6) & 0x3F]);
        out.push_back(table[n & 0x3F]);
    }
    if (i < data.size()) {
        std::uint32_t n = data[i] << 16;
        const bool two = (i + 1) < data.size();
        if (two)
            n |= data[i + 1] << 8;
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(two ? table[(n >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

Bytes fromBase64(const std::string &base64)
{
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' && c <= '9')
            return c - '0' + 52;
        if (c == '+')
            return 62;
        if (c == '/')
            return 63;
        return -1;
    };

    Bytes out;
    int buffer = 0;
    int bits = 0;
    for (char c : base64) {
        if (c == '=')
            break;
        const int v = value(c);
        if (v < 0)
            continue;
        buffer = (buffer << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace ngl::crypto
