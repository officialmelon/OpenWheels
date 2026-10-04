// NET (PC addition): ghost race snapshots of the local rider, see GhostCapture.h.
#include "net/race/GhostCapture.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Session.h"

USING_NS_CC;

namespace race {

namespace {

int32_t q(float v, float scale) {
    const double d = std::floor(static_cast<double>(v) * scale + 0.5);
    if (d > 2.0e9) return 2000000000;
    if (d < -2.0e9) return -2000000000;
    return static_cast<int32_t>(d);
}

// ---- protected engine state ----------------------------------------------------------------------

// SpriteFrameCache keeps its frames in a protected PlistFramesCache (frame name -> SpriteFrame,
// frame name -> plist) whose accessors are inline in the engine's .cpp. This mirror has the same
// members in the same order; the size check below guards it.
struct FramesCacheMirror {
    Map<std::string, SpriteFrame*> spriteFrames;
    std::unordered_map<std::string, std::set<std::string>> indexPlist2Frames;
    std::unordered_map<std::string, std::string> indexFrame2plist;
    std::unordered_map<std::string, bool> isPlistFull;
};

struct FrameCacheAccess : SpriteFrameCache {
    static_assert(sizeof(FramesCacheMirror) == sizeof(SpriteFrameCache::PlistFramesCache),
                  "SpriteFrameCache::PlistFramesCache layout changed");
    static const FramesCacheMirror& get(SpriteFrameCache* cache) {
        auto& inner = cache->*(&FrameCacheAccess::_spriteFramesCache);
        return reinterpret_cast<const FramesCacheMirror&>(inner);
    }
};

struct DrawNodeAccess : DrawNode {
    static const V2F_C4B_T2F* triangles(DrawNode* node, int* count) {
        *count = static_cast<int>(node->*(&DrawNodeAccess::_bufferCount));
        return node->*(&DrawNodeAccess::_buffer);
    }
};

// Reverse index texture -> frames, rebuilt (at most twice a second) when a lookup misses and the
// cache changed.
struct FrameEntry {
    Rect rect;
    bool rotated;
    std::string name;
};
std::unordered_map<const Texture2D*, std::vector<FrameEntry>> s_index;
size_t s_indexedCount = static_cast<size_t>(-1);
double s_lastRebuild = -10.0;

double nowSeconds() {
    return static_cast<double>(utils::getTimeInMilliseconds()) / 1000.0;
}

void rebuildIndex(bool force) {
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    const FramesCacheMirror& m = FrameCacheAccess::get(cache);
    const double now = nowSeconds();
    if (!force && (m.spriteFrames.size() == s_indexedCount || now - s_lastRebuild < 0.5)) return;
    s_lastRebuild = now;
    s_indexedCount = m.spriteFrames.size();
    s_index.clear();
    for (const auto& kv : m.spriteFrames) {
        SpriteFrame* f = kv.second;
        if (!f || !f->getTexture()) continue;
        s_index[f->getTexture()].push_back({f->getRect(), f->isRotated(), kv.first});
    }
}

bool sameRect(const Rect& a, const Rect& b) {
    return std::fabs(a.origin.x - b.origin.x) < 0.01f && std::fabs(a.origin.y - b.origin.y) < 0.01f &&
           std::fabs(a.size.width - b.size.width) < 0.01f && std::fabs(a.size.height - b.size.height) < 0.01f;
}

std::string plistOf(const std::string& frame) {
    const FramesCacheMirror& m = FrameCacheAccess::get(SpriteFrameCache::getInstance());
    auto it = m.indexFrame2plist.find(frame);
    return it == m.indexFrame2plist.end() ? std::string() : frames::relativePath(it->second);
}

}  // namespace

// ---- frames ------------------------------------------------------------------------------------

namespace frames {

std::string relativePath(const std::string& fullPath) {
    std::string path = fullPath;
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    std::string best;
    for (std::string sp : FileUtils::getInstance()->getSearchPaths()) {
        for (char& c : sp) {
            if (c == '\\') c = '/';
        }
        if (!sp.empty() && path.compare(0, sp.size(), sp) == 0 && sp.size() > best.size()) best = sp;
    }
    if (best.empty()) {
        const std::string root = FileUtils::getInstance()->getDefaultResourceRootPath();
        if (!root.empty() && path.compare(0, root.size(), root) == 0) best = root;
    }
    return best.empty() ? path : path.substr(best.size());
}

bool nameOf(Texture2D* texture, const Rect& rect, bool rotated, std::string* name, std::string* plist) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto it = s_index.find(texture);
        if (it != s_index.end()) {
            for (const FrameEntry& e : it->second) {
                if (e.rotated == rotated && sameRect(e.rect, rect)) {
                    *name = e.name;
                    *plist = plistOf(e.name);
                    return true;
                }
            }
        }
        if (attempt == 0) rebuildIndex(false);
    }
    return false;
}

