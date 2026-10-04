#pragma once
// NET (PC addition): the message unit of a net::Channel, and its wire framing.
//
// A Message is a type plus string fields (binary-safe values). On the wire each message is one
// frame:
//   offset 0  u32 magic  'OWNM' (0x4f574e4d)          all integers big-endian
//          4  u8  frame version (1)
//          5  u8  flags (0)
//          6  u16 reserved (0)
//          8  u32 payload length (bytes after the 16-byte header)
//         12  u32 CRC-32 (IEEE, as zlib) of the payload
//         16  payload:
//               u16 type length, type bytes
//               u16 field count
//               per field: u16 key length, key, u32 value length, value
// A frame whose magic, version, length (> the channel's limit) or CRC is wrong closes the channel.

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace net {

struct Message {
    std::string type;
    std::map<std::string, std::string> fields;

    Message() = default;
    explicit Message(std::string t) : type(std::move(t)) {}

    Message& set(const std::string& key, const std::string& value) { fields[key] = value; return *this; }
    Message& set(const std::string& key, const char* value) { fields[key] = value; return *this; }
    Message& set(const std::string& key, long long value) { fields[key] = std::to_string(value); return *this; }
    Message& set(const std::string& key, int value) { fields[key] = std::to_string(value); return *this; }
    Message& set(const std::string& key, bool value) { fields[key] = value ? "1" : "0"; return *this; }
    bool has(const std::string& key) const { return fields.count(key) != 0; }
    const std::string& get(const std::string& key) const;   // "" when missing
    long long getInt(const std::string& key, long long fallback = 0) const;
    bool getBool(const std::string& key) const { return getInt(key, 0) != 0; }
};

namespace frame {
constexpr uint32_t kMagic = 0x4f574e4du;   // "OWNM"
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 16;

std::string encode(const Message& message);
enum class DecodeResult { NeedMore, Ok, Error };
// Decodes the first frame of `buffer`. Ok: `message` filled, *consumed = frame size.
// Error: *error says why (bad magic, too large, checksum, malformed).
DecodeResult decode(const std::string& buffer, size_t maxPayload, Message* message, size_t* consumed,
                    std::string* error);
}  // namespace frame

uint32_t crc32(const void* data, size_t size, uint32_t crc = 0);

}  // namespace net
