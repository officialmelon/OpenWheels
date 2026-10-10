// ONLINE (PC addition): see TjfTestDriver.h.
#include "online/account/TjfTestDriver.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <memory>
#include <vector>

#include "cocos2d.h"
#include "CharacterB2D.h"
#include "Gameplay.h"
#include "LevelB2D.h"
#include "HWWindow.h"
#include "Session.h"
#include "Settings.h"
#include "StageCamera.h"
#include "online/FlashPhysics.h"
#include "online/HWApi.h"
#include "online/OnlineLevelBrowser.h"
#include "online/OnlinePlay.h"
#include "online/OnlineUi.h"
#include "online/account/PublishPanel.h"
#include "online/account/TjfAccount.h"
#include "online/account/TjfServices.h"
#include "online/account/TjfUi.h"
#include "online/replays/ReplayApi.h"
#include "online/replays/ReplayPanel.h"
#include "online/replays/ReplayRuntime.h"

USING_NS_CC;

namespace online {

namespace {

struct Step {
    std::string what;
    std::function<bool()> run;   // polled every 0.1 s until true
    float timeout;
};

struct Driver {
    std::vector<Step> steps;
    size_t index = 0;
    float elapsed = 0.0f;
    std::string out;
    int shots = 0;
    int failures = 0;
    OnlineLevelInfo runLevel;
    b2Vec2 recorded{0, 0}, replayed{0, 0};
    bool gotRecorded = false, gotReplayed = false;
};

Driver* g_driver = nullptr;

void say(const std::string& s) { log("[tjftest] %s", s.c_str()); }

Scene* scene() { return Director::getInstance()->getRunningScene(); }

Node* findNode(Node* root, const std::function<bool(Node*)>& match) {
    if (!root) return nullptr;
    if (match(root)) return root;
    for (Node* c : root->getChildren())
        if (Node* n = findNode(c, match)) return n;
    return nullptr;
}

Node* topModal() {
    Scene* s = scene();
    if (!s) return nullptr;
    Node* found = nullptr;
    for (Node* c : s->getChildren())
        if (c->getName() == ui::kModalNodeName && c->isVisible()) found = c;
    return found;
}

HWWindow* topWindow() {
    Scene* s = scene();
    if (!s) return nullptr;
    HWWindow* found = nullptr;
    for (Node* c : s->getChildren())
        if (auto* w = dynamic_cast<HWWindow*>(c)) found = w;
    return found;
}

ui::Button* buttonWithText(Node* root, const std::string& text) {
    return static_cast<ui::Button*>(findNode(root, [&](Node* n) {
        auto* b = dynamic_cast<ui::Button*>(n);
        return b && b->label() && b->label()->getString() == text && ui::isShown(b) && b->isEnabled();
    }));
}

void worldToScreen(const Vec2& world, float* x, float* y) {
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    const Vec2 p = director->convertToUI(world);
    const Rect vp = glview->getViewPortRect();
    *x = p.x * glview->getScaleX() + vp.origin.x;
    *y = p.y * glview->getScaleY() + vp.origin.y;
}

void tapWorld(const Vec2& world) {
    float x, y;
    worldToScreen(world, &x, &y);
    intptr_t id = 777;
    auto* view = Director::getInstance()->getOpenGLView();
    view->handleTouchesBegin(1, &id, &x, &y);
    view->handleTouchesEnd(1, &id, &x, &y);
}

bool tap(Node* node) {
    if (!node) return false;
    const Size s = node->getContentSize();
    tapWorld(node->convertToWorldSpace(Vec2(s.width * 0.5f, s.height * 0.5f)));
    return true;
}

void key(EventKeyboard::KeyCode code) {
    EventKeyboard down(code, true);
    Director::getInstance()->getEventDispatcher()->dispatchEvent(&down);
    EventKeyboard up(code, false);
    Director::getInstance()->getEventDispatcher()->dispatchEvent(&up);
}

void type(const std::string& text) { IMEDispatcher::sharedDispatcher()->dispatchInsertText(text.c_str(), text.size()); }

bool activateWindowButton(HWWindow* w, int tag) {
    auto* item = static_cast<MenuItem*>(findNode(w, [tag](Node* n) {
        auto* m = dynamic_cast<MenuItem*>(n);
        return m && m->getTag() == tag;
    }));
    if (!item) return false;
    item->activate();
    return true;
}

bool inGameplay() {
    return findNode(scene(), [](Node* n) { return dynamic_cast<Gameplay*>(n) != nullptr; }) != nullptr;
}

OnlineLevelBrowser* browser() {
    return static_cast<OnlineLevelBrowser*>(
        findNode(scene(), [](Node* n) { return dynamic_cast<OnlineLevelBrowser*>(n) != nullptr; }));
}

b2Vec2 focusPosition() {
    Session* s = Settings::getInstance()->getCurrentSession();
    if (!s || !s->getCamera() || !s->getCamera()->getFocus()) return b2Vec2(0, 0);
    return s->getCamera()->getFocus()->GetPosition();
}

// --- step builders ---------------------------------------------------------------------------------
void add(const std::string& what, std::function<bool()> run, float timeout = 15.0f) {
    g_driver->steps.push_back({what, std::move(run), timeout});
}
void doit(const std::string& what, std::function<void()> f) {
    add(what, [f]() {
        f();
        return true;
    });
}
void wait(float seconds) {
    auto until = std::make_shared<double>(-1.0);
    add("wait " + std::to_string(seconds), [until, seconds]() {
        const double now = utils::gettime();
        if (*until < 0) *until = now + seconds;
        return now >= *until;
    }, seconds + 60.0f);
}
void shot(const std::string& name) {
    add("shot " + name, [name]() {
        const std::string file = g_driver->out + name + ".png";
        utils::captureScreen([file](bool ok, const std::string&) { say((ok ? "saved " : "FAILED ") + file); }, file);
        return true;
    });
    wait(0.4f);
}
void tapButton(const std::string& text, bool inModal) {
    add("tap " + text, [text, inModal]() {
        Node* root = inModal ? topModal() : scene();
        return root && tap(buttonWithText(root, text));
    });
}
void tapNamed(const std::string& name) {
    add("tap " + name, [name]() {
        return tap(findNode(scene(), [&](Node* n) { return n->getName() == name && ui::isShown(n); }));
    });
}
void waitModal(bool open) {
    add(open ? "modal open" : "modal closed", [open]() { return (topModal() != nullptr) == open; });
    wait(0.4f);
}
void confirmWindow() {
    add("confirm alert", []() {
        HWWindow* w = topWindow();
        return w && activateWindowButton(w, 1);
    });
    wait(0.5f);
}

void tick(float dt) {
    Driver* d = g_driver;
    if (!d) return;
    if (d->index >= d->steps.size()) {
        say(d->failures ? "FINISHED with " + std::to_string(d->failures) + " failure(s)" : "FINISHED ok");
        Director::getInstance()->getScheduler()->unschedule("tjf_test", d);
        g_driver = nullptr;
        Director::getInstance()->getScheduler()->schedule([](float) { Director::getInstance()->end(); },
                                                          Director::getInstance(), 0.0f, 0, 1.0f, false, "tjf_end");
        return;
    }
    Step& s = d->steps[d->index];
    d->elapsed += dt;
    bool done = false;
    done = s.run();
    if (done) {
        say("ok: " + s.what);
        d->index++;
        d->elapsed = 0.0f;
    } else if (d->elapsed > s.timeout) {
        say("TIMEOUT: " + s.what);
        d->failures++;
        d->index++;
        d->elapsed = 0.0f;
    }
}

bool localBase() {
    const std::string b = HWApi::siteUrl();
    return b.compare(0, 17, "http://127.0.0.1:") == 0 || b.compare(0, 17, "http://localhost:") == 0;
}

// A deterministic input pattern per 30 Hz frame (both steps of a frame get the same byte on the
// 1/60 profile; one step per frame with browser physics, online/FlashPhysics.h).
uint8_t pattern(int step) {
    const int f = step / stepsPerFlashFrame();
    uint8_t b = 0x01;                          // accelerate
    if ((f / 20) % 3 == 1) b |= 0x08;          // lean back for a while
    if ((f / 25) % 4 == 2) b |= 0x04;          // lean forward
    if (f % 45 == 10) b |= 0x10;               // space now and then
    return b;
}


// Moves of everything drawn in the running level: a checksum of every sprite position under the
// Session node, compared display frame to display frame.
float drawnChecksum(Node* root) {
    float sum = root->getPositionX() * 0.37f + root->getPositionY() * 0.61f + root->getRotation() * 0.13f;
    for (Node* c : root->getChildren()) sum += drawnChecksum(c);
    return sum;
}

struct DontMove {
    int levelId = 0;
    bool completed = false;  // the recorded run reached the finish line
    bool deadRecording = false, deadWatching = false;
    int frames = 0, movedFrames = 0;  // display frames, frames whose picture moved
    float lastChecksum = 0.0f;
    bool counting = false;
    std::vector<uint64_t> recorded, watched;  // world state before every step
    int finishStep = 0;
};
DontMove g_dontMove;

bool characterDead() {
    Session* s = Settings::getInstance()->getCurrentSession();
    CharacterB2D* c = s && s->getLevel() ? s->getLevel()->getCharacter() : nullptr;
    return c && c->getDead();
}

// Every body's position, angle and velocity, bit for bit.
uint64_t worldState() {
    Session* s = Settings::getInstance()->getCurrentSession();
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](float f) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof bits);
        h = (h ^ bits) * 1099511628211ull;
    };
    if (!s || !s->getWorld()) return 0;
    for (b2Body* b = s->getWorld()->GetBodyList(); b; b = b->GetNext()) {
        mix(b->GetPosition().x);
        mix(b->GetPosition().y);
        mix(b->GetAngle());
        mix(b->GetLinearVelocity().x);
        mix(b->GetLinearVelocity().y);
        mix(b->GetAngularVelocity());
    }
    return h;
}