bool containing(Texture2D* texture, const Rect& rect, std::string* name, std::string* plist, Rect* frameRect) {
    auto it = s_index.find(texture);
    if (it == s_index.end()) return false;
    for (const FrameEntry& e : it->second) {
        if (e.rotated) continue;
        if (rect.origin.x >= e.rect.origin.x - 0.01f && rect.origin.y >= e.rect.origin.y - 0.01f &&
            rect.getMaxX() <= e.rect.getMaxX() + 0.01f && rect.getMaxY() <= e.rect.getMaxY() + 0.01f) {
            *name = e.name;
            *plist = plistOf(e.name);
            *frameRect = e.rect;
            return true;
        }
    }
    return false;
}

SpriteFrame* resolve(const std::string& name, const std::string& plist) {
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (SpriteFrame* f = cache->getSpriteFrameByName(name)) return f;
    if (plist.empty()) return nullptr;
    static std::set<std::string> s_tried;
    if (!s_tried.insert(plist).second && !cache->isSpriteFramesWithFileLoaded(plist)) return nullptr;
    if (!FileUtils::getInstance()->isFileExist(plist)) {
        log("race: ghost art %s is not available here", plist.c_str());
        return nullptr;
    }
    cache->addSpriteFramesWithFile(plist);
    return cache->getSpriteFrameByName(name);
}

}  // namespace frames

// ---- capture -----------------------------------------------------------------------------------

bool GhostCapture::ImageKey::operator<(const ImageKey& o) const {
    if (texture != o.texture) return texture < o.texture;
    if (x != o.x) return x < o.x;
    if (y != o.y) return y < o.y;
    if (w != o.w) return w < o.w;
    if (h != o.h) return h < o.h;
    return rotated < o.rotated;
}

void GhostCapture::begin(Session* session, uint32_t run) {
    end();
    _session = session;
    _run = run;
    _seq = 0;
    _keyframe = true;
    if (!session) return;
    _layers[kLayerCharacterBackground] = session->getCharacterBackground();
    _layers[kLayerVehicleBackground] = session->getVehicleBackground();
    _layers[kLayerCharacterMidground] = session->getCharacterMidground();
    _layers[kLayerCharacterForeground] = session->getCharacterForeground();
    _layers[kLayerVehicleForeground] = session->getVehicleForeground();
}

void GhostCapture::end() {
    _session = nullptr;
    _tracked.clear();
    for (auto& l : _layers) l = nullptr;
    _nextId = kFirstNodeId;
}

uint16_t GhostCapture::imageFor(Sprite* sprite) {
    Texture2D* texture = sprite->getTexture();
    if (!texture) return 0;
    const Rect rect = sprite->getTextureRect();
    const bool rotated = sprite->isTextureRectRotated();
    ImageKey key{texture, q(rect.origin.x, 4.0f), q(rect.origin.y, 4.0f), q(rect.size.width, 4.0f),
                 q(rect.size.height, 4.0f), rotated};
    auto it = _images.find(key);
    if (it != _images.end()) return it->second;

    ImageRef ref;
    std::string name, plist;
    Rect frameRect;
    if (frames::nameOf(texture, rect, rotated, &name, &plist)) {
        ref.kind = kImageFrame;
        ref.name = name;
        ref.source = plist;
    } else if (!rotated && frames::containing(texture, rect, &name, &plist, &frameRect)) {
        ref.kind = kImageFrameRect;
        ref.name = name;
        ref.source = plist;
        ref.rx = rect.origin.x - frameRect.origin.x;
        ref.ry = rect.origin.y - frameRect.origin.y;
        ref.rw = rect.size.width;
        ref.rh = rect.size.height;
    } else {
        const Size full = texture->getContentSize();
        const std::string path = texture->getPath();
        if (!path.empty() && sameRect(rect, Rect(Vec2::ZERO, full)) && !rotated) {
            ref.kind = kImageFile;
            ref.name = frames::relativePath(path);
        } else {
            _images[key] = 0;   // unknown art: drawn as an empty container on the other side
            return 0;
        }
    }
    const uint16_t id = _nextImage++;
    _images[key] = id;
    _pendingImages.emplace_back(id, ref);
    _allImages.emplace_back(id, ref);
    return id;
}

