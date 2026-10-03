// Disabled until the reconstructed game links: define OW_WITH_PC_LAYER (see main.cpp).
#ifdef OW_WITH_PC_LAYER
#include "platform/win32/PCInput.h"

#include <map>

#include "cocos2d.h"
#include "GameplayBtn.h"
#include "GameplayControls.h"

USING_NS_CC;

namespace openwheels {
namespace pc {
namespace {

enum Target { kStateBit, kPause, kReset };

struct Binding {
    Target target;
    unsigned int bit;  // state bit for kStateBit
};

const std::map<EventKeyboard::KeyCode, Binding>& bindings() {
    using K = EventKeyboard::KeyCode;
    static const std::map<K, Binding> m = {
        {K::KEY_UP_ARROW, {kStateBit, 0x01}},    {K::KEY_W, {kStateBit, 0x01}},
        {K::KEY_DOWN_ARROW, {kStateBit, 0x02}},  {K::KEY_S, {kStateBit, 0x02}},
        {K::KEY_RIGHT_ARROW, {kStateBit, 0x04}}, {K::KEY_D, {kStateBit, 0x04}},
        {K::KEY_LEFT_ARROW, {kStateBit, 0x08}},  {K::KEY_A, {kStateBit, 0x08}},
        {K::KEY_SPACE, {kStateBit, 0x10}},
        {K::KEY_Z, {kStateBit, 0x80}},
        {K::KEY_ESCAPE, {kPause, 0}},            {K::KEY_P, {kPause, 0}},
        {K::KEY_R, {kReset, 0}},
    };
    return m;
}

// Virtual finger ids: far away from the mouse's id 0.
intptr_t touchIdFor(EventKeyboard::KeyCode key) { return 1000 + static_cast<intptr_t>(key); }

// Keys currently holding a virtual finger, and where it went down (frame pixels).
std::map<EventKeyboard::KeyCode, Vec2>& activeKeys() {
    static std::map<EventKeyboard::KeyCode, Vec2> m;
    return m;
}

GameplayControls* findControls(Node* node) {
    if (!node) return nullptr;
    if (auto c = dynamic_cast<GameplayControls*>(node)) return c;
    for (auto child : node->getChildren()) {
        if (auto c = findControls(child)) return c;
    }
    return nullptr;
}

// The GameplayBtn of `controls` that the binding refers to (enabled and visible), or null.
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

void press(EventKeyboard::KeyCode key, const Binding& b) {
    if (activeKeys().count(key)) return;
    auto scene = Director::getInstance()->getRunningScene();
    auto controls = findControls(scene);
    if (!controls) return;
    auto btn = findButton(controls, b);
    if (!btn) return;
    const Rect hit = btn->getHitArea();  // in the controls layer's space
    const Vec2 centre(hit.getMidX(), hit.getMidY());
    float x, y;
    worldToScreen(controls->convertToWorldSpace(centre), &x, &y);
    intptr_t id = touchIdFor(key);
    Director::getInstance()->getOpenGLView()->handleTouchesBegin(1, &id, &x, &y);
    activeKeys()[key] = Vec2(x, y);
}

void release(EventKeyboard::KeyCode key) {
    auto it = activeKeys().find(key);
    if (it == activeKeys().end()) return;
    // The finger lifts where it went down: tap-style buttons (pause, reset) only fire when the
    // touch ends inside them.
    float x = it->second.x, y = it->second.y;
    activeKeys().erase(it);
    intptr_t id = touchIdFor(key);
    Director::getInstance()->getOpenGLView()->handleTouchesEnd(1, &id, &x, &y);
}

}  // namespace

void installKeyboardControls() {
    auto listener = EventListenerKeyboard::create();
    listener->onKeyPressed = [](EventKeyboard::KeyCode key, Event*) {
        auto it = bindings().find(key);
        if (it != bindings().end()) press(key, it->second);
    };
    listener->onKeyReleased = [](EventKeyboard::KeyCode key, Event*) { release(key); };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(listener, 1);
}

}  // namespace pc
}  // namespace openwheels
#endif  // OW_WITH_PC_LAYER
