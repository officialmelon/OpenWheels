// ONLINE (PC addition): see FlashRuntime.h.
#include "online/FlashRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "cocos2d.h"

#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "Trigger.h"

USING_NS_CC;

namespace online {

namespace {

bool g_flashLevel = false;
float g_flashVersion = 0.0f;
std::set<b2Body*> g_nanBodies;  // bodies poisoned by a NaN-density shape (see flashNanBody)

Session* currentSession() { return Settings::getInstance()->getCurrentSession(); }

struct ArtEntry {
    float originX = 0.0f;  // registration point, image pixels from the top-left
    float originY = 0.0f;
    float zoom = 1.0f;     // image pixels per Flash pixel
};

std::map<std::string, ArtEntry>& artIndex()
{
    static std::map<std::string, ArtEntry> index;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        const std::string path = FileUtils::getInstance()->fullPathForFilename("generated/flash/index.tsv");
        if (!path.empty()) {
            std::istringstream in(FileUtils::getInstance()->getStringFromFile(path));
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream fields(line);
                std::string name;
                ArtEntry e;
                if (fields >> name >> e.originX >> e.originY >> e.zoom) index[name] = e;
            }
        }
    }
    return index;
}

// Layers live as long as their session; a new session (restart) gets new ones.
struct Layers {
    Session* session = nullptr;   // valid only while token.matches(session)
    Node* background = nullptr;
    Node* foreground = nullptr;
    EventListenerTouchOneByOne* touch = nullptr;
    EventListenerMouse* mouse = nullptr;
};
Layers g_layers;
SessionToken g_layersToken;

void syncSession()
{
    Session* s = currentSession();
    if (g_layers.session == s && g_layersToken.matches(s)) return;
    // The previous session is gone (its children went with it); drop the listeners.
    if (g_layers.touch) Director::getInstance()->getEventDispatcher()->removeEventListener(g_layers.touch);
    if (g_layers.mouse) Director::getInstance()->getEventDispatcher()->removeEventListener(g_layers.mouse);
    g_layers = Layers();
    g_layers.session = s;
    g_layersToken.bind(s);
}

b2Vec2 screenToWorld(const Vec2& screen)
{
    Session* s = currentSession();
    Vec2 local = s->convertToNodeSpace(screen);
    const float ptm = s->getPtmRatio();
    return b2Vec2(local.x / ptm, local.y / ptm);
}

}  // namespace

bool flashLevel() { return g_flashLevel; }
float flashVersion() { return g_flashVersion; }

namespace {
// generated/flash/sounds (tools/assets/extract_flash_sounds.py): browser sounds the Android build
// lacks. On the search path only while a browser level runs, so mobile levels keep exactly the
// sounds they had (e.g. the kid's "Kid1..." voices stay silent there, as on Android).
std::string g_soundPath;

void setFlashSoundPath(bool on)
{
    FileUtils* fu = FileUtils::getInstance();
    if (on && g_soundPath.empty()) {
        std::vector<std::string> paths = fu->getSearchPaths();
        paths.push_back(fu->getDefaultResourceRootPath());
        paths.push_back("");
        for (const std::string& path : paths) {
            const std::string candidate = path + "generated/flash/sounds/";
            if (fu->isFileExist(candidate + "index.txt")) {
                g_soundPath = candidate;
                break;
            }
        }
        if (!g_soundPath.empty()) {
            fu->addSearchPath(g_soundPath);  // after the original sounds
            log("online: browser sounds from %s (BoomboxHit -> %s)", g_soundPath.c_str(),
                fu->fullPathForFilename("BoomboxHit.ogg").c_str());
        }
    } else if (!on && !g_soundPath.empty()) {
        std::vector<std::string> paths = fu->getSearchPaths();
        std::vector<std::string> kept;
        for (const std::string& p : paths) {
            if (p.find("generated/flash/sounds") == std::string::npos) kept.push_back(p);
        }
        fu->setSearchPaths(kept);
        g_soundPath.clear();
    }
}
}  // namespace

void setFlashLevel(bool on, float browserVersion)
{
    setFlashSoundPath(on);  // ONLINE (PC addition)
    g_flashLevel = on;
    g_flashVersion = on ? browserVersion : 0.0f;
    g_nanBodies.clear();  // a new level: none of its bodies are poisoned yet
}

bool flashNanBody(b2Body* body)
{
    return g_flashLevel && body && g_nanBodies.count(body) != 0;
}