bool GhostCapture::encodeDraw(DrawNode* draw, Tracked& t, double time, Writer& out) {
    int count = 0;
    const V2F_C4B_T2F* v = DrawNodeAccess::triangles(draw, &count);
    count = std::min(count, 1500);
    count -= count % 3;
    uint32_t hash = 2166136261u;
    auto mix = [&hash](int32_t x) {
        hash ^= static_cast<uint32_t>(x);
        hash *= 16777619u;
    };
    for (int i = 0; i < count; ++i) {
        mix(q(v[i].vertices.x, kDrawScale));
        mix(q(v[i].vertices.y, kDrawScale));
    }
    mix(count);
    if (count > 0) mix(*reinterpret_cast<const int32_t*>(&v[0].colors));
    if (t.drawSentAt >= 0.0 && (hash == t.drawHash || time - t.drawSentAt < 0.08)) return false;
    t.drawHash = hash;
    t.drawSentAt = time;
    out.var(static_cast<uint32_t>(count));
    const Color4B c = count > 0 ? v[0].colors : Color4B::WHITE;
    out.u8(c.r);
    out.u8(c.g);
    out.u8(c.b);
    out.u8(c.a);
    int32_t px = 0, py = 0;
    for (int i = 0; i < count; ++i) {
        const int32_t x = q(v[i].vertices.x, kDrawScale);
        const int32_t y = q(v[i].vertices.y, kDrawScale);
        out.svar(x - px);
        out.svar(y - py);
        px = x;
        py = y;
    }
    return true;
}

void GhostCapture::visit(Node* node, uint16_t parent, int rank, Writer& out, uint32_t& count, double time) {
    if (node->getName() == "ow_npc") return;   // browser-level NPCs share the character layer
    if (_tracked.size() > 1500) return;
    auto it = _tracked.find(node);
    if (it == _tracked.end()) {
        Tracked t;
        t.id = _nextId++;
        if (_nextId == 0) _nextId = kFirstNodeId;
        t.node = node;
        it = _tracked.emplace(node, t).first;
    }
    Tracked& t = it->second;
    t.seen = _pass;

    NodeState s;
    Sprite* sprite = dynamic_cast<Sprite*>(node);
    DrawNode* draw = sprite ? nullptr : dynamic_cast<DrawNode*>(node);
    s.kind = sprite ? kKindSprite : (draw ? kKindDraw : kKindNode);
    s.parent = parent;
    s.rank = rank;
    if (sprite) s.image = imageFor(sprite);
    s.flags = (node->isVisible() ? kFlagVisible : 0);
    if (sprite) {
        if (sprite->isFlippedX()) s.flags |= kFlagFlipX;
        if (sprite->isFlippedY()) s.flags |= kFlagFlipY;
    }
    const bool visible = node->isVisible();
    if (visible || !t.known) {
        s.x = q(node->getPositionX(), kPosScale);
        s.y = q(node->getPositionY(), kPosScale);
        s.rot = q(node->getRotationSkewX(), kRotScale);
        s.rotY = q(node->getRotationSkewY(), kRotScale);
        s.sx = q(node->getScaleX(), kScaleScale);
        s.sy = q(node->getScaleY(), kScaleScale);
        s.ax = q(node->getAnchorPoint().x, kScaleScale);
        s.ay = q(node->getAnchorPoint().y, kScaleScale);
    } else {
        // Hidden: keep the transform that was sent; only the flag changes.
        s.x = t.sent.x;
        s.y = t.sent.y;
        s.rot = t.sent.rot;
        s.rotY = t.sent.rotY;
        s.sx = t.sent.sx;
        s.sy = t.sent.sy;
        s.ax = t.sent.ax;
        s.ay = t.sent.ay;
    }
    s.opacity = node->getOpacity();
    const Color3B c = node->getColor();
    s.r = c.r;
    s.g = c.g;
    s.b = c.b;

    const NodeState base = t.known ? t.sent : NodeState();
    uint32_t mask = t.known ? 0u : kMaskNew;
    if (!t.known) {
        // A new node: everything that differs from the defaults (and the draw content).
        mask |= kMaskParent | kMaskRank | kMaskFlags;
    }
    if (s.parent != base.parent) mask |= kMaskParent;
    if (s.rank != base.rank) mask |= kMaskRank;
    if (s.image != base.image) mask |= kMaskImage;
    if (s.x != base.x || s.y != base.y) mask |= kMaskPos;
    if (s.rot != base.rot) mask |= kMaskRot;
    if (s.rotY - s.rot != base.rotY - base.rot) mask |= kMaskRotY;   // skew (rarely non-zero)
    if (s.sx != base.sx || s.sy != base.sy) mask |= kMaskScale;
    if (s.ax != base.ax || s.ay != base.ay) mask |= kMaskAnchor;
    if (s.flags != base.flags) mask |= kMaskFlags;
    if (s.opacity != base.opacity || s.r != base.r || s.g != base.g || s.b != base.b) mask |= kMaskColor;

    Writer drawOut;
    if (draw && visible && encodeDraw(draw, t, time, drawOut)) mask |= kMaskDraw;

    if (mask != 0) {
        out.var(t.id);
        out.var(mask);
        if (mask & kMaskNew) out.u8(s.kind);
        if (mask & kMaskParent) out.var(s.parent);
        if (mask & kMaskRank) out.svar(s.rank);
        if (mask & kMaskImage) out.var(s.image);
        if (mask & kMaskPos) {
            out.svar(s.x - base.x);
            out.svar(s.y - base.y);
        }
        if (mask & kMaskRot) out.svar(s.rot - base.rot);
        if (mask & kMaskRotY) out.svar(s.rotY - s.rot);
        if (mask & kMaskScale) {
            out.svar(s.sx);
            out.svar(s.sy);
        }
        if (mask & kMaskAnchor) {
            out.svar(s.ax);
            out.svar(s.ay);
        }
        if (mask & kMaskFlags) out.u8(s.flags);
        if (mask & kMaskColor) {
            out.u8(s.opacity);
            out.u8(s.r);
            out.u8(s.g);
            out.u8(s.b);
        }
        if (mask & kMaskDraw) out.data().append(drawOut.data());
        ++count;
        t.sent = s;
        t.known = true;
    }

    // Children (wounds on the chest, the helmet on the head...).
    if (node->getChildrenCount() > 0) {
        node->sortAllChildren();
        int childRank = 0;
        for (Node* child : node->getChildren()) visit(child, t.id, childRank++, out, count, time);
    }
}

