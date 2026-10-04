// NET (PC addition): ghost race - remote riders as ghosts, see GhostView.h.
#include "net/race/GhostView.h"

#include <algorithm>
#include <cmath>

#include "net/race/GhostCapture.h"
#include "online/OnlineUi.h"

USING_NS_CC;

namespace race {

namespace {
constexpr double kRenderDelay = 0.12;   // seconds behind the newest snapshot
constexpr double kKeepSeconds = 1.5;
constexpr size_t kMaxFrames = 64;

float lerp(int32_t a, int32_t b, float t) {
    return static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
}
}  // namespace

// ---- GhostTrack ------------------------------------------------------------------------------------

void GhostTrack::reset() {
    _current = Frame();
    _frames.clear();
    _draws.clear();
    _haveRun = false;
    _haveOffset = false;
}

bool GhostTrack::apply(const std::string& data, double receivedAt) {
    Reader r(data);
    const uint8_t format = r.u8();
    if (format != kSnapshotFormat) return false;
    const uint32_t run = r.var();
    r.var();   // seq
    const double time = r.f64();
    const float fx = r.f32();
    const float fy = r.f32();
    const uint8_t flags = r.u8();
    const bool keyframe = (flags & 1) != 0;
    if (!r.ok()) return false;
    bytes += data.size();
    ++snapshots;

    if (keyframe || !_haveRun || run != _run) {
        if (!keyframe) return true;   // mid-run data of a run whose start we did not see: wait
        _current = Frame();
        _frames.clear();
        _draws.clear();
        _run = run;
        _haveRun = true;
        _haveOffset = false;   // a restart may come after a pause: measure the clock again
    }

    // clock offset: the smallest (local - sender) seen is the one with the least delay; let it
    // creep up slowly so a one-off early packet does not stick forever.
    const double offset = receivedAt - time;
    if (!_haveOffset || offset < _offset) {
        _offset = offset;
        _haveOffset = true;
    } else {
        _offset += (offset - _offset) * 0.002;
    }

    const uint32_t images = r.var();
    for (uint32_t i = 0; i < images && r.ok(); ++i) {
        const uint16_t id = static_cast<uint16_t>(r.var());
        ImageRef ref;
        ref.kind = r.u8();
        ref.name = r.str();
        ref.source = r.str();
        if (ref.kind == kImageFrameRect) {
            ref.rx = r.f32();
            ref.ry = r.f32();
            ref.rw = r.f32();
            ref.rh = r.f32();
        }
        _images[id] = ref;
    }

    const uint32_t count = r.var();
    for (uint32_t i = 0; i < count && r.ok(); ++i) {
        const uint16_t id = static_cast<uint16_t>(r.var());
        const uint32_t mask = r.var();
        NodeState& s = _current.nodes[id];
        if (mask & kMaskNew) {
            s = NodeState();
            s.kind = r.u8();
        }
        if (mask & kMaskParent) s.parent = static_cast<uint16_t>(r.var());
        if (mask & kMaskRank) s.rank = r.svar();
        if (mask & kMaskImage) s.image = static_cast<uint16_t>(r.var());
        const int32_t skew = s.rotY - s.rot;
        if (mask & kMaskPos) {
            s.x += r.svar();
            s.y += r.svar();
        }
        if (mask & kMaskRot) s.rot += r.svar();
        s.rotY = s.rot + ((mask & kMaskRotY) ? r.svar() : skew);
        if (mask & kMaskScale) {
            s.sx = r.svar();
            s.sy = r.svar();
        }
        if (mask & kMaskAnchor) {
            s.ax = r.svar();
            s.ay = r.svar();
        }
        if (mask & kMaskFlags) s.flags = r.u8();
        if (mask & kMaskColor) {
            s.opacity = r.u8();
            s.r = r.u8();
            s.g = r.u8();
            s.b = r.u8();
        }
        if (mask & kMaskDraw) {
            DrawContent& d = _draws[id];
            const uint32_t n = std::min<uint32_t>(r.var(), 4096);
            d.color.r = r.u8();
            d.color.g = r.u8();
            d.color.b = r.u8();
            d.color.a = r.u8();
            d.vertices.resize(n);
            int32_t x = 0, y = 0;
            for (uint32_t k = 0; k < n; ++k) {
                x += r.svar();
                y += r.svar();
                d.vertices[k] = Vec2(x / kDrawScale, y / kDrawScale);
            }
            ++d.version;
        }
    }
    const uint32_t removed = r.var();
    for (uint32_t i = 0; i < removed && r.ok(); ++i) {
        const uint16_t id = static_cast<uint16_t>(r.var());
        _current.nodes.erase(id);
        _draws.erase(id);
    }
    if (!r.ok()) {
        log("race: unreadable ghost snapshot (%u bytes), waiting for the next run", static_cast<unsigned>(data.size()));
        reset();
        return false;
    }
    _current.time = time;
    _current.run = run;
    _current.focus = Vec2(fx, fy);
    if (!_frames.empty() && time < _frames.back().time) _frames.clear();   // clock went back: restart
    _frames.push_back(_current);
    while (_frames.size() > kMaxFrames || (_frames.size() > 2 && _frames.back().time - _frames.front().time > kKeepSeconds)) {
        _frames.pop_front();
    }
    return true;
}

bool GhostTrack::sample(double now, const Frame** a, const Frame** b, float* t) const {
    if (_frames.empty()) return false;
    const double target = now - _offset - kRenderDelay;
    if (_frames.size() == 1 || target <= _frames.front().time) {
        *a = *b = &_frames.front();
        *t = 0.0f;
        if (_frames.size() > 1 && target > _frames.front().time) *b = &_frames[1];
        return true;
    }
    for (size_t i = 0; i + 1 < _frames.size(); ++i) {
        const Frame& f0 = _frames[i];
        const Frame& f1 = _frames[i + 1];
        if (target >= f0.time && target < f1.time) {
            *a = &f0;
            *b = &f1;
            const double span = f1.time - f0.time;
            *t = span > 1e-6 ? static_cast<float>((target - f0.time) / span) : 1.0f;
            return true;
        }
    }
    *a = *b = &_frames.back();   // newer than everything received: hold the last pose
    *t = 0.0f;
    return true;
}

// ---- GhostLayer ------------------------------------------------------------------------------------

GhostLayer* GhostLayer::create() {
    GhostLayer* l = new (std::nothrow) GhostLayer();
    if (l && l->init()) {
        l->autorelease();
        l->setName("ow_race_ghosts");
        return l;
    }
    delete l;
    return nullptr;
}

GhostLayer::Rig& GhostLayer::rig(int playerId) {
    auto it = _rigs.find(playerId);
    if (it != _rigs.end()) return it->second;
    Rig& r = _rigs[playerId];
    r.root = Node::create();
    addChild(r.root, playerId);
    for (int l = 0; l < kLayerCount; ++l) {
        r.layers[l] = Node::create();
        r.root->addChild(r.layers[l], l);
    }
    return r;
}

void GhostLayer::clearRig(Rig& r) {
    for (int l = 0; l < kLayerCount; ++l) r.layers[l]->removeAllChildren();
    r.nodes.clear();
    r.nodeImage.clear();
    r.nodeParent.clear();
    r.drawVersion.clear();
}

void GhostLayer::sweep() {
    for (auto it = _rigs.begin(); it != _rigs.end();) {
        if (!it->second.drawn) {
            it->second.root->removeFromParent();
            it = _rigs.erase(it);
        } else {
            it->second.drawn = false;
            ++it;
        }
    }
}

void GhostLayer::applyImage(Node* node, const ImageRef* ref) {
    Sprite* sprite = dynamic_cast<Sprite*>(node);
    if (!sprite) return;
    bool ok = false;
    if (ref) {
        if (ref->kind == kImageFrame) {
            if (SpriteFrame* f = frames::resolve(ref->name, ref->source)) {
                sprite->setSpriteFrame(f);
                ok = true;
            }
        } else if (ref->kind == kImageFrameRect) {
            if (SpriteFrame* f = frames::resolve(ref->name, ref->source)) {
                sprite->setTexture(f->getTexture());
                const Rect fr = f->getRect();
                sprite->setTextureRect(Rect(fr.origin.x + ref->rx, fr.origin.y + ref->ry, ref->rw, ref->rh), false,
                                       Size(ref->rw, ref->rh));
                ok = true;
            }
        } else if (ref->kind == kImageFile) {
            if (FileUtils::getInstance()->isFileExist(ref->name)) {
                if (Texture2D* tex = Director::getInstance()->getTextureCache()->addImage(ref->name)) {
                    sprite->setTexture(tex);
                    sprite->setTextureRect(Rect(Vec2::ZERO, tex->getContentSize()));
                    ok = true;
                }
            }
        }
    }
    sprite->setUserData(reinterpret_cast<void*>(static_cast<uintptr_t>(ok ? 1 : 0)));
}

void GhostLayer::drawTrack(int playerId, const GhostTrack& track, const GhostStyle& style, double now, float fade) {
    const GhostTrack::Frame* a = nullptr;
    const GhostTrack::Frame* b = nullptr;
    float t = 0.0f;
    Rig& r = rig(playerId);
    r.drawn = true;
    if (!track.sample(now, &a, &b, &t)) {
        r.root->setVisible(false);
        return;
    }
    r.root->setVisible(fade > 0.01f);
    if (a->run != r.run) {
        clearRig(r);
        r.run = a->run;
    }

    // 1. drop nodes that are gone, create new ones
    for (auto it = r.nodes.begin(); it != r.nodes.end();) {
        if (!a->nodes.count(it->first)) {
            it->second->removeFromParent();
            r.nodeImage.erase(it->first);
            r.nodeParent.erase(it->first);
            r.drawVersion.erase(it->first);
            it = r.nodes.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& kv : a->nodes) {
        if (r.nodes.count(kv.first)) continue;
        Node* n = nullptr;
        switch (kv.second.kind) {
        case kKindSprite: n = Sprite::create(); break;
        case kKindDraw: n = DrawNode::create(); break;
        default: n = Node::create(); break;
        }
        r.nodes[kv.first] = n;
        r.nodeImage[kv.first] = 0xffff;
        r.nodeParent[kv.first] = 0;
    }
    // 2. parents (in a second pass: a parent may have a larger id than its child)
    for (const auto& kv : a->nodes) {
        const NodeState& s = kv.second;
        Node* n = r.nodes[kv.first].get();
        uint16_t& parent = r.nodeParent[kv.first];
        if (parent == s.parent && n->getParent()) continue;
        Node* p = nullptr;
        if (s.parent >= 1 && s.parent <= kLayerCount) {
            p = r.layers[s.parent - 1];
        } else {
            auto pit = r.nodes.find(s.parent);
            p = (pit != r.nodes.end() && pit->second.get() != n) ? pit->second.get() : r.layers[0];
        }
        if (n->getParent() != p) {
            n->retain();
            n->removeFromParentAndCleanup(false);
            p->addChild(n, s.rank);
            n->release();
        }
        parent = s.parent;
    }
    // 3. state
    const float alpha = style.alpha * fade;
    const auto& images = track.images();
    for (const auto& kv : a->nodes) {
        const NodeState& s = kv.second;
        Node* n = r.nodes[kv.first].get();
        const NodeState* e = &s;
        auto bit = b->nodes.find(kv.first);
        if (bit != b->nodes.end() && bit->second.parent == s.parent && bit->second.kind == s.kind) e = &bit->second;
        const bool visible = (s.flags & kFlagVisible) != 0;
        if (n->getLocalZOrder() != s.rank) n->setLocalZOrder(s.rank);
        if (s.kind == kKindSprite) {
            uint16_t& shown = r.nodeImage[kv.first];
            if (shown != s.image) {
                auto it = images.find(s.image);
                applyImage(n, it == images.end() ? nullptr : &it->second);
                shown = s.image;
            }
            Sprite* sprite = static_cast<Sprite*>(n);
            const bool haveArt = sprite->getUserData() != nullptr;
            n->setVisible(visible && haveArt);
            sprite->setFlippedX((s.flags & kFlagFlipX) != 0);
            sprite->setFlippedY((s.flags & kFlagFlipY) != 0);
            sprite->setColor(Color3B(static_cast<GLubyte>(s.r * style.tint.r / 255),
                                     static_cast<GLubyte>(s.g * style.tint.g / 255),
                                     static_cast<GLubyte>(s.b * style.tint.b / 255)));
            sprite->setOpacity(static_cast<GLubyte>(std::min(255.0f, s.opacity * alpha)));
        } else {
            n->setVisible(visible);
        }
        if (!visible) continue;
        n->setPosition(lerp(s.x, e->x, t) / kPosScale, lerp(s.y, e->y, t) / kPosScale);
        n->setRotationSkewX(lerp(s.rot, e->rot, t) / kRotScale);
        n->setRotationSkewY(lerp(s.rotY, e->rotY, t) / kRotScale);
        n->setScaleX(lerp(s.sx, e->sx, t) / kScaleScale);
        n->setScaleY(lerp(s.sy, e->sy, t) / kScaleScale);
        n->setAnchorPoint(Vec2(s.ax / kScaleScale, s.ay / kScaleScale));

        if (s.kind == kKindDraw) {
            auto dit = track.draws().find(kv.first);
            const uint32_t version = dit == track.draws().end() ? 0 : dit->second.version;
            uint32_t& drawn = r.drawVersion[kv.first];
            if (drawn != version || version == 0) {
                DrawNode* d = static_cast<DrawNode*>(n);
                d->clear();
                if (dit != track.draws().end()) {
                    const DrawContent& c = dit->second;
                    const Color4F color(c.color.r * style.tint.r / (255.0f * 255.0f),
                                        c.color.g * style.tint.g / (255.0f * 255.0f),
                                        c.color.b * style.tint.b / (255.0f * 255.0f), c.color.a / 255.0f * alpha);
                    for (size_t k = 0; k + 2 < c.vertices.size(); k += 3) {
                        d->drawTriangle(c.vertices[k], c.vertices[k + 1], c.vertices[k + 2], color);
                    }
                }
                drawn = version;
            }
        }
    }

    // name tag above the rider
    if (!r.tag) {
        r.tag = Label::createWithTTF(style.name, online::ui::kFontHeading, 64.0f);
        r.tag->enableOutline(Color4B(0, 0, 0, 170), 5);
        r.tag->setAnchorPoint(Vec2(0.5f, 0.0f));
        r.root->addChild(r.tag, 100);
    }
    if (r.tag->getString() != style.name) r.tag->setString(style.name);
    r.tag->setColor(style.tint);
    r.tag->setOpacity(static_cast<GLubyte>(230.0f * fade));
    const Vec2 focus = a->focus + (b->focus - a->focus) * t;
    r.tag->setPosition(focus + Vec2(0.0f, 330.0f));
}

}  // namespace race