namespace {

// Box2D 2.0's NaN island (FlashRuntime.h, flashNanBody): every dynamic body touching a NaN-mass
// static body and everything joined to it through joints or touching contacts.
void poisonNanIslands(b2World* world)
{
    Session* session = currentSession();
    LevelB2D* level = session ? session->getLevel() : nullptr;
    if (!level || level->onlineNanMassBodies.empty()) return;
    auto solid = [](b2Contact* c) {
        return c->IsTouching() && c->IsEnabled() && !c->GetFixtureA()->IsSensor() &&
               !c->GetFixtureB()->IsSensor();
    };
    std::vector<b2Body*> stack;
    for (b2Contact* c = world->GetContactList(); c; c = c->GetNext()) {
        if (!solid(c)) continue;
        b2Body* a = c->GetFixtureA()->GetBody();
        b2Body* b = c->GetFixtureB()->GetBody();
        // Box2D 2.0 only solves islands of awake bodies.
        if (level->onlineNanMassBodies.count(a) && b->GetType() == b2_dynamicBody && b->IsAwake()) stack.push_back(b);
        if (level->onlineNanMassBodies.count(b) && a->GetType() == b2_dynamicBody && a->IsAwake()) stack.push_back(a);
    }
    std::vector<b2Body*> poisoned;
    while (!stack.empty()) {
        b2Body* body = stack.back();
        stack.pop_back();
        if (!body->IsActive() || !g_nanBodies.insert(body).second) continue;
        poisoned.push_back(body);
        for (b2JointEdge* je = body->GetJointList(); je; je = je->next) {
            if (je->other->GetType() == b2_dynamicBody) stack.push_back(je->other);
        }
        for (b2ContactEdge* ce = body->GetContactList(); ce; ce = ce->next) {
            if (solid(ce->contact) && ce->other->GetType() == b2_dynamicBody) stack.push_back(ce->other);
        }
    }
    // Box2D 2.0 freezes them where the NaN put them (no velocity, no collision); the art stays
    // where it was last drawn.
    for (b2Body* body : poisoned) {
        body->SetLinearVelocity(b2Vec2_zero);
        body->SetAngularVelocity(0.0f);
        body->SetActive(false);
    }
}

}  // namespace

float pointsPerFlashPx()
{
    Session* s = currentSession();
    return (s ? s->getPtmRatio() : 250.0f) / kFlashPtm;
}

bool hasFlashArt(const std::string& name) { return artIndex().count(name) != 0; }

Sprite* createFlashSprite(const std::string& name)
{
    auto it = artIndex().find(name);
    if (it == artIndex().end()) return nullptr;
    Sprite* sprite = Sprite::create("generated/flash/" + name + ".png");
    if (!sprite) return nullptr;
    const Size px = sprite->getTexture()->getContentSizeInPixels();
    if (px.width <= 0 || px.height <= 0) return sprite;
    sprite->setAnchorPoint(Vec2(it->second.originX / px.width, 1.0f - it->second.originY / px.height));
    // contentSize (points) = pixels / content scale; we want pixels / zoom * pointsPerFlashPx.
    sprite->setScale(pointsPerFlashPx() * Director::getInstance()->getContentScaleFactor() /
                     it->second.zoom);
    return sprite;
}

Node* flashBackgroundLayer()
{
    syncSession();
    if (!g_layers.background && g_layers.session) {
        g_layers.background = Node::create();
        g_layers.session->addChild(g_layers.background, 1);
    }
    return g_layers.background;
}

Node* flashForegroundLayer()
{
    syncSession();
    if (!g_layers.foreground && g_layers.session) {
        g_layers.foreground = Node::create();
        g_layers.session->addChild(g_layers.foreground, 14);
    }
    return g_layers.foreground;
}

namespace {
struct Sleeper {
    b2Body* body;
    b2Transform xf;
};
std::vector<Sleeper> g_sleepers;
std::vector<b2Body*> g_awakeBefore;  // sorted, for the joint test
std::vector<Sleeper> g_lastGood;     // transform of every dynamic body before the step
}  // namespace

void flashPreStep(b2World* world)
{
    g_sleepers.clear();
    g_awakeBefore.clear();
    g_lastGood.clear();
    for (b2Body* body = world->GetBodyList(); body; body = body->GetNext()) {
        if (body->GetType() == b2_dynamicBody) {
            g_lastGood.push_back({body, body->GetTransform()});
            if (!body->IsAwake()) {
                g_sleepers.push_back({body, body->GetTransform()});
            } else {
                g_awakeBefore.push_back(body);
            }
        }
        static const bool keepRadius = getenv("OW_FLASH_KEEP_POLYGON_RADIUS") != nullptr;  // debugging
        if (keepRadius) continue;
        for (b2Fixture* f = body->GetFixtureList(); f; f = f->GetNext()) {
            b2Shape* shape = f->GetShape();
            if (shape->GetType() == b2Shape::e_polygon && shape->m_radius != 0.0f) shape->m_radius = 0.0f;
        }
    }
}

