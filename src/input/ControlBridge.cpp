// Virtual fingers on the gameplay buttons, see ControlBridge.h.
#include "input/ControlBridge.h"

#include <map>
#include <vector>

#include "cocos2d.h"
#include "GameplayBtn.h"
#include "GameplayControls.h"
#include "qol/QoL.h"  // QOL (PC addition)

USING_NS_CC;

namespace openwheels {
namespace controls {
namespace {

// A source that is held down. While it holds a virtual finger, `btn` (retained) is the button the
// finger went down on and `at` where (frame pixels). A held source without a finger (no matching
// button yet, controls hidden, layout rebuilt) is pressed again as soon as its button is there:
// see tick().
struct Held
{
    Binding binding;
    bool down = false;
    Vec2 at;
    GameplayBtn* btn = nullptr;
};

std::map<intptr_t, Held>& heldFingers()
{
    static std::map<intptr_t, Held> m;
    return m;
}

// Breadth first: the controls sit at scene -> Gameplay -> controls, next to the session's large
// subtree, and tick() looks them up every frame while a finger is held.
GameplayControls* findControls(Node* root)
{
    if (!root) return nullptr;
    std::vector<Node*> level{root}, next;
    while (!level.empty())
    {
        for (Node* node : level)
        {
            if (auto c = dynamic_cast<GameplayControls*>(node)) return c;
            for (auto child : node->getChildren()) next.push_back(child);
        }
        level.swap(next);
        next.clear();
    }
    return nullptr;
}

// The GameplayBtn of `controls` that the binding refers to (enabled and visible), or null.
// Buttons of hidden touch controls (GameplayBtn::getKeyOnly) are visible at opacity 0.
GameplayBtn* findButton(GameplayControls* controls, const Binding& b)
{
    for (auto child : controls->getChildren())
    {
        auto btn = dynamic_cast<GameplayBtn*>(child);
        if (!btn || !btn->isVisible() || !btn->getEnabled()) continue;
        if (b.target == kStateBit && btn->getStateValue() == b.bit) return btn;
        if (b.target == kPause && btn->getStateValue() == 0 && btn->getTag() == 0) return btn;
        if (b.target == kReset && btn->getStateValue() == 0 && btn->getTag() == 1) return btn;
    }
    return nullptr;
}

// World point -> the frame-pixel coordinates GLView::handleTouches* expects (its exact inverse).
void worldToScreen(const Vec2& world, float* x, float* y)
{
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    const Vec2 ui = director->convertToUI(world);
    const Rect vp = glview->getViewPortRect();
    *x = ui.x * glview->getScaleX() + vp.origin.x;
    *y = ui.y * glview->getScaleY() + vp.origin.y;
}

// Puts the finger down on its button, if the controls are on screen and have one. Pause hides
// the controls (GameplayControls::setHidden) and drops their touch listener: no finger then, so
// it can't land on the pause menu.
void fingerDown(intptr_t id, Held& held, GameplayControls* controls)
{
    if (held.down || !controls || !controls->isVisible()) return;
    auto btn = findButton(controls, held.binding);
    if (!btn) return;
    const Rect hit = btn->getHitArea();  // in the controls layer's space
    const Vec2 centre(hit.getMidX(), hit.getMidY());
    float x, y;
    worldToScreen(controls->convertToWorldSpace(centre), &x, &y);
    held.down = true;
    held.at = Vec2(x, y);
    held.btn = btn;
    btn->retain();  // compared by address in tick(): keep it from being reused
    qol::setKeyboardTouch(true);  // QOL (PC addition): lets hidden touch controls take it
    Director::getInstance()->getOpenGLView()->handleTouchesBegin(1, &id, &x, &y);
    qol::setKeyboardTouch(false);
}

// Lifts the finger. `tap`: where it went down, so tap-style buttons (pause, reset) fire;
// otherwise far off screen, so a finger moved to a new layout never fires one by accident.
void fingerUp(intptr_t id, Held& held, bool tap)
{
    if (!held.down) return;
    float x = tap ? held.at.x : -100000.0f, y = tap ? held.at.y : -100000.0f;
    GameplayBtn* btn = held.btn;
    held.down = false;
    held.btn = nullptr;
    Director::getInstance()->getOpenGLView()->handleTouchesEnd(1, &id, &x, &y);
    btn->release();
}

// Every frame while fingers are held: a finger whose button went away (eject / death / victory
// layouts, a restart, pause) is lifted and pressed again on the current layout, so e.g. holding
// space through an eject grabs without pressing it again, and Esc / R held while the pause /
// reset buttons are not there yet work once they appear.
void tick(float)
{
    if (heldFingers().empty()) return;
    GameplayControls* controls = runningControls();
    for (auto& entry : heldFingers())
    {
        Held& held = entry.second;
        if (held.down)
        {
            const bool stale = !controls || !controls->isVisible() || held.btn->getParent() != controls ||
                               findButton(controls, held.binding) != held.btn;
            if (stale) fingerUp(entry.first, held, false);
        }
        if (!held.down) fingerDown(entry.first, held, controls);
    }
}

void installTick()
{
    static bool installed = false;
    if (installed) return;
    installed = true;
    static int s_tickTarget = 0;
    Director::getInstance()->getScheduler()->schedule(tick, &s_tickTarget, 0.0f, false, "ow_virtual_fingers");
}

}  // namespace

void press(intptr_t finger, const Binding& binding)
{
    installTick();
    if (heldFingers().count(finger)) return;
    Held& held = heldFingers()[finger];
    held.binding = binding;
    fingerDown(finger, held, runningControls());
}

void release(intptr_t finger)
{
    auto it = heldFingers().find(finger);
    if (it == heldFingers().end()) return;
    // Erased first: the tap may dispatch "gameplayControlsAction" (pause / reset) synchronously.
    Held held = it->second;
    heldFingers().erase(it);
    fingerUp(finger, held, true);
}

bool isPressed(intptr_t finger) { return heldFingers().count(finger) != 0; }

GameplayControls* runningControls() { return findControls(Director::getInstance()->getRunningScene()); }

bool gameplayActive()
{
    GameplayControls* controls = runningControls();
    return controls && controls->isVisible();
}

}  // namespace controls
}  // namespace openwheels