std::string GhostCapture::capture(double time, const Vec2& focus) {
    if (!_session) return std::string();
    ++_pass;
    Writer body;
    uint32_t count = 0;
    if (_keyframe) {
        // Forget what was sent: every node is described again.
        for (auto& kv : _tracked) {
            kv.second.known = false;
            kv.second.drawSentAt = -1.0;
        }
        _pendingImages = _allImages;
    }
    for (int l = 0; l < kLayerCount; ++l) {
        Node* layer = _layers[l].get();
        if (!layer) continue;
        layer->sortAllChildren();
        int rank = 0;
        for (Node* child : layer->getChildren()) visit(child, static_cast<uint16_t>(l + 1), rank++, body, count, time);
    }
    std::vector<uint16_t> removed;
    for (auto it = _tracked.begin(); it != _tracked.end();) {
        if (it->second.seen != _pass) {
            if (it->second.known) removed.push_back(it->second.id);
            it = _tracked.erase(it);
        } else {
            ++it;
        }
    }

    const bool keyframe = _keyframe;
    if (!keyframe && count == 0 && removed.empty() && _pendingImages.empty() && (_seq % 10) != 0) {
        ++_seq;   // nothing moved; still send one every 10 so the other side's clock keeps up
        return std::string();
    }
    Writer out;
    out.u8(static_cast<uint8_t>(kSnapshotFormat));
    out.var(_run);
    out.var(_seq++);
    out.f64(time);
    out.f32(focus.x);
    out.f32(focus.y);
    out.u8(keyframe ? 1 : 0);
    out.var(static_cast<uint32_t>(_pendingImages.size()));
    for (const auto& img : _pendingImages) {
        out.var(img.first);
        out.u8(img.second.kind);
        out.str(img.second.name);
        out.str(img.second.source);
        if (img.second.kind == kImageFrameRect) {
            out.f32(img.second.rx);
            out.f32(img.second.ry);
            out.f32(img.second.rw);
            out.f32(img.second.rh);
        }
    }
    _pendingImages.clear();
    out.var(count);
    out.data().append(body.data());
    out.var(static_cast<uint32_t>(removed.size()));
    for (uint16_t id : removed) out.var(id);
    _keyframe = false;
    return out.data();
}

}  // namespace race
