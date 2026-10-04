#pragma once
// NET (PC addition): ghost race - records what the local player's rider and vehicle look like, as
// compact snapshots for the other racers (who draw them as ghosts, GhostView.h).
//
// The hook is generic: every playable character (the six originals, the five restored ones, bare
// characters) and every vehicle draws only into the Session's five rig layers - character
// background / midground / foreground and vehicle background / foreground - and nothing else does
// except the browser levels' NPCs, whose nodes are named "ow_npc". So a snapshot is simply every
// node below those layers: limbs, swapped damage frames, wounds, detached gore (brain, heart,
// chunks, intestines, spinal cord), broken vehicle parts, kids, elves, the mower deck, the
// helicopter's rotor and rope... with a stable id per node, its sprite frame (sent by name once,
// then by index), parent, draw order and transform.
//
// Snapshot payload (little-endian; var = LEB128, svar = zig-zag var):
//   u8 format (1), var run, var seq, f64 time (sender clock, s), f32 focusX, f32 focusY (points),
//   u8 flags (1 = keyframe: drop everything known first)
//   var images: { var id, u8 kind, str name, str source, kind 2: f32 x, y, w, h }
//   var nodes:  { var id, var mask, [mask new: u8 kind], [parent var], [rank svar], [image var],
//                 [pos: svar dx, dy vs the last sent value, 1/8 pt], [rot svar d, 1/32 deg],
//                 [rotY svar d], [scale svar sx, sy, 1/1024], [anchor svar ax, ay, 1/1024],
//                 [flags u8], [color u8 opacity, r, g, b],
//                 [draw: var vertex count, u8 r, g, b, a, svar dx, dy per vertex, 1/4 pt] }
//   var removed: { var id }
// Deltas are against the previous snapshot of the same run (the channel is TCP: ordered, lossless).

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "cocos2d.h"
#include "net/race/RaceWire.h"

class Session;

namespace race {

class GhostCapture {
public:
    // A new local run (level start or restart): forgets everything, the next snapshot is a keyframe.
    void begin(Session* session, uint32_t run);
    void end();
    bool active() const { return _session != nullptr; }
    uint32_t run() const { return _run; }

    // Builds the next snapshot. `time` = sender clock (s), `focus` = the rider's camera focus
    // (points). Returns "" when nothing at all changed and no keyframe is due.
    std::string capture(double time, const cocos2d::Vec2& focus);

    size_t nodeCount() const { return _tracked.size(); }

private:
    struct Tracked {
        uint16_t id = 0;
        cocos2d::RefPtr<cocos2d::Node> node;
        NodeState sent;
        bool known = false;          // sent at least once
        uint32_t seen = 0;           // pass stamp
        double drawSentAt = -1.0;
        uint32_t drawHash = 0;
    };
    struct ImageKey {
        const void* texture;
        int x, y, w, h;
        bool rotated;
        bool operator<(const ImageKey& o) const;
    };

    void visit(cocos2d::Node* node, uint16_t parent, int rank, Writer& out, uint32_t& count, double time);
    uint16_t imageFor(cocos2d::Sprite* sprite);
    bool encodeDraw(cocos2d::DrawNode* draw, Tracked& t, double time, Writer& out);

    Session* _session = nullptr;
    cocos2d::RefPtr<cocos2d::Node> _layers[kLayerCount];
    uint32_t _run = 0;
    uint32_t _seq = 0;
    uint32_t _pass = 0;
    bool _keyframe = true;
    uint16_t _nextId = kFirstNodeId;
    std::unordered_map<cocos2d::Node*, Tracked> _tracked;

    // Image table (kept for the whole race: ids are never reused).
    std::map<ImageKey, uint16_t> _images;
    std::vector<std::pair<uint16_t, ImageRef>> _pendingImages;   // defined, not sent yet
    std::vector<std::pair<uint16_t, ImageRef>> _allImages;       // resent on keyframes
    uint16_t _nextImage = 1;
};

// Sprite frame lookups shared by capture (texture area -> frame name) and ghosts (name -> frame).
namespace frames {
// The name and plist (relative to the search paths) of the cached frame showing exactly this
// texture area; false when none does.
bool nameOf(cocos2d::Texture2D* texture, const cocos2d::Rect& rect, bool rotated, std::string* name,
            std::string* plist);
// A cached frame on `texture` whose area contains `rect` (not rotated).
bool containing(cocos2d::Texture2D* texture, const cocos2d::Rect& rect, std::string* name, std::string* plist,
                cocos2d::Rect* frameRect);
// Path relative to the first search path it is under ("" -> the input unchanged).
std::string relativePath(const std::string& fullPath);
// The frame by name, loading `plist` once if it is not cached (nullptr when unavailable).
cocos2d::SpriteFrame* resolve(const std::string& name, const std::string& plist);
}  // namespace frames

}  // namespace race
