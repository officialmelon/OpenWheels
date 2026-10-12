// PAD (PC addition): the per-frame controller processing (see Gamepad.h): driving through the
// remappable bindings and virtual fingers, tilt steering, menu focus navigation and haptics.
#include <cmath>

#include "cocos2d.h"

#include "input/ControlBridge.h"
#include "input/Gamepad.h"
#include "input/Haptics.h"
#include "input/MenuFocus.h"
#include "input/Tilt.h"
#include "online/vehicles/UserVehicle.h"
#include "qol/PadBindings.h"
#include "qol/QoL.h"

USING_NS_CC;

namespace openwheels {
namespace pad {
namespace {

using qol::PadAction;

const int kDriveActions = (int)PadAction::MenuSelect;  // Accelerate .. Restart

controls::Binding bindingFor(PadAction action)
{
    using controls::kPause;
    using controls::kReset;
    using controls::kStateBit;
    switch (action)
    {
    case PadAction::Accelerate: return {kStateBit, 0x01};
    case PadAction::Reverse: return {kStateBit, 0x02};
    case PadAction::LeanForward: return {kStateBit, 0x04};
    case PadAction::LeanBack: return {kStateBit, 0x08};
    case PadAction::Special: return {kStateBit, 0x10};
    case PadAction::Shift: return {kStateBit, 0x20};
    case PadAction::Ctrl: return {kStateBit, 0x40};
    case PadAction::Eject: return {kStateBit, 0x80};
    case PadAction::Pause: return {kPause, 0};
    default: return {kReset, 0};
    }
}

// Virtual finger ids (input/ControlBridge.h): one per action, whatever presses it.
intptr_t fingerFor(int action) { return 5000 + action; }

bool g_blocked[kDriveActions] = {};  // held since before the game was being driven

// Presses / lets go of each driving action's finger. An action only starts while the game is
// being driven: an A held through "play" (menu: select) doesn't fire the primary action when the
// level appears; it has to be let go first.
void drive(bool gameplay)
{
    unsigned char extraBits = 0;
    for (int a = 0; a < kDriveActions; a++)
    {
        const PadAction action = (PadAction)a;
        bool held = qol::padActionHeld(action);
        if (action == PadAction::LeanBack && tilt::leanBack()) held = true;
        if (action == PadAction::LeanForward && tilt::leanForward()) held = true;
        if (!held) g_blocked[a] = false;
        if (!gameplay && held) g_blocked[a] = true;
        const bool want = held && !g_blocked[a] && !capturing();
        const intptr_t finger = fingerFor(a);
        if (want && !controls::isPressed(finger))
        {
            setSyntheticInput(true);
            controls::press(finger, bindingFor(action));
            setSyntheticInput(false);
        }
        else if (!want && controls::isPressed(finger))
        {
            setSyntheticInput(true);
            controls::release(finger);
            setSyntheticInput(false);
        }
        // ONLINE: user vehicles read these bits directly (online/vehicles/UserVehicle.h).
        const controls::Binding b = bindingFor(action);
        if (want && b.target == controls::kStateBit && b.bit >= 0x10) extraBits |= (unsigned char)b.bit;
    }
    online::setPadExtraBits(extraBits);
}

void frame(float dt)
{
    update(dt);
    const bool gameplay = controls::gameplayActive();
    tilt::update(gameplay);
    drive(gameplay);
    haptics::setGameplayActive(gameplay);
    menufocus::update(dt, gameplay);
}

// Mouse, touch and keyboard use hide the menu highlight and bring the key hints back.
void installOtherInputWatch()
{
    EventDispatcher* dispatcher = Director::getInstance()->getEventDispatcher();
    auto touches = EventListenerTouchAllAtOnce::create();
    touches->onTouchesBegan = [](const std::vector<Touch*>&, Event*) {
        if (!syntheticInput() && !qol::keyboardTouch()) noteOtherInput();
    };
    dispatcher->addEventListenerWithFixedPriority(touches, -1000);
    auto mouse = EventListenerMouse::create();
    mouse->onMouseMove = [](EventMouse* e) {
        // Only a real move (not the hover the pointer mode sends, nor a jitter).
        static Vec2 last(-1.0f, -1.0f);
        const Vec2 at(e->getCursorX(), e->getCursorY());
        if (syntheticInput()) return;
        if (last.x >= 0.0f && at.distance(last) > 25.0f) noteOtherInput();
        last = at;
    };
    mouse->onMouseScroll = [](EventMouse*) {
        if (!syntheticInput()) noteOtherInput();
    };
    dispatcher->addEventListenerWithFixedPriority(mouse, -1000);
    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [](EventKeyboard::KeyCode, Event*) {
        if (!syntheticInput()) noteOtherInput();
    };
    dispatcher->addEventListenerWithFixedPriority(keys, -1000);
}

// Scheduler::scheduleUpdate's target (the lambda overload has no priority).
struct Ticker
{
    void update(float dt) { frame(dt); }
};

}  // namespace

void install()
{
    static bool installed = false;
    if (installed) return;
    installed = true;
    installPlatformProvider();
    installOtherInputWatch();
    // Before the game's own per-frame updates (priority 0), so they see this frame's input.
    static Ticker ticker;
    Director::getInstance()->getScheduler()->scheduleUpdate(&ticker, -1000, false);
}

}  // namespace pad
}  // namespace openwheels
