#include "nglicense/License.h"

#include <cctype>
#include <cstring>

namespace ngl {

std::string toString(LicenseType type)
{
    switch (type) {
    case LicenseType::Duration:
        return "duration";
    case LicenseType::Uses:
        return "uses";
    case LicenseType::Lifetime:
        return "lifetime";
    }
    return "unknown";
}

namespace {

// Crockford base32: no I, L, O or U, so keys are hard to mis-transcribe.
constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

int base32Value(char c)
{
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    // Common confusions map back to their intended digit.
    if (c == 'I' || c == 'L')
        c = '1';
    else if (c == 'O')
        c = '0';
    else if (c == 'U')
        c = 'V';
    for (int i = 0; i < 32; ++i) {
        if (kAlphabet[i] == c)
            return i;
    }
    return -1;
}

std::string base32Encode(const Bytes &data)
{
    std::string out;
    int buffer = 0;
    int bits = 0;
    for (std::uint8_t byte : data) {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out.push_back(kAlphabet[(buffer >> bits) & 0x1F]);
        }
    }
    if (bits > 0)
        out.push_back(kAlphabet[(buffer << (5 - bits)) & 0x1F]);
    return out;
}

Bytes base32Decode(const std::string &text)
{
    Bytes out;
    int buffer = 0;
    int bits = 0;
    for (char c : text) {
        if (c == '-' || c == ' ' || c == '\t' || c == '\n' || c == '\r')
            continue;
        const int v = base32Value(c);
        if (v < 0)
            return {};
        buffer = (buffer << 5) | v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

void putU32(Bytes &out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void putU64(Bytes &out, std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
}

std::uint32_t getU32(const std::uint8_t *p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8)
           | std::uint32_t(p[3]);
}

std::uint64_t getU64(const std::uint8_t *p)
{
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | p[i];
    return v;
}

} // namespace

std::string LicensePayload::keyIdHex() const
{
    return crypto::toHex(Bytes(keyId.begin(), keyId.end()));
}

Bytes LicensePayload::serialise() const
{
    Bytes out;
    out.reserve(license::kPayloadSize);
    out.push_back(version);
    out.push_back(static_cast<std::uint8_t>(type));
    out.push_back(flags);
    out.insert(out.end(), keyId.begin(), keyId.end());
    putU64(out, static_cast<std::uint64_t>(issuedUnix));
    putU64(out, static_cast<std::uint64_t>(expiryUnix));
    putU32(out, maxUses);
    return out;
}

std::optional<LicensePayload> LicensePayload::deserialise(const Bytes &data)
{
    if (data.size() != license::kPayloadSize)
        return std::nullopt;

    LicensePayload payload;
    std::size_t offset = 0;
    payload.version = data[offset++];
    payload.type = static_cast<LicenseType>(data[offset++]);
    payload.flags = data[offset++];
    std::memcpy(payload.keyId.data(), data.data() + offset, payload.keyId.size());
    offset += payload.keyId.size();
    payload.issuedUnix = static_cast<std::int64_t>(getU64(data.data() + offset));
    offset += 8;
    payload.expiryUnix = static_cast<std::int64_t>(getU64(data.data() + offset));
    offset += 8;
    payload.maxUses = getU32(data.data() + offset);
    return payload;
}

namespace license {

std::string format(const Bytes &raw)
{
    const std::string encoded = base32Encode(raw);

    std::string out = kPrefix;
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (i % 8 == 0)
            out.push_back('-');
        out.push_back(encoded[i]);
    }
    return out;
}

std::string issue(const std::array<std::uint8_t, 32> &privateSeed, const LicensePayload &payload)
{
    const Bytes body = payload.serialise();
    const std::array<std::uint8_t, 64> signature = crypto::sign(privateSeed, body);

    Bytes raw = body;
    raw.insert(raw.end(), signature.begin(), signature.end());
    return format(raw);
}

std::optional<ParsedLicense> parse(const std::string &keyString)
{
    // Tolerate the prefix in any case, plus surrounding whitespace.
    std::string trimmed = keyString;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())))
        trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))
        trimmed.pop_back();

    const std::size_t prefixLen = std::strlen(kPrefix);
    if (trimmed.size() >= prefixLen) {
        std::string head = trimmed.substr(0, prefixLen);
        for (char &c : head)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (head == kPrefix) {
            trimmed = trimmed.substr(prefixLen);
            if (!trimmed.empty() && trimmed.front() == '-')
                trimmed.erase(trimmed.begin());
        }
    }

    const Bytes raw = base32Decode(trimmed);
    if (raw.size() < kPayloadSize + 64)
        return std::nullopt;

    ParsedLicense result;
    result.signedBytes = Bytes(raw.begin(), raw.begin() + kPayloadSize);

    auto payload = LicensePayload::deserialise(result.signedBytes);
    if (!payload)
        return std::nullopt;
    result.payload = *payload;

    std::memcpy(result.signature.data(), raw.data() + kPayloadSize, 64);
    return result;
}

bool verify(const std::array<std::uint8_t, 32> &publicKey, const ParsedLicense &parsed)
{
    return crypto::verify(publicKey, parsed.signedBytes, parsed.signature);
}

std::optional<ParsedLicense> parseAndVerify(const std::array<std::uint8_t, 32> &publicKey,
                                            const std::string &keyString)
{
    auto parsed = parse(keyString);
    if (!parsed || !verify(publicKey, *parsed))
        return std::nullopt;
    return parsed;
}

} // namespace license
} // namespace ngl
