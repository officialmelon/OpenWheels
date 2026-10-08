#pragma once
// Remappable keyboard controls (PC addition, QoL page "controls"). Every action has up to two
// keys; the defaults are the keyboard bridge's original keys (src/platform/desktop/PCInput.cpp).
// Persisted in UserDefault under "qol_keys_<action>" ("code,code", cocos2d KeyCode values).
//
// Android: the Cocos2dxGLSurfaceView forwards only back / menu / d-pad / enter / play-pause to the
// engine and the keyboard bridge is Win32-only, so the bindings apply to the desktop build.

#include <string>
#include <vector>

#include "base/CCEventKeyboard.h"

namespace cocos2d {
class Label;
}

namespace qol {

enum class KeyAction
{
    Accelerate = 0,  // control bit 0x01 (ejected: superman)
    Reverse,         // 0x02 (ejected: tuck)
    LeanBack,        // 0x08 (ejected: pushup)
    LeanForward,     // 0x04 (ejected: arch)
    Special,         // 0x10 primary action (ejected: grab)
    Shift,           // 0x20 restored characters / user vehicles: first extra action
    Ctrl,            // 0x40 second extra action
    Eject,           // 0x80
    Pause,
    Restart,
    Fullscreen,
    Count
};

const int kKeySlots = 2;

// "accelerate", "lean back", ...
const char* keyActionName(KeyAction action);
// Bound keys of `action` (slot order, at most kKeySlots; KEY_NONE for an empty slot).
std::vector<cocos2d::EventKeyboard::KeyCode> keysFor(KeyAction action);
// Binds `key` to slot `slot` of `action` (KEY_NONE clears it). The key is taken away from every
// other action and slot first, so a key always does one thing.
void setKey(KeyAction action, int slot, cocos2d::EventKeyboard::KeyCode key);
void resetKeyBindings();
// The action bound to `key`; false when none.
bool actionForKey(cocos2d::EventKeyboard::KeyCode key, KeyAction* action);
bool keyIs(cocos2d::EventKeyboard::KeyCode key, KeyAction action);

// Lower-case key name for the menus ("up", "w", "left shift", "f11"...).
std::string keyName(cocos2d::EventKeyboard::KeyCode key);
// "up / w" ("-" when unbound).
std::string keysText(KeyAction action);
// In-game key hint: keysText in capitals ("UP / W").
std::string keyHint(KeyAction action);
// A small yellow, outlined label with keyHint(action) (reset button, tutorial arrows).
cocos2d::Label* createKeyHintLabel(KeyAction action, float fontSize);

}  // namespace qol