// Writes every body, fixture and joint not described yet as JSON lines (OW_TJF_TEST_DUMP + ".defs"),
// in the same terms as the web dump, to compare how a level and its character are built.
std::map<const void*, int> g_dumpIds;
// Each body's transform at the previous dump: where this step's contacts were evaluated.
std::map<const b2Body*, b2Transform> g_dumpPrevXf;
void dumpDefs(const char* path, int step) {
    Session* s = Settings::getInstance()->getCurrentSession();
    if (!s || !s->getWorld()) return;
    if (step == 1) g_dumpIds.clear();
    std::string file = std::string(path) + ".defs";
    FILE* f = std::fopen(file.c_str(), step == 1 ? "w" : "a");
    if (!f) return;
    auto vec = [](const b2Vec2& v) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "[%.5f,%.5f]", v.x, v.y);
        return std::string(buf);
    };
    for (b2Body* b = s->getWorld()->GetBodyList(); b; b = b->GetNext()) {
        if (g_dumpIds.count(b)) continue;
        const int id = (int)g_dumpIds.size() + 1;
        g_dumpIds[b] = id;
        std::fprintf(f, "{\"step\":%d,\"kind\":\"body\",\"id\":%d,\"static\":%s,\"pos\":%s,\"a\":%.5f,"
                     "\"lc\":%s,\"mass\":%.6f,\"I\":%.6f,\"ld\":%.4f,\"ad\":%.4f,\"awake\":%d,"
                     "\"sleepOk\":%d,\"bullet\":%d,\"fixedRot\":%d,\"shapes\":[",
                     step, id, b->GetType() == b2_staticBody ? "true" : "false", vec(b->GetPosition()).c_str(),
                     b->GetAngle(), vec(b->GetLocalCenter()).c_str(), b->GetMass(), b->GetInertia(),
                     b->GetLinearDamping(), b->GetAngularDamping(), b->IsAwake(), b->IsSleepingAllowed(),
                     b->IsBullet(), b->IsFixedRotation());
        bool first = true;
        for (b2Fixture* x = b->GetFixtureList(); x; x = x->GetNext()) {
            std::string geo;
            if (x->GetType() == b2Shape::e_circle) {
                auto* c = static_cast<b2CircleShape*>(x->GetShape());
                char buf[96];
                std::snprintf(buf, sizeof buf, "\"r\":%.5f,\"lp\":%s", c->m_radius, vec(c->m_p).c_str());
                geo = buf;
            } else if (x->GetType() == b2Shape::e_polygon) {
                auto* p = static_cast<b2PolygonShape*>(x->GetShape());
                geo = "\"verts\":[";
                for (int i = 0; i < p->m_count; ++i) geo += (i ? "," : "") + vec(p->m_vertices[i]);
                geo += "]";
            } else {
                geo = "\"other\":" + std::to_string((int)x->GetType());
            }
            const b2Filter& fl = x->GetFilterData();
            std::fprintf(f, "%s{\"t\":%d,%s,\"d\":%.4f,\"fr\":%.4f,\"re\":%.4f,\"filter\":[%d,%d,%d],\"sensor\":%d}",
                         first ? "" : ",", (int)x->GetType(), geo.c_str(), x->GetDensity(), x->GetFriction(),
                         x->GetRestitution(), fl.categoryBits, fl.maskBits, fl.groupIndex, x->IsSensor());
            first = false;
        }
        std::fprintf(f, "]}\n");
    }
    for (b2Joint* j = s->getWorld()->GetJointList(); j; j = j->GetNext()) {
        if (g_dumpIds.count(j)) continue;
        g_dumpIds[j] = (int)g_dumpIds.size() + 1;
        std::string extra;
        char buf[512];
        if (j->GetType() == e_revoluteJoint) {
            auto* r = static_cast<b2RevoluteJoint*>(j);
            std::snprintf(buf, sizeof buf, ",\"la1\":%s,\"la2\":%s,\"ref\":%.5f,\"lo\":%.5f,\"hi\":%.5f,\"lim\":%d,\"mot\":%d,\"maxT\":%.3f,\"speed\":%.4f",
                          vec(r->GetLocalAnchorA()).c_str(), vec(r->GetLocalAnchorB()).c_str(), r->GetReferenceAngle(),
                          r->GetLowerLimit(), r->GetUpperLimit(), r->IsLimitEnabled(), r->IsMotorEnabled(),
                          r->GetMaxMotorTorque(), r->GetMotorSpeed());
            extra = buf;
        } else if (j->GetType() == e_prismaticJoint) {
            auto* r = static_cast<b2PrismaticJoint*>(j);
            std::snprintf(buf, sizeof buf, ",\"la1\":%s,\"la2\":%s,\"axis\":%s,\"ref\":%.5f,\"lo\":%.5f,\"hi\":%.5f,\"lim\":%d,\"mot\":%d,\"maxF\":%.3f,\"speed\":%.4f",
                          vec(r->GetLocalAnchorA()).c_str(), vec(r->GetLocalAnchorB()).c_str(), vec(r->GetLocalAxisA()).c_str(),
                          r->GetReferenceAngle(), r->GetLowerLimit(), r->GetUpperLimit(), r->IsLimitEnabled(),
                          r->IsMotorEnabled(), r->GetMaxMotorForce(), r->GetMotorSpeed());
            extra = buf;
        } else if (j->GetType() == e_distanceJoint) {
            auto* r = static_cast<b2DistanceJoint*>(j);
            std::snprintf(buf, sizeof buf, ",\"la1\":%s,\"la2\":%s,\"len\":%.5f,\"hz\":%.4f,\"damp\":%.4f",
                          vec(r->GetLocalAnchorA()).c_str(), vec(r->GetLocalAnchorB()).c_str(), r->GetLength(),
                          r->GetFrequency(), r->GetDampingRatio());
            extra = buf;
        }
        std::fprintf(f, "{\"step\":%d,\"kind\":\"joint\",\"type\":%d,\"b1\":%d,\"b2\":%d,\"cc\":%d%s}\n", step,
                     (int)j->GetType(), g_dumpIds.count(j->GetBodyA()) ? g_dumpIds[j->GetBodyA()] : -1,
                     g_dumpIds.count(j->GetBodyB()) ? g_dumpIds[j->GetBodyB()] : -1, j->GetCollideConnected(),
                     extra.c_str());
    }
    std::fclose(f);
}

