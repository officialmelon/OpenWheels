// ONLINE (PC addition): see TjfTestDriver.h.
#include "online/account/TjfTestDriver.h"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <functional>
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
