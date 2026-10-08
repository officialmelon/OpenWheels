// PC keyboard bridge (Windows, Linux, macOS): define OW_WITH_PC_LAYER (see CMakeLists.txt).
#ifdef OW_WITH_PC_LAYER
#include "platform/desktop/PCInput.h"

#include <map>
#include <vector>

#include "cocos2d.h"
#include "GameplayBtn.h"
#include "GameplayControls.h"
#include "online/vehicles/UserVehicle.h"  // ONLINE (PC addition)
#include "qol/KeyBindings.h"  // QOL (PC addition)
#include "qol/QoL.h"  // QOL (PC addition)

USING_NS_CC;

namespace openwheels {
namespace pc {
namespace {

enum Target { kStateBit, kPause, kReset };

struct Binding {
    Target target;
    unsigned int bit;  // state bit for kStateBit
};

// QOL (PC addition): the keys come from the remappable bindings (src/qol/KeyBindings.h; the
// defaults are the original arrows / WASD, space, Z, shift / ctrl, Esc / P and R).
bool bindingFor(EventKeyboard::KeyCode key, Binding* binding) {
    qol::KeyAction action;
    if (!qol::actionForKey(key, &action)) return false;
    switch (action) {
    case qol::KeyAction::Accelerate: *binding = {kStateBit, 0x01}; return true;
    case qol::KeyAction::Reverse: *binding = {kStateBit, 0x02}; return true;
    case qol::KeyAction::LeanForward: *binding = {kStateBit, 0x04}; return true;
    case qol::KeyAction::LeanBack: *binding = {kStateBit, 0x08}; return true;
    case qol::KeyAction::Special: *binding = {kStateBit, 0x10}; return true;
    // RESTORED (PC addition): the restored characters' extra buttons (Flash shift / ctrl).
    case qol::KeyAction::Shift: *binding = {kStateBit, 0x20}; return true;
    case qol::KeyAction::Ctrl: *binding = {kStateBit, 0x40}; return true;
    case qol::KeyAction::Eject: *binding = {kStateBit, 0x80}; return true;
    case qol::KeyAction::Pause: *binding = {kPause, 0}; return true;
    case qol::KeyAction::Restart: *binding = {kReset, 0}; return true;
    default: return false;  // fullscreen: main.cpp
    }
}

// Virtual finger ids: far away from the mouse's id 0.
intptr_t touchIdFor(EventKeyboard::KeyCode key) { return 1000 + static_cast<intptr_t>(key); }

// A bound key that is held down. While it holds a virtual finger, `btn` (retained) is the button
// the finger went down on and `at` where (frame pixels). A held key without a finger (no matching
// button yet, controls hidden, layout rebuilt) is pressed again as soon as its button is there:
// see tick().
struct HeldKey {
    Binding binding;
    bool down = false;
    Vec2 at;
    GameplayBtn* btn = nullptr;
};

std::map<EventKeyboard::KeyCode, HeldKey>& heldKeys() {
    static std::map<EventKeyboard::KeyCode, HeldKey> m;
    return m;
}

// Breadth first: the controls sit at scene -> Gameplay -> controls, next to the session's large
// subtree, and tick() looks them up every frame while a key is held.
GameplayControls* findControls(Node* root) {
    if (!root) return nullptr;
    std::vector<Node*> level{root}, next;
    while (!level.empty()) {
        for (Node* node : level) {
            if (auto c = dynamic_cast<GameplayControls*>(node)) return c;
            for (auto child : node->getChildren()) next.push_back(child);
        }
        level.swap(next);
        next.clear();
    }
    return nullptr;
}

GameplayControls* runningControls() { return findControls(Director::getInstance()->getRunningScene()); }

// The GameplayBtn of `controls` that the binding refers to (enabled and visible), or null.
// Buttons of hidden touch controls (GameplayBtn::getKeyOnly) are visible at opacity 0.
GameplayBtn* findButton(GameplayControls* controls, const Binding& b) {
    for (auto child : controls->getChildren()) {
        auto btn = dynamic_cast<GameplayBtn*>(child);
        if (!btn || !btn->isVisible() || !btn->getEnabled()) continue;
        if (b.target == kStateBit && btn->getStateValue() == b.bit) return btn;
        if (b.target == kPause && btn->getStateValue() == 0 && btn->getTag() == 0) return btn;
        if (b.target == kReset && btn->getStateValue() == 0 && btn->getTag() == 1) return btn;
    }
    return nullptr;
}

// World point -> the frame-pixel coordinates GLView::handleTouches* expects (its exact inverse).
void worldToScreen(const Vec2& world, float* x, float* y) {
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    const Vec2 ui = director->convertToUI(world);
    const Rect vp = glview->getViewPortRect();
    *x = ui.x * glview->getScaleX() + vp.origin.x;
    *y = ui.y * glview->getScaleY() + vp.origin.y;
}

// Puts the key's finger down on its button, if the controls are on screen and have one. Pause
// hides the controls (GameplayControls::setHidden) and drops their touch listener: no finger then,
// so it can't land on the pause menu.
void fingerDown(EventKeyboard::KeyCode key, HeldKey& held, GameplayControls* controls) {
    if (held.down || !controls || !controls->isVisible()) return;
    auto btn = findButton(controls, held.binding);
    if (!btn) return;
    const Rect hit = btn->getHitArea();  // in the controls layer's space
    const Vec2 centre(hit.getMidX(), hit.getMidY());
    float x, y;
    worldToScreen(controls->convertToWorldSpace(centre), &x, &y);
    intptr_t id = touchIdFor(key);
    held.down = true;
    held.at = Vec2(x, y);
    held.btn = btn;
    btn->retain();  // compared by address in tick(): keep it from being reused
    qol::setKeyboardTouch(true);  // QOL (PC addition): lets hidden touch controls take it
    Director::getInstance()->getOpenGLView()->handleTouchesBegin(1, &id, &x, &y);
    qol::setKeyboardTouch(false);
}

// Lifts the key's finger. `tap`: where it went down, so tap-style buttons (pause, reset) fire;
// otherwise far off screen, so a finger moved to a new layout never fires one by accident.
void fingerUp(EventKeyboard::KeyCode key, HeldKey& held, bool tap) {
    if (!held.down) return;
    float x = tap ? held.at.x : -100000.0f, y = tap ? held.at.y : -100000.0f;
    GameplayBtn* btn = held.btn;
    held.down = false;
    held.btn = nullptr;
    intptr_t id = touchIdFor(key);
    Director::getInstance()->getOpenGLView()->handleTouchesEnd(1, &id, &x, &y);
    btn->release();
}

void press(EventKeyboard::KeyCode key, const Binding& b) {
    if (heldKeys().count(key)) return;
    HeldKey& held = heldKeys()[key];
    held.binding = b;
    fingerDown(key, held, runningControls());
}

void release(EventKeyboard::KeyCode key) {
    auto it = heldKeys().find(key);
    if (it == heldKeys().end()) return;
    // Erased first: the tap may dispatch "gameplayControlsAction" (pause / reset) synchronously.
    HeldKey held = it->second;
    heldKeys().erase(it);
    fingerUp(key, held, true);
}

// Every frame while keys are held: a finger whose button went away (eject / death / victory
// layouts, a restart, pause) is lifted and the key pressed again on the current layout, so e.g.
// holding space through an eject grabs without pressing it again, and Esc / R held while the
// pause / reset buttons are not there yet work once they appear.
void tick(float) {
    if (heldKeys().empty()) return;
    GameplayControls* controls = runningControls();
    for (auto& entry : heldKeys()) {
        HeldKey& held = entry.second;
        if (held.down) {
            const bool stale = !controls || !controls->isVisible() || held.btn->getParent() != controls ||
                               findButton(controls, held.binding) != held.btn;
            if (stale) fingerUp(entry.first, held, false);
        }
        if (!held.down) fingerDown(entry.first, held, controls);
    }
}

// ONLINE (PC addition): keys with no on-screen button, read by browser levels only (Gameplay
// ORs online::pcExtraControlBits() into the control byte): Shift 0x20 / Ctrl 0x40 = the browser
// game's secondary actions of user vehicles, Z 0x80 = eject from a user vehicle, and Space 0x10
// so a user vehicle's space action never depends on the button layout of the moment.
void onlineExtraKey(EventKeyboard::KeyCode key, bool down) {
    qol::KeyAction action;
    if (!qol::actionForKey(key, &action)) return;
    switch (action) {
    case qol::KeyAction::Special:
        online::setPcExtraKey(0x10, down);
        break;
    case qol::KeyAction::Shift:
        online::setPcExtraKey(0x20, down);
        break;
    case qol::KeyAction::Ctrl:
        online::setPcExtraKey(0x40, down);
        break;
    case qol::KeyAction::Eject:
        online::setPcExtraKey(0x80, down);
        break;
    default:
        break;
    }
}

}  // namespace

void installKeyboardControls() {
    auto listener = EventListenerKeyboard::create();
    listener->onKeyPressed = [](EventKeyboard::KeyCode key, Event*) {
        onlineExtraKey(key, true);  // ONLINE (PC addition)
        Binding binding;
        if (bindingFor(key, &binding)) press(key, binding);
    };
    listener->onKeyReleased = [](EventKeyboard::KeyCode key, Event*) {
        onlineExtraKey(key, false);  // ONLINE (PC addition)
        release(key);
    };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(listener, 1);
    static int s_tickTarget = 0;
    Director::getInstance()->getScheduler()->schedule(tick, &s_tickTarget, 0.0f, false, "ow_pc_keys");
}

}  // namespace pc
}  // namespace openwheels
#endif  // OW_WITH_PC_LAYER