// Appends the touching contacts (OW_TJF_TEST_DUMP + ".contacts") between steps
// OW_TJF_TEST_CONTACTS_FROM and _TO: bodies, normal and per point separation and impulses.
void dumpContacts(const char* path, int step) {
    const char* from = std::getenv("OW_TJF_TEST_CONTACTS_FROM");
    const char* to = std::getenv("OW_TJF_TEST_CONTACTS_TO");
    if (!from || !to || step < std::atoi(from) || step > std::atoi(to)) return;
    Session* s = Settings::getInstance()->getCurrentSession();
    if (!s || !s->getWorld()) return;
    std::string file = std::string(path) + ".contacts";
    FILE* f = std::fopen(file.c_str(), "a");
    if (!f) return;
    for (b2Contact* c = s->getWorld()->GetContactList(); c; c = c->GetNext()) {
        b2Body* a = c->GetFixtureA()->GetBody();
        b2Body* b = c->GetFixtureB()->GetBody();
        if (!c->IsTouching()) {
            if (std::getenv("OW_TJF_TEST_CONTACTS_ALL")) {
                b2DistanceInput in;
                in.proxyA.Set(c->GetFixtureA()->GetShape(), c->GetChildIndexA());
                in.proxyB.Set(c->GetFixtureB()->GetShape(), c->GetChildIndexB());
                in.transformA = a->GetTransform();
                in.transformB = b->GetTransform();
                in.useRadii = true;
                b2SimplexCache cache;
                cache.count = 0;
                b2DistanceOutput out;
                b2Distance(&out, &cache, &in);
                std::fprintf(f, "%d %d %d untouched types %d %d dist %.5f enabled %d\n", step,
                             g_dumpIds.count(a) ? g_dumpIds[a] : -1, g_dumpIds.count(b) ? g_dumpIds[b] : -1,
                             (int)c->GetFixtureA()->GetType(), (int)c->GetFixtureB()->GetType(), out.distance,
                             c->IsEnabled());
            }
            continue;
        }
        b2WorldManifold wm;
        const b2Manifold* m = c->GetManifold();
        auto xfA = g_dumpPrevXf.count(a) ? g_dumpPrevXf[a] : a->GetTransform();
        auto xfB = g_dumpPrevXf.count(b) ? g_dumpPrevXf[b] : b->GetTransform();
        wm.Initialize(m, xfA, c->GetFixtureA()->GetShape()->m_radius, xfB, c->GetFixtureB()->GetShape()->m_radius);
        std::fprintf(f, "%d %d %d %.4f %.4f", step, g_dumpIds.count(a) ? g_dumpIds[a] : -1,
                     g_dumpIds.count(b) ? g_dumpIds[b] : -1, wm.normal.x, wm.normal.y);
        for (int i = 0; i < m->pointCount; ++i) {
            std::fprintf(f, " [%.5f %.5f %.5f %.3f %.3f]", wm.separations[i], m->points[i].normalImpulse,
                         m->points[i].tangentImpulse, wm.points[i].x, wm.points[i].y);
        }
        std::fprintf(f, " persist %d", c->GetFlash20PersistCount());
        std::fprintf(f, " px %d %d", c->GetFixtureA()->GetFlash20ProxyId(),
                     c->GetFixtureB()->GetFlash20ProxyId());
        std::fprintf(f, "\n");
    }
    std::fclose(f);
}

