#include "platform/common/BinaryPlist.h"

#include <cstdint>
#include <cstring>
#include <set>

#include "platform/common/AppleImage.h"

USING_NS_CC;

namespace openwheels {
namespace {

class Reader {
public:
    Reader(const unsigned char* d, size_t n) : _d(d), _n(n) {}

    Value read() {
        if (_n < 40 || std::memcmp(_d, "bplist00", 8) != 0) return Value();
        const size_t t = _n - 32;
        _offsetSize = _d[t + 6];
        _refSize = _d[t + 7];
        _numObjects = be(t + 8, 8);
        const uint64_t top = be(t + 16, 8);
        _offsetTable = (size_t)be(t + 24, 8);
        if (top >= _numObjects || _offsetTable + _numObjects * _offsetSize > _n) return Value();
        return object(top, 0);
    }

private:
    uint64_t be(size_t at, size_t n) const {
        uint64_t v = 0;
        for (size_t i = 0; i < n && at + i < _n; ++i) v = (v << 8) | _d[at + i];
        return v;
    }
    size_t offsetOf(uint64_t ref) const { return (size_t)be(_offsetTable + ref * _offsetSize, _offsetSize); }
    // Low nibble 0xF: the length follows as an int object.
    uint64_t length(unsigned char marker, size_t& pos) const {
        uint64_t n = marker & 0x0F;
        if (n == 0x0F) {
            const size_t bytes = (size_t)1 << (_d[pos] & 0x0F);
            n = be(pos + 1, bytes);
            pos += 1 + bytes;
        }
        return n;
    }

    Value object(uint64_t ref, int depth) {
        if (ref >= _numObjects || depth > 64) return Value();
        size_t pos = offsetOf(ref);
        if (pos >= _n) return Value();
        const unsigned char marker = _d[pos++];
        switch (marker >> 4) {
            case 0x0:
                if (marker == 0x08) return Value(false);
                if (marker == 0x09) return Value(true);
                return Value();
            case 0x1: {
                const size_t bytes = (size_t)1 << (marker & 0x0F);
                return Value((int)(int64_t)be(pos, bytes));
            }
            case 0x2: {
                const size_t bytes = (size_t)1 << (marker & 0x0F);
                const uint64_t bits = be(pos, bytes);
                if (bytes == 4) {
                    float f;
                    const uint32_t b32 = (uint32_t)bits;
                    std::memcpy(&f, &b32, 4);
                    return Value(f);
                }
                double d;
                std::memcpy(&d, &bits, 8);
                return Value(d);
            }
            case 0x5: {
                const uint64_t n = length(marker, pos);
                return Value(std::string(reinterpret_cast<const char*>(_d + pos), (size_t)n));
            }
            case 0x6: {
                const uint64_t n = length(marker, pos);
                std::u16string u;
                for (uint64_t i = 0; i < n; ++i) u.push_back((char16_t)be(pos + i * 2, 2));
                std::string out;
                StringUtils::UTF16ToUTF8(u, out);
                return Value(out);
            }
            case 0xA: {
                const uint64_t n = length(marker, pos);
                ValueVector v;
                for (uint64_t i = 0; i < n; ++i) v.push_back(object(be(pos + i * _refSize, _refSize), depth + 1));
                return Value(v);
            }
            case 0xD: {
                const uint64_t n = length(marker, pos);
                ValueMap m;
                for (uint64_t i = 0; i < n; ++i) {
                    const Value key = object(be(pos + i * _refSize, _refSize), depth + 1);
                    if (key.getType() != Value::Type::STRING) continue;
                    m[key.asString()] = object(be(pos + (n + i) * _refSize, _refSize), depth + 1);
                }
                return Value(m);
            }
            default:
                return Value(std::string());
        }
    }

    const unsigned char* _d;
    size_t _n;
    size_t _offsetSize = 0, _refSize = 0, _offsetTable = 0;
    uint64_t _numObjects = 0;
};

// addSpriteFramesWithDictionary is protected; a pointer to it through a derived class is the
// legal way in.
struct FrameCacheAccess : SpriteFrameCache {
    using AddFn = void (SpriteFrameCache::*)(ValueMap&, Texture2D*, const std::string&);
    static AddFn add() { return &FrameCacheAccess::addSpriteFramesWithDictionary; }
};

}  // namespace

Value parseBinaryPlist(const Data& bytes) {
    if (bytes.isNull()) return Value();
    return Reader(bytes.getBytes(), (size_t)bytes.getSize()).read();
}

ValueMap readPlistDict(const std::string& path) {
    const Data data = FileUtils::getInstance()->getDataFromFile(path);
    if (data.getSize() >= 8 && std::memcmp(data.getBytes(), "bplist00", 8) == 0) {
        const Value v = parseBinaryPlist(data);
        return v.getType() == Value::Type::MAP ? v.asValueMap() : ValueMap();
    }
    return FileUtils::getInstance()->getValueMapFromFile(path);
}

bool addSpriteFramesWithPlist(const std::string& plistPath) {
    // (the dictionary overload does not record the plist in SpriteFrameCache's loaded set)
    static std::set<std::string> loaded;
    if (loaded.count(plistPath)) return true;
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    ValueMap dict = readPlistDict(plistPath);
    if (dict.find("frames") == dict.end()) {
        log("BinaryPlist: no frames in %s", plistPath.c_str());
        return false;
    }
    std::string textureName;
    auto meta = dict.find("metadata");
    if (meta != dict.end() && meta->second.getType() == Value::Type::MAP) {
        const ValueMap& m = meta->second.asValueMap();
        auto t = m.find("textureFileName");
        if (t != m.end()) textureName = t->second.asString();
    }
    const size_t slash = plistPath.find_last_of("/\\");
    const std::string dir = slash == std::string::npos ? std::string() : plistPath.substr(0, slash + 1);
    if (textureName.empty()) {
        const size_t dot = plistPath.rfind('.');
        textureName = plistPath.substr(dir.size(), dot == std::string::npos ? std::string::npos : dot - dir.size()) + ".png";
    }
    Texture2D* texture = addTexture(dir + textureName);
    if (!texture) {
        log("BinaryPlist: cannot load texture %s%s", dir.c_str(), textureName.c_str());
        return false;
    }
    (cache->*FrameCacheAccess::add())(dict, texture, plistPath);
    loaded.insert(plistPath);
    return true;
}

}  // namespace openwheels
