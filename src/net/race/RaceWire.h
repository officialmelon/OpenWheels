#pragma once
// NET (PC addition): ghost race - shared constants, the byte-level helpers of the snapshot format
// and the ghost node state (see docs/RACE.md and GhostCapture.h for the format).

#include <cstdint>
#include <cstring>
#include <string>

namespace race {

constexpr int kRaceProtocol = 1;
constexpr const char* kRaceService = "race";
constexpr int kMaxPlayers = 4;
constexpr int kSnapshotFormat = 1;

// Ghost node ids: 1..5 are the five rig layers (parents only), sprites start at kFirstNodeId.
enum RigLayer {
    kLayerCharacterBackground = 0,   // Session z 6
    kLayerVehicleBackground = 1,     // z 7
    kLayerCharacterMidground = 2,    // z 8
    kLayerCharacterForeground = 3,   // z 11
    kLayerVehicleForeground = 4,     // z 12
    kLayerCount = 5,
};
constexpr uint16_t kFirstNodeId = 16;

// Quantization of the replicated transforms.
constexpr float kPosScale = 8.0f;      // 1/8 point
constexpr float kRotScale = 32.0f;     // 1/32 degree
constexpr float kScaleScale = 1024.0f; // 1/1024
constexpr float kDrawScale = 4.0f;     // DrawNode vertices: 1/4 point

// Change mask of one node record.
enum NodeMask : uint32_t {
    kMaskNew = 0x001,      // u8 kind follows; all other fields are relative to zero / defaults
    kMaskPos = 0x002,
    kMaskRot = 0x004,
    kMaskRotY = 0x008,     // skew: rotationSkewY - rotationSkewX (absolute, default 0)
    kMaskScale = 0x010,
    kMaskAnchor = 0x020,
    kMaskFlags = 0x040,
    kMaskRank = 0x080,
    kMaskParent = 0x100,
    kMaskImage = 0x200,
    kMaskColor = 0x400,
    kMaskDraw = 0x800,
};

enum NodeKind : uint8_t {
    kKindNode = 0,     // container (transform only)
    kKindSprite = 1,
    kKindDraw = 2,     // DrawNode: triangles
};

enum NodeFlags : uint8_t {
    kFlagVisible = 0x01,
    kFlagFlipX = 0x02,
    kFlagFlipY = 0x04,
};

// Replicated state of one node (quantized; what is sent and what a ghost interpolates).
struct NodeState {
    uint8_t kind = kKindNode;
    uint16_t parent = 1;
    int32_t rank = 0;
    uint16_t image = 0;
    int32_t x = 0, y = 0;
    int32_t rot = 0, rotY = 0;
    int32_t sx = 1024, sy = 1024;
    int32_t ax = 512, ay = 512;
    uint8_t flags = kFlagVisible;
    uint8_t opacity = 255;
    uint8_t r = 255, g = 255, b = 255;
};

// How an image (sprite texture area) is named on the wire.
enum ImageKind : uint8_t {
    kImageFrame = 1,      // sprite frame name (+ the plist it came from, relative)
    kImageFrameRect = 2,  // a sub-rectangle (points, relative to the frame) of a sprite frame
    kImageFile = 3,       // a whole image file (relative path)
};

struct ImageRef {
    uint8_t kind = kImageFrame;
    std::string name;      // frame name / file path
    std::string source;    // plist (relative) for frames
    float rx = 0, ry = 0, rw = 0, rh = 0;   // kImageFrameRect
};

// ---- bytes -------------------------------------------------------------------------------------

class Writer {
public:
    std::string& data() { return _d; }
    void u8(uint8_t v) { _d.push_back(static_cast<char>(v)); }
    void var(uint32_t v) {
        while (v >= 0x80) {
            u8(static_cast<uint8_t>(v | 0x80));
            v >>= 7;
        }
        u8(static_cast<uint8_t>(v));
    }
    void svar(int32_t v) { var((static_cast<uint32_t>(v) << 1) ^ static_cast<uint32_t>(v >> 31)); }
    void f32(float f) {
        uint32_t u;
        static_assert(sizeof(u) == sizeof(f), "float");
        std::memcpy(&u, &f, 4);
        for (int i = 0; i < 4; ++i) u8(static_cast<uint8_t>(u >> (i * 8)));
    }
    void f64(double f) {
        uint64_t u;
        std::memcpy(&u, &f, 8);
        for (int i = 0; i < 8; ++i) u8(static_cast<uint8_t>(u >> (i * 8)));
    }
    void str(const std::string& s) {
        var(static_cast<uint32_t>(s.size()));
        _d.append(s);
    }

private:
    std::string _d;
};

class Reader {
public:
    explicit Reader(const std::string& d) : _p(d.data()), _e(d.data() + d.size()) {}
    bool ok() const { return _ok; }
    bool atEnd() const { return _p >= _e; }
    uint8_t u8() {
        if (_p >= _e) {
            _ok = false;
            return 0;
        }
        return static_cast<uint8_t>(*_p++);
    }
    uint32_t var() {
        uint32_t v = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            const uint8_t b = u8();
            v |= static_cast<uint32_t>(b & 0x7f) << shift;
            if (!(b & 0x80)) return v;
        }
        _ok = false;
        return 0;
    }
    int32_t svar() {
        const uint32_t v = var();
        return static_cast<int32_t>((v >> 1) ^ (~(v & 1) + 1));
    }
    float f32() {
        uint32_t u = 0;
        for (int i = 0; i < 4; ++i) u |= static_cast<uint32_t>(u8()) << (i * 8);
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    double f64() {
        uint64_t u = 0;
        for (int i = 0; i < 8; ++i) u |= static_cast<uint64_t>(u8()) << (i * 8);
        double f;
        std::memcpy(&f, &u, 8);
        return f;
    }
    std::string str() {
        const uint32_t n = var();
        if (!_ok || static_cast<size_t>(_e - _p) < n) {
            _ok = false;
            return std::string();
        }
        std::string s(_p, n);
        _p += n;
        return s;
    }

private:
    const char* _p;
    const char* _e;
    bool _ok = true;
};

}  // namespace race