// Appends every dynamic body's state, one line per body, for comparing a replay step by step
// against the web game (OW_TJF_TEST_DUMP=path).
void dumpBodies(const char* path, int step) {
    Session* s = Settings::getInstance()->getCurrentSession();
    if (!s || !s->getWorld()) return;
    FILE* f = std::fopen(path, step == 1 ? "w" : "a");
    if (!f) return;
    for (b2Body* b = s->getWorld()->GetBodyList(); b; b = b->GetNext()) {
        if (b->GetType() == b2_staticBody) continue;
        std::fprintf(f, "%d %.5f %.5f %.5f %.4f %.4f %d %.5f %.4f\n", step, b->GetPosition().x,
                     b->GetPosition().y, b->GetAngle(), b->GetLinearVelocity().x,
                     b->GetLinearVelocity().y, b->IsAwake() ? 1 : 0, b->GetMass(), b->GetAngularVelocity());
    }
    std::fclose(f);
}

// The don't-move check (--online-test dont-move): plays a browser level without touching a key,
// with browser physics on, until its finish line; then watches that run as a replay to its end
// and checks every step of it matches the run. OW_TJF_TEST_LEVEL_ID picks the level (default
// 900001), OW_TJF_TEST_BROWSER_PHYSICS=0 runs it on the 1/60 profile instead.
void dontMoveScenario() {
    const char* id = std::getenv("OW_TJF_TEST_LEVEL_ID");
    const char* physics = std::getenv("OW_TJF_TEST_BROWSER_PHYSICS");
    g_dontMove.levelId = id ? std::atoi(id) : 900001;
    setBrowserPhysicsOption(!(physics && physics[0] == '0'));
    say("dont-move: level " + std::to_string(g_dontMove.levelId) + ", browser physics " +
        (browserPhysicsOption() ? "on" : "off"));
    // Every display frame: does the picture move? With 60 fps drawing over 30 Hz steps it moves
    // on every frame while things fall; drawn at the step rate it moves on every other one.
    Director::getInstance()->getScheduler()->schedule([](float) {
        if (!g_dontMove.counting) return;
        Session* s = Settings::getInstance()->getCurrentSession();
        if (!s) return;
        const float sum = drawnChecksum(s);
        if (g_dontMove.frames > 0 && sum != g_dontMove.lastChecksum) g_dontMove.movedFrames++;
        g_dontMove.lastChecksum = sum;
        g_dontMove.frames++;
    }, &g_dontMove, 0.0f, false, "dont_move_frames");

    add("level info", []() {
        static bool asked = false;
        if (!asked) {
            asked = true;
            HWApi::getInstance()->getLevelInfo(g_dontMove.levelId, [](bool ok, const std::string& e, const OnlineLevelInfo& l) {
                if (ok) g_driver->runLevel = l;
                else say("level info failed: " + e);
            });
        }
        return g_driver->runLevel.id != 0;
    }, 30.0f);
    doit("play without input", []() {
        replays::setTestInput([](int) { return (uint8_t)0; });
        replays::setTestStepObserver(replays::kObserveEveryStep, [](int mode) {
            (mode == 1 ? g_dontMove.recorded : g_dontMove.watched).push_back(worldState());
            bool& dead = mode == 1 ? g_dontMove.deadRecording : g_dontMove.deadWatching;
            dead = dead || characterDead();
        });
        OnlineLevelBrowser::levelStarting(0);
        playOnlineLevel(g_driver->runLevel, false, nullptr);
    });
    add("playing", []() { return inGameplay(); }, 30.0f);
    doit("count frames", []() { g_dontMove.counting = true; });
    wait(1.0f);
    shot("dm_01_playing");
    add("finish line", []() {
        for (replays::RunRecord* run : replays::recentRuns(g_dontMove.levelId)) {
            if (run->completed) {
                g_dontMove.completed = true;
                g_dontMove.finishStep = run->completeStep;
                return true;
            }
        }
        return false;
    }, 60.0f);
    doit("frame report", []() {
        g_dontMove.counting = false;
        say("finished at step " + std::to_string(g_dontMove.finishStep) + " (" +
            std::to_string(g_dontMove.finishStep / (float)stepsPerFlashFrame() / 30.0f) + " s); " +
            std::to_string(g_dontMove.frames) + " frames drawn, " + std::to_string(g_dontMove.movedFrames) +
            " of them moved");
    });
    wait(1.0f);
    shot("dm_02_finished");
    doit("exit level", []() {
        replays::setTestInput(nullptr);
        say(std::string("run: the character ") + (g_dontMove.deadRecording ? "DIED" : "survived"));
        if (g_dontMove.deadRecording) g_driver->failures++;
        int a = 1;
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &a);
    });
    add("level closed", []() { return !inGameplay(); }, 30.0f);
    wait(1.0f);
    doit("open its replays", []() { replays::ReplayPanel::show(g_driver->runLevel); });
    waitModal(true);
    wait(1.5f);
    shot("dm_03_replays");
    doit("watch own run", []() { key(EventKeyboard::KeyCode::KEY_ENTER); });
    add("watching own run", []() { return inGameplay(); }, 30.0f);
    add("replay at the finish step", []() { return (int)g_dontMove.watched.size() > g_dontMove.finishStep; }, 60.0f);
    wait(1.5f);
    shot("dm_04_replay_end");
    doit("compare", []() {
        const size_t n = std::min<size_t>(g_dontMove.finishStep + 1, g_dontMove.recorded.size());
        size_t firstDifference = n;
        for (size_t i = 0; i < n && firstDifference == n; ++i)
            if (i >= g_dontMove.watched.size() || g_dontMove.watched[i] != g_dontMove.recorded[i]) firstDifference = i;
        if (firstDifference == n) {
            say("replay matches the run at all " + std::to_string(n) + " steps (exact)");
        } else {
            say("replay DIFFERS from the run from step " + std::to_string(firstDifference));
            g_driver->failures++;
        }
        say(std::string("replay: the character ") + (g_dontMove.deadWatching ? "DIED" : "survived"));
        if (g_dontMove.deadWatching) g_driver->failures++;
        replays::setTestStepObserver(-1, nullptr);
        int a = 1;
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &a);
    });
    add("replay closed", []() { return !inGameplay(); }, 30.0f);
}


