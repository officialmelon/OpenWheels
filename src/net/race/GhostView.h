#pragma once
// NET (PC addition): ghost race - the other racers' riders, rebuilt from their snapshots
// (GhostCapture.h) and drawn as translucent, tinted copies behind the local rider.
//
// GhostTrack (one per remote player, lives for the whole race): decodes snapshots into the full
// node state and keeps the last ~half second of them with their sender times. GhostLayer (one per
// Gameplay, a child of the Session at z 5: in front of the level's shapes and items, behind the
// local rider's layers at z 6..12) draws every track ~100 ms in the past, interpolating positions,
// rotations and scales between the two snapshots around that time; frames, draw order, parents and
// visibility switch with the older one. Nodes appear and disappear with the sender's. Ghosts are
// pure art: no bodies, nothing collides.

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "net/race/RaceWire.h"

namespace race {

struct DrawContent {
    uint32_t version = 0;
    cocos2d::Color4B color;
    std::vector<cocos2d::Vec2> vertices;   // triangles
};

class GhostTrack {
public:
    struct Frame {
        double time = 0.0;     // sender clock
        uint32_t run = 0;
        cocos2d::Vec2 focus;
        std::map<uint16_t, NodeState> nodes;
    };

    // Applies one snapshot. False when it could not be read (the track is then reset).
    bool apply(const std::string& data, double receivedAt);
    void reset();

    // The state to draw at local time `now`: two frames and the blend between them (a == b when
    // only one is known). False before the first snapshot.
    bool sample(double now, const Frame** a, const Frame** b, float* t) const;
    const std::map<uint16_t, ImageRef>& images() const { return _images; }
    const std::map<uint16_t, DrawContent>& draws() const { return _draws; }
    uint32_t run() const { return _run; }
    bool hasFrames() const { return !_frames.empty(); }
    cocos2d::Vec2 latestFocus() const { return _frames.empty() ? cocos2d::Vec2::ZERO : _frames.back().focus; }

    // Statistics.
    size_t bytes = 0;
    size_t snapshots = 0;

private:
    std::map<uint16_t, ImageRef> _images;
    std::map<uint16_t, DrawContent> _draws;
    Frame _current;
    std::deque<Frame> _frames;
    uint32_t _run = 0;
    bool _haveRun = false;
    double _offset = 0.0;            // local clock - sender clock (smallest seen = least delay)
    bool _haveOffset = false;
    double _offsetSetAt = 0.0;
};

// Per-player look.
struct GhostStyle {
    cocos2d::Color3B tint;
    std::string name;
    float alpha = 0.55f;
};

class GhostLayer : public cocos2d::Node {
public:
    static GhostLayer* create();
    // Draws `track` as player `playerId` (called every frame; creates the rig on first use).
    void drawTrack(int playerId, const GhostTrack& track, const GhostStyle& style, double now, float fade);
    // Removes rigs of players not drawn since the last call to this.
    void sweep();

private:
    struct Rig {
        cocos2d::Node* root = nullptr;
        cocos2d::Node* layers[kLayerCount] = {};
        std::map<uint16_t, cocos2d::RefPtr<cocos2d::Node>> nodes;   // retained: a parent may go first
        std::map<uint16_t, uint16_t> nodeImage;
        std::map<uint16_t, uint16_t> nodeParent;
        std::map<uint16_t, uint32_t> drawVersion;
        cocos2d::Label* tag = nullptr;
        uint32_t run = 0;
        bool drawn = false;
    };
    Rig& rig(int playerId);
    void clearRig(Rig& r);
    void applyImage(cocos2d::Node* node, const ImageRef* ref);

    std::map<int, Rig> _rigs;
};

}  // namespace race
