// NET (PC addition): message framing, see Message.h.
#include "net/Message.h"

#include <cstdlib>

namespace net {

namespace {

void putU16(std::string& out, size_t v) {
    out += static_cast<char>((v >> 8) & 0xff);
    out += static_cast<char>(v & 0xff);
}

void putU32(std::string& out, uint32_t v) {
    out += static_cast<char>((v >> 24) & 0xff);
    out += static_cast<char>((v >> 16) & 0xff);
    out += static_cast<char>((v >> 8) & 0xff);
    out += static_cast<char>(v & 0xff);
}

uint32_t getU32(const unsigned char* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint32_t getU16(const unsigned char* p) { return (static_cast<uint32_t>(p[0]) << 8) | p[1]; }

}  // namespace

const std::string& Message::get(const std::string& key) const {
    static const std::string empty;
    auto it = fields.find(key);
    return it == fields.end() ? empty : it->second;
}

long long Message::getInt(const std::string& key, long long fallback) const {
    auto it = fields.find(key);
    if (it == fields.end() || it->second.empty()) return fallback;
    char* end = nullptr;
    long long v = std::strtoll(it->second.c_str(), &end, 10);
    return (end && *end == 0) ? v : fallback;
}

uint32_t crc32(const void* data, size_t size, uint32_t crc) {
    struct Table {
        uint32_t v[256];
        Table() {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
                v[i] = c;
            }
        }
    };
    static const Table tableHolder;   // thread-safe one-time init
    const uint32_t* table = tableHolder.v;
    const unsigned char* p = static_cast<const unsigned char*>(data);
    crc = ~crc;
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
    return ~crc;
}

namespace frame {

std::string encode(const Message& message) {
    std::string payload;
    payload.reserve(64 + message.type.size());
    putU16(payload, message.type.size() & 0xffff);
    payload += message.type.substr(0, 0xffff);
    putU16(payload, message.fields.size() & 0xffff);
    for (const auto& kv : message.fields) {
        putU16(payload, kv.first.size() & 0xffff);
        payload += kv.first.substr(0, 0xffff);
        putU32(payload, static_cast<uint32_t>(kv.second.size()));
        payload += kv.second;
    }
    std::string out;
    out.reserve(kHeaderSize + payload.size());
    putU32(out, kMagic);
    out += static_cast<char>(kVersion);
    out += '\0';
    putU16(out, 0);
    putU32(out, static_cast<uint32_t>(payload.size()));
    putU32(out, crc32(payload.data(), payload.size()));
    out += payload;
    return out;
}

DecodeResult decode(const std::string& buffer, size_t maxPayload, Message* message, size_t* consumed,
                    std::string* error) {
    if (buffer.size() < kHeaderSize) return DecodeResult::NeedMore;
    const unsigned char* h = reinterpret_cast<const unsigned char*>(buffer.data());
    if (getU32(h) != kMagic) {
        if (error) *error = "not an OpenWheels connection";
        return DecodeResult::Error;
    }
    if (h[4] != kVersion) {
        if (error) *error = "unsupported protocol version";
        return DecodeResult::Error;
    }
    const uint32_t length = getU32(h + 8);
    if (length > maxPayload) {
        if (error) *error = "message too large";
        return DecodeResult::Error;
    }
    if (buffer.size() < kHeaderSize + length) return DecodeResult::NeedMore;
    const unsigned char* p = h + kHeaderSize;
    if (crc32(p, length) != getU32(h + 12)) {
        if (error) *error = "checksum mismatch";
        return DecodeResult::Error;
    }
    const unsigned char* end = p + length;
    auto fail = [error]() {
        if (error) *error = "malformed message";
        return DecodeResult::Error;
    };
    Message m;
    if (end - p < 2) return fail();
    uint32_t n = getU16(p);
    p += 2;
    if (static_cast<uint32_t>(end - p) < n) return fail();
    m.type.assign(reinterpret_cast<const char*>(p), n);
    p += n;
    if (end - p < 2) return fail();
    const uint32_t count = getU16(p);
    p += 2;
    for (uint32_t i = 0; i < count; ++i) {
        if (end - p < 2) return fail();
        const uint32_t kl = getU16(p);
        p += 2;
        if (static_cast<uint32_t>(end - p) < kl) return fail();
        std::string key(reinterpret_cast<const char*>(p), kl);
        p += kl;
        if (end - p < 4) return fail();
        const uint32_t vl = getU32(p);
        p += 4;
        if (static_cast<uint32_t>(end - p) < vl) return fail();
        m.fields[key].assign(reinterpret_cast<const char*>(p), vl);
        p += vl;
    }
    if (p != end) return fail();
    if (message) *message = std::move(m);
    if (consumed) *consumed = kHeaderSize + length;
    return DecodeResult::Ok;
}

}  // namespace frame

}  // namespace net