// --- replay-check -----------------------------------------------------------------------------------
// Watches browser replays (OW_TJF_TEST_REPLAYS = "replayId,replayId,...", from the site the game
// talks to) fast-forwarded, and compares where each ends with the browser game's own record: the
// frame its finish line was reached (ct) or that it did not finish. One line per replay
// ("replay-check: ..."), then a summary.
struct ReplayCheck {
    std::vector<int> ids;
    size_t index = 0;
    // The replay being watched.
    replays::ReplayInfo info;
    OnlineLevelInfo level;
    replays::ReplayInput input;
    std::string xml;
    bool loaded = false, failed = false, started = false;
    int steps = 0, finishStep = -1, deathStep = -1;
    // Totals.
    int exact = 0, near = 0, finishedLate = 0, notFinished = 0, unexpected = 0, errors = 0;
};
ReplayCheck g_replayCheck;

void replayCheckNext();

void replayCheckReport() {
    ReplayCheck& c = g_replayCheck;
    const int spf = std::max(1, stepsPerFlashFrame());
    const int web = c.info.frames;
    std::string line = "replay-check: replay " + std::to_string(c.info.id) + " level " + std::to_string(c.level.id) +
                       " (" + c.level.name + ", char " + std::to_string(c.info.character) + ", " +
                       std::to_string(c.input.keys.size()) + " frames): web ";
    line += c.info.completed() ? "finished at frame " + std::to_string(web) : std::string("did not finish");
    const int ours = c.finishStep >= 0 ? (c.finishStep + spf - 1) / spf : -1;
    line += "; ours ";
    line += ours >= 0 ? "finished at frame " + std::to_string(ours) : std::string("did not finish");
    if (c.deathStep >= 0) line += ", character died at frame " + std::to_string(c.deathStep / spf);
    std::string verdict;
    if (c.info.completed() && ours >= 0) {
        const int d = ours - web;
        if (d == 0) { verdict = "EXACT"; c.exact++; }
        else if (std::abs(d) <= 3) { verdict = "NEAR (" + std::to_string(d) + ")"; c.near++; }
        else { verdict = "OFF (" + std::to_string(d) + " frames)"; c.finishedLate++; }
    } else if (c.info.completed()) {
        verdict = "MISSED THE FINISH"; c.notFinished++;
    } else if (ours >= 0) {
        verdict = "FINISHED BUT WEB DID NOT"; c.unexpected++;
    } else {
        verdict = "both did not finish"; c.exact++;
    }
    say(line + " -> " + verdict);
}

