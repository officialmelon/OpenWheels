#include "platform/common/Localization.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "cocos2d.h"
#include "platform/common/IOSBundle.h"

namespace Localization {
namespace {

// Minimal reader for Apple binary plists ("bplist00") holding a dict of string -> string,
// which is what Localizable.strings is.
class BinaryPlist {
public:
    explicit BinaryPlist(const std::vector<unsigned char>& d) : _d(d) {}

    bool readStringDict(std::unordered_map<std::string, std::string>& out) {
        if (_d.size() < 40 || std::string(_d.begin(), _d.begin() + 8) != "bplist00") return false;
        const size_t t = _d.size() - 32;
        _offsetSize = _d[t + 6];
        _refSize = _d[t + 7];
        const uint64_t numObjects = be(t + 8, 8);
        const uint64_t top = be(t + 16, 8);
        _offsetTable = (size_t)be(t + 24, 8);
        if (top >= numObjects) return false;
        const size_t off = objectOffset(top);
        const unsigned char marker = _d[off];
        if ((marker >> 4) != 0xD) return false;  // dict
        size_t pos = off + 1;
        const uint64_t count = length(marker, pos);
        for (uint64_t i = 0; i < count; ++i) {
            const uint64_t k = be(pos + i * _refSize, _refSize);
            const uint64_t v = be(pos + (count + i) * _refSize, _refSize);
            std::string key, value;
            if (readString(k, key) && readString(v, value)) out[key] = value;
        }
        return true;
    }

private:
    uint64_t be(size_t at, size_t n) const {
        uint64_t v = 0;
        for (size_t i = 0; i < n; ++i) v = (v << 8) | _d[at + i];
        return v;
    }
    size_t objectOffset(uint64_t ref) const { return (size_t)be(_offsetTable + ref * _offsetSize, _offsetSize); }
    // Low nibble 0xF means the length follows as an int object.
    uint64_t length(unsigned char marker, size_t& pos) const {
        uint64_t n = marker & 0x0F;
        if (n == 0x0F) {
            const unsigned char intMarker = _d[pos];
            const size_t bytes = (size_t)1 << (intMarker & 0x0F);
            n = be(pos + 1, bytes);
            pos += 1 + bytes;
        }
        return n;
    }
    bool readString(uint64_t ref, std::string& out) const {
        size_t pos = objectOffset(ref);
        const unsigned char marker = _d[pos++];
        const uint64_t n = length(marker, pos);
        if ((marker >> 4) == 0x5) {  // ASCII
            out.assign(_d.begin() + pos, _d.begin() + pos + n);
            return true;
        }
        if ((marker >> 4) == 0x6) {  // UTF-16BE -> UTF-8
            std::u16string u;
            for (uint64_t i = 0; i < n; ++i) u.push_back((char16_t)be(pos + i * 2, 2));
            return cocos2d::StringUtils::UTF16ToUTF8(u, out);
        }
        return false;
    }

    const std::vector<unsigned char>& _d;
    size_t _offsetSize = 0, _refSize = 0, _offsetTable = 0;
};

std::unordered_map<std::string, std::string>* g_table = nullptr;

void load() {
    g_table = new std::unordered_map<std::string, std::string>();
    if (!openwheels::hasIOSBundle()) {
        cocos2d::log("Localization: no iOS bundle; UI text falls back to keys");
        return;
    }
    const std::string path = openwheels::iosBundlePath() + "Localizable.strings";
    cocos2d::Data data = cocos2d::FileUtils::getInstance()->getDataFromFile(path);
    if (data.isNull()) {
        cocos2d::log("Localization: %s not found", path.c_str());
        return;
    }
    std::vector<unsigned char> bytes(data.getBytes(), data.getBytes() + data.getSize());
    if (!BinaryPlist(bytes).readStringDict(*g_table))
        cocos2d::log("Localization: %s is not a binary-plist string table", path.c_str());
}

}  // namespace

const std::string& get(const std::string& key) {
    if (!g_table) load();
    auto it = g_table->find(key);
    return it == g_table->end() ? key : it->second;
}

size_t size() {
    if (!g_table) load();
    return g_table->size();
}

}  // namespace Localization