void flashPostStep(b2World* world)
{
    poisonNanIslands(world);
    // Some browser constructions (e.g. "string": meshing gears pinned by 99999 N m motors) blow
    // up Box2D 2.3's solver where Box2D 2.0 copes; a NaN body then hangs the next steps' TOI
    // solver. Put such bodies back where they were, at rest.
    for (const Sleeper& g : g_lastGood) {
        b2Body* b = g.body;
        const b2Vec2 p = b->GetPosition(), v = b->GetLinearVelocity();
        const float w = b->GetAngularVelocity(), a = b->GetAngle();
        const bool finite = std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(v.x) &&
                            std::isfinite(v.y) && std::isfinite(w) && std::isfinite(a);
        if (finite && v.LengthSquared() < 1.0e8f && std::fabs(w) < 1.0e5f) continue;
        b->SetTransform(g.xf.p, g.xf.q.GetAngle());
        b->SetLinearVelocity(b2Vec2_zero);
        b->SetAngularVelocity(0.0f);
    }
    static const bool keepWakes = getenv("OW_FLASH_KEEP_BOX2D_WAKES") != nullptr;  // debugging
    if (g_sleepers.empty() || keepWakes) return;
    std::sort(g_awakeBefore.begin(), g_awakeBefore.end());
    for (const Sleeper& s : g_sleepers) {
        b2Body* body = s.body;
        if (!body->IsAwake()) continue;
        bool reason = false;
        for (b2ContactEdge* ce = body->GetContactList(); ce && !reason; ce = ce->next) {
            b2Contact* c = ce->contact;
            // Box2D 2.0 wakes a sleeping body only when an awake body collides with it (a
            // static floor it rests on doesn't count).
            if (c->IsTouching() && c->IsEnabled() && !c->GetFixtureA()->IsSensor() &&
                !c->GetFixtureB()->IsSensor() &&
                std::binary_search(g_awakeBefore.begin(), g_awakeBefore.end(), ce->other)) {
                reason = true;
            }
        }
        for (b2JointEdge* je = body->GetJointList(); je && !reason; je = je->next) {
            if (std::binary_search(g_awakeBefore.begin(), g_awakeBefore.end(), je->other)) reason = true;
        }
        if (reason) continue;
        body->SetTransform(s.xf.p, s.xf.q.GetAngle());
        body->SetAwake(false);  // also clears its velocities
    }
    g_sleepers.clear();
}

void installClickTriggers(LevelB2D* level)
{
    syncSession();
    if (g_layers.touch || !g_layers.session) return;
    auto* dispatcher = Director::getInstance()->getEventDispatcher();

    // After the gameplay controls (fixed priority 1), so their buttons keep working.
    auto* touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [](Touch* t, Event*) {
        LevelB2D* lvl = currentSession() ? currentSession()->getLevel() : nullptr;
        if (!flashLevel() || !lvl) return false;
        const b2Vec2 p = screenToWorld(t->getLocation());
        for (Trigger* trigger : lvl->onlineTriggers()) {
            if (trigger && trigger->onlineClickHit(p)) return true;
        }
        return false;
    };
    // Flash fires on MOUSE_UP over the button.
    touch->onTouchEnded = [](Touch* t, Event*) {
        LevelB2D* lvl = currentSession() ? currentSession()->getLevel() : nullptr;
        if (!flashLevel() || !lvl) return;
        const b2Vec2 p = screenToWorld(t->getLocation());
        // Only the topmost enabled button gets the click: Flash adds the buttons to
        // session.buttonContainer in trigger order, so the last hit trigger is on top.
        Trigger* top = nullptr;
        for (Trigger* trigger : lvl->onlineTriggers()) {
            if (trigger && trigger->onlineClickHit(p)) top = trigger;
        }
        if (top) top->onlineMouseClick();
    };
    dispatcher->addEventListenerWithFixedPriority(touch, 2);
    g_layers.touch = touch;

    // Repeat "continuously" click triggers stop when the pointer leaves them (ROLL_OUT).
    auto* mouse = EventListenerMouse::create();
    mouse->onMouseMove = [](EventMouse* e) {
        LevelB2D* lvl = currentSession() ? currentSession()->getLevel() : nullptr;
        if (!flashLevel() || !lvl) return;
        const b2Vec2 p = screenToWorld(Vec2(e->getCursorX(), e->getCursorY()));
        for (Trigger* trigger : lvl->onlineTriggers()) {
            if (trigger) trigger->onlineMouseMove(p);
        }
    };
    dispatcher->addEventListenerWithFixedPriority(mouse, 2);
    g_layers.mouse = mouse;
    (void)level;
}

bool SessionToken::matches(Session* session) const
{
    return session && _marker && _marker->getParent() == session;
}

void SessionToken::bind(Session* session)
{
    reset();
    if (!session) return;
    _marker = Node::create();
    _marker->retain();
    _marker->setVisible(false);
    _marker->setName("onlineSessionToken");
    session->addChild(_marker);
}

void SessionToken::reset()
{
    if (!_marker) return;
    if (_marker->getParent()) _marker->removeFromParent();
    _marker->release();
    _marker = nullptr;
}

}  // namespace online