void replayCheckNext() {
    ReplayCheck& c = g_replayCheck;
    if (c.index >= c.ids.size()) {
        say("replay-check summary: " + std::to_string(c.ids.size()) + " replays, " + std::to_string(c.exact) +
            " exact, " + std::to_string(c.near) + " within 3 frames, " + std::to_string(c.finishedLate) +
            " finished off by more, " + std::to_string(c.notFinished) + " missed the finish, " +
            std::to_string(c.unexpected) + " finished when the web run did not, " + std::to_string(c.errors) +
            " errors");
        return;
    }
    const int id = c.ids[c.index];
    c.info = replays::ReplayInfo();
    c.level = OnlineLevelInfo();
    c.input = replays::ReplayInput();
    c.xml.clear();
    c.loaded = c.failed = c.started = false;
    c.steps = 0;
    c.finishStep = c.deathStep = -1;

    add("replay " + std::to_string(id) + " loaded", [id]() {
        ReplayCheck& c = g_replayCheck;
        static int asked = 0;
        if (asked != id) {
            asked = id;
            replays::ReplayApi::get()->getCombined(id, [](bool ok, const std::string& e, const replays::ReplayInfo& r,
                                                          const OnlineLevelInfo& l) {
                ReplayCheck& c = g_replayCheck;
                if (!ok) {
                    say("replay-check: getCombined failed: " + e);
                    c.failed = true;
                    return;
                }
                c.info = r;
                c.level = l;
                replays::ReplayApi::get()->download(r, l, [](bool ok, const std::string& e, const replays::ReplayInput& in,
                                                             const std::string& xml) {
                    ReplayCheck& c = g_replayCheck;
                    if (!ok) {
                        say("replay-check: download failed: " + e);
                        c.failed = true;
                        return;
                    }
                    c.input = in;
                    c.xml = xml;
                    c.loaded = true;
                });
            });
        }
        return c.loaded || c.failed;
    }, 120.0f);
    doit("watch", []() {
        ReplayCheck& c = g_replayCheck;
        if (c.failed) return;
        // Contacts right after each world step, as the browser game's dump takes them (the step
        // observer runs before the next step, after the items' actions).
        setAfterWorldStepObserver([]() {
            if (const char* dump = std::getenv("OW_TJF_TEST_DUMP")) dumpContacts(dump, g_replayCheck.steps + 1);
        });
        replays::setTestStepObserver(replays::kObserveEveryStep, [](int mode) {
            if (mode != 2) return;
            ReplayCheck& c = g_replayCheck;
            c.steps++;
            if (c.deathStep < 0 && characterDead()) c.deathStep = c.steps;
            if (const char* dump = std::getenv("OW_TJF_TEST_DUMP")) {
                dumpDefs(dump, c.steps);
                dumpBodies(dump, c.steps);
                for (b2Body* b = Settings::getInstance()->getCurrentSession()->getWorld()->GetBodyList(); b; b = b->GetNext()) {
                    g_dumpPrevXf[b] = b->GetTransform();
                }
            }
        });
        std::string error;
        OnlineLevelBrowser::levelStarting(0);
        if (!replays::watch(c.level, c.info, c.input, c.xml, "check", &error)) {
            say("replay-check: watch failed: " + error);
            c.failed = true;
        }
        c.started = true;
    });
    if (std::getenv("OW_TJF_TEST_REALTIME")) {
        add("watching", []() { return inGameplay(); }, 30.0f);
        wait(1.5f);
        shot("rc_" + std::to_string(id) + "_a");
    }
    add("replay over", []() {
        ReplayCheck& c = g_replayCheck;
        if (c.failed) return true;
        if (!inGameplay()) return false;
        const int spf = std::max(1, stepsPerFlashFrame());
        // Past the end of its keys (plus 2 s), or finished.
        return c.finishStep >= 0 || c.steps > ((int)c.input.keys.size() + 60) * spf;
    }, 900.0f);
    shot("rc_" + std::to_string(id));
    doit("report", []() {
        ReplayCheck& c = g_replayCheck;
        replays::setTestStepObserver(-1, nullptr);
        if (c.failed) c.errors++;
        else replayCheckReport();
        if (inGameplay()) {
            int a = 1;
            Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &a);
        }
        c.index++;
    });
    add("replay closed", []() { return !inGameplay(); }, 30.0f);
    doit("next", []() { replayCheckNext(); });
}

void replayCheckScenario() {
    const char* list = std::getenv("OW_TJF_TEST_REPLAYS");
    const char* physics = std::getenv("OW_TJF_TEST_BROWSER_PHYSICS");
    setBrowserPhysicsOption(!(physics && physics[0] == '0'));
    std::string s = list ? list : "";
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find(',', start);
        if (end == std::string::npos) end = s.size();
        const int id = std::atoi(s.substr(start, end - start).c_str());
        if (id > 0) g_replayCheck.ids.push_back(id);
        start = end + 1;
    }
    say("replay-check: " + std::to_string(g_replayCheck.ids.size()) + " replays, browser physics " +
        (browserPhysicsOption() ? "on" : "off"));
    // As fast as the game can draw: one world step per frame.
    if (!std::getenv("OW_TJF_TEST_REALTIME")) {
        replays::setTestFastForward(true);
        Director::getInstance()->setAnimationInterval(1.0f / 1000.0f);
    }
    Director::getInstance()->getEventDispatcher()->addCustomEventListener("levelCompleted", [](EventCustom*) {
        ReplayCheck& c = g_replayCheck;
        if (c.started && c.finishStep < 0) c.finishStep = c.steps;
    });
    replayCheckNext();
}

}  // namespace

