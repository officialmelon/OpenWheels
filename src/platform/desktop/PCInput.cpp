// PC keyboard bridge (Windows, Linux, macOS): define OW_WITH_PC_LAYER (see CMakeLists.txt).
#ifdef OW_WITH_PC_LAYER
#include "platform/desktop/PCInput.h"

#include "cocos2d.h"
#include "input/ControlBridge.h"
#include "input/Gamepad.h"  // PAD (PC addition)
#include "online/vehicles/UserVehicle.h"  // ONLINE (PC addition)
#include "qol/KeyBindings.h"  // QOL (PC addition)

USING_NS_CC;

namespace openwheels {
namespace pc {
namespace {

using controls::Binding;
using controls::kPause;
using controls::kReset;
using controls::kStateBit;

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

// Virtual finger ids (input/ControlBridge.h): far away from the mouse's id 0.
intptr_t touchIdFor(EventKeyboard::KeyCode key) { return 1000 + static_cast<intptr_t>(key); }

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
        pad::noteOtherInput();  // PAD (PC addition): key hints show keys again
        onlineExtraKey(key, true);  // ONLINE (PC addition)
        Binding binding;
        if (bindingFor(key, &binding)) controls::press(touchIdFor(key), binding);
    };
    listener->onKeyReleased = [](EventKeyboard::KeyCode key, Event*) {
        onlineExtraKey(key, false);  // ONLINE (PC addition)
        controls::release(touchIdFor(key));
    };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(listener, 1);
}

}  // namespace pc
}  // namespace openwheels
#endif  // OW_WITH_PC_LAYER