void runTjfTestScenario(const std::string& scenario) {
    if (scenario == "live-replays") {
        // Read-only, allowed on the real site: browse, list a level's replays, watch the fastest
        // (one get_cmb_records = one replay view, like watching it in the browser). No login.
        const char* out = std::getenv("OW_TJF_TEST_OUT");
        g_driver = new Driver();
        g_driver->out = out ? std::string(out) + "/" : FileUtils::getInstance()->getWritablePath();
        say("scenario live-replays against " + HWApi::siteUrl());
        doit("open browser", []() { Director::getInstance()->replaceScene(OnlineLevelBrowser::createScene()); });
        add("list loaded", []() { return browser() && OnlineLevelBrowser::state().loaded; }, 60.0f);
        doit("search POKEMON", []() { type("POKEMON"); });
        doit("enter", []() { key(EventKeyboard::KeyCode::KEY_ENTER); });
        add("search loaded", []() {
            const BrowserState& st = OnlineLevelBrowser::state();
            return st.loaded && !st.levels.empty() && st.selected >= 0;
        }, 60.0f);
        doit("select 562820", []() {
            for (int i = 0; i < 500; ++i) {
                const BrowserState& st = OnlineLevelBrowser::state();
                if (st.levels[st.selected].id == 562820) break;
                key(EventKeyboard::KeyCode::KEY_DOWN_ARROW);
            }
        });
        wait(5.0f);
        shot("01_live_detail");
        tapNamed("tjf_replays");
        waitModal(true);
        wait(5.0f);
        shot("02_live_replays");
        doit("watch", []() { key(EventKeyboard::KeyCode::KEY_ENTER); });
        add("watching", []() { return inGameplay(); }, 60.0f);
        {
            // OW_TJF_WATCH_SECONDS: watch longer (to the replay's end) before the screenshot.
            const char* watch = std::getenv("OW_TJF_WATCH_SECONDS");
            wait(watch ? (float)std::atof(watch) : 8.0f);
        }
        shot("03_live_watch");
        doit("exit replay", []() {
            int a = 1;
            Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &a);
        });
        add("back in browser", []() { return browser() != nullptr && !inGameplay(); });
        Director::getInstance()->getScheduler()->schedule([](float dt) { tick(dt); }, g_driver, 0.1f, false, "tjf_test");
        return;
    }
    if (!localBase()) {
        say("refusing to run: OW_TJF_BASE must point at the local mock (http://127.0.0.1:<port>/)");
        Director::getInstance()->end();
        return;
    }
    if (scenario == "replay-check") {
        g_driver = new Driver();
        const char* out = std::getenv("OW_TJF_TEST_OUT");
        g_driver->out = out ? std::string(out) + "/" : FileUtils::getInstance()->getWritablePath();
        replayCheckScenario();
        Director::getInstance()->getScheduler()->schedule([](float dt) { tick(dt); }, g_driver, 0.0f, false, "tjf_test");
        return;
    }
    if (scenario == "dont-move") {
        const char* out = std::getenv("OW_TJF_TEST_OUT");
        g_driver = new Driver();
        g_driver->out = out ? std::string(out) + "/" : FileUtils::getInstance()->getWritablePath();
        dontMoveScenario();
        Director::getInstance()->getScheduler()->schedule([](float dt) { tick(dt); }, g_driver, 0.1f, false, "tjf_test");
        return;
    }
    const char* email = std::getenv("OW_TJF_TEST_EMAIL");
    const char* password = std::getenv("OW_TJF_TEST_PASSWORD");
    const char* out = std::getenv("OW_TJF_TEST_OUT");
    if (!email || !password) {
        say("refusing to run: set OW_TJF_TEST_EMAIL / OW_TJF_TEST_PASSWORD to the mock's test account");
        Director::getInstance()->end();
        return;
    }
    g_driver = new Driver();
    g_driver->out = out ? out : FileUtils::getInstance()->getWritablePath();
    if (!g_driver->out.empty() && g_driver->out.back() != '/' && g_driver->out.back() != '\\') g_driver->out += "/";
    const std::string mail = email, pass = password;
    say("scenario " + scenario + " against " + HWApi::siteUrl());

    // Start logged out.
    doit("log out", []() {
        if (account::TjfAccount::get()->loggedIn()) account::TjfAccount::get()->logout(nullptr);
    });
    doit("open browser", []() { Director::getInstance()->replaceScene(OnlineLevelBrowser::createScene()); });
    add("list loaded", []() { return browser() && OnlineLevelBrowser::state().loaded; });
    wait(0.5f);
    doit("search POKEMON", []() { type("POKEMON TRAINING"); });
    doit("enter", []() { key(EventKeyboard::KeyCode::KEY_ENTER); });
    add("search loaded", []() {
        const BrowserState& st = OnlineLevelBrowser::state();
        return st.loaded && !st.levels.empty() && st.selected >= 0 && st.levels[st.selected].id == 562820;
    });
    wait(3.0f);  // the record fetch waits 1.2 s on a selection
    shot("01_browser_detail");

    // Login.
    tapNamed("tjf_account");
    waitModal(true);
    shot("02_login");
    doit("type email", [mail]() { type(mail); });
    doit("tab", []() { key(EventKeyboard::KeyCode::KEY_TAB); });
    doit("type password", [pass]() { type(pass); });
    wait(0.3f);
    shot("03_login_filled");
    tapButton("LOG IN", true);
    add("logged in", []() { return account::TjfAccount::get()->loggedIn() && account::TjfAccount::get()->userId() > 0; });
    waitModal(false);
    wait(1.0f);
    shot("04_logged_in");

    // Favorite + rate the level.
    tapNamed("tjf_heart");
    add("favorite set", []() { return account::TjfServices::get()->isFavorite(562820); });
    wait(0.8f);
    shot("05_favorite");
    tapNamed("tjf_rate");
    waitModal(true);
    add("pick 4 stars", []() {
        auto* picker = static_cast<tjfui::StarPicker*>(
            findNode(topModal(), [](Node* n) { return dynamic_cast<tjfui::StarPicker*>(n) != nullptr; }));
        if (!picker) return false;
        const Size s = picker->getContentSize();
        tapWorld(picker->convertToWorldSpace(Vec2(s.width * 0.7f, s.height * 0.5f)));
        return true;
    });
    wait(0.3f);
    shot("06_rate");
    tapButton("SUBMIT 4 STARS", true);
    waitModal(false);
    wait(0.5f);

    // Account panel -> Favorites list.
    tapNamed("tjf_account");
    waitModal(true);
    shot("07_account");
    tapButton("FAVORITES", true);
    add("favorites listed", []() {
        const BrowserState& st = OnlineLevelBrowser::state();
        return st.special == 1 && st.loaded;
    });
    wait(1.0f);
    shot("08_favorites");

    // Replays of POKEMON TRAINING; watch the fastest.
    doit("back to search", []() { key(EventKeyboard::KeyCode::KEY_ENTER); });
    add("search loaded", []() {
        const BrowserState& st = OnlineLevelBrowser::state();
        return st.loaded && st.special == 0 && st.selected >= 0 && st.levels[st.selected].id == 562820;
    });
    wait(0.5f);
    tapNamed("tjf_replays");
    waitModal(true);
    wait(2.0f);
    shot("09_replays");
    doit("watch", []() { key(EventKeyboard::KeyCode::KEY_ENTER); });
    add("watching", []() { return inGameplay(); }, 30.0f);
    wait(6.0f);
    shot("10_watch");
    wait(14.0f);
    shot("11_watch_20s");
    wait(16.0f);
    shot("12_watch_36s");
    doit("exit replay", []() {
        int a = 1;
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &a);
    });
    add("back in browser", []() { return browser() != nullptr && !inGameplay(); });
    wait(1.0f);

    // Record a run of LEVEL EDITOR VS. YOU (forced Irresponsible Dad) with scripted keys.
    add("level info", []() {
        static bool asked = false;
        if (!asked) {
            asked = true;
            HWApi::getInstance()->getLevelInfo(10049173, [](bool ok, const std::string& e, const OnlineLevelInfo& l) {
                if (ok) g_driver->runLevel = l;
                else say("level info failed: " + e);
            });
        }
        return g_driver->runLevel.id != 0;
    });
    doit("play with scripted keys", []() {
        replays::setTestInput(pattern);
        replays::setTestStepObserver(600, [](int mode) {
            const b2Vec2 p = focusPosition();
            say("step 600 (" + std::string(mode == 1 ? "recording" : "watching") + "): x=" + std::to_string(p.x) +
                " y=" + std::to_string(p.y));
            if (mode == 1) {
                g_driver->recorded = p;
                g_driver->gotRecorded = true;
            } else {
                g_driver->replayed = p;
                g_driver->gotReplayed = true;
            }
        });
        OnlineLevelBrowser::levelStarting(0);
        playOnlineLevel(g_driver->runLevel, false, nullptr);
    });
    add("playing", []() { return inGameplay(); }, 30.0f);
    add("step 600 recorded", []() { return g_driver->gotRecorded; }, 30.0f);
    shot("13_recording");
    wait(2.0f);
    doit("exit level", []() {
        replays::setTestInput(nullptr);
        int a = 1;
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &a);
    });
    add("back in browser", []() { return browser() != nullptr && !inGameplay(); });
    wait(1.0f);
    doit("open its replays", []() { replays::ReplayPanel::show(g_driver->runLevel); });
    waitModal(true);
    wait(1.5f);
    shot("14_my_run");
    tapButton("SAVE", true);
    wait(1.0f);
    tapButton("UPLOAD", true);
    add("upload form", []() { return topModal() && buttonWithText(topModal(), "UPLOAD REPLAY"); });
    doit("comment", []() { type("scripted test run"); });
    wait(0.3f);
    shot("15_upload_form");
    tapButton("UPLOAD REPLAY", true);
    wait(0.6f);
    shot("16_upload_confirm");
    confirmWindow();
    add("uploaded", []() { return topModal() && !buttonWithText(topModal(), "UPLOAD REPLAY"); }, 20.0f);
    wait(1.0f);
    shot("17_uploaded");
    doit("close replays", []() {
        if (auto* p = dynamic_cast<tjfui::Panel*>(topModal())) p->dismiss();
    });
    waitModal(false);

    // Watch the recorded run: same physics, so it must land exactly where the run was.
    doit("open replays again", []() { replays::ReplayPanel::show(g_driver->runLevel); });
    waitModal(true);
    wait(1.5f);
    doit("watch own run", []() { key(EventKeyboard::KeyCode::KEY_ENTER); });
    add("watching own run", []() { return inGameplay(); }, 30.0f);
    add("step 600 replayed", []() { return g_driver->gotReplayed; }, 30.0f);
    shot("18_watch_own_run");
    doit("compare", []() {
        const float dx = g_driver->recorded.x - g_driver->replayed.x, dy = g_driver->recorded.y - g_driver->replayed.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        say("own-run replay vs recording at step 600: distance " + std::to_string(dist) + " m" +
            (dist < 1e-4f ? " (exact)" : " (DIFFERENT)"));
        if (dist >= 1e-4f) g_driver->failures++;
        replays::setTestStepObserver(-1, nullptr);
        int a = 1;
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("gameplayMenuAction", &a);
    });
    add("back in browser", []() { return browser() != nullptr && !inGameplay(); });
    wait(1.0f);

    // Publish a browser-format level.
    doit("publish panel", []() {
        account::PublishRequest r;
        r.xml = FileUtils::getInstance()->getStringFromFile(
            FileUtils::getInstance()->getWritablePath() + "online/publish_test.xml");
        if (r.xml.empty()) {
            const char* sample = std::getenv("OW_TJF_TEST_LEVEL");
            if (sample) r.xml = FileUtils::getInstance()->getStringFromFile(sample);
        }
        r.name = "Mock Publish Test";
        r.comment = "uploaded by the OpenWheels test driver to the local mock";
        account::publishLevel(r);
    });
    waitModal(true);
    wait(0.5f);
    shot("19_publish");
    tapButton("PUBLISH", true);
    wait(0.6f);
    shot("20_publish_confirm");
    confirmWindow();
    add("published", []() { return topModal() == nullptr; }, 20.0f);
    wait(0.5f);
    shot("21_published");
    tapNamed("tjf_account");
    waitModal(true);
    tapButton("MY LEVELS", true);
    add("my levels listed", []() {
        const BrowserState& st = OnlineLevelBrowser::state();
        return st.special == 2 && st.loaded;
    });
    wait(1.0f);
    shot("22_my_levels");
    tapNamed("tjf_account");
    waitModal(true);
    tapButton("LOG OUT", true);
    add("logged out", []() { return !account::TjfAccount::get()->loggedIn(); });
    wait(1.0f);
    shot("23_logged_out");

    Director::getInstance()->getScheduler()->schedule([](float dt) { tick(dt); }, g_driver, 0.1f, false, "tjf_test");
}

}  // namespace online
