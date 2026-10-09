#pragma once
// PAD (PC addition): remappable game controller bindings (QoL page "controller"). Every action has
// up to three inputs (src/input/Gamepad.h codes). Two groups: the driving actions (pressed through
// virtual fingers like the keyboard's, input/ControlBridge.h) and the menu actions (input/MenuFocus.h:
// select the highlighted button, go back). An input is bound once per group, so A can be both
// "primary action" and "menu: select".
// Persisted in UserDefault under "qol_pad_<action>" ("code,code,code").

#include <string>
#include <vector>

namespace qol {

enum class PadAction
{
    Accelerate = 0,  // control bit 0x01 (ejected: superman)
    Reverse,         // 0x02 (ejected: tuck)
    LeanBack,        // 0x08 (ejected: pushup)
    LeanForward,     // 0x04 (ejected: arch)
    Special,         // 0x10 primary action (ejected: grab)
    Shift,           // 0x20 restored characters / user vehicles: first extra action
    Ctrl,            // 0x40 second extra action
    Eject,           // 0x80
    Pause,           // pause button; resumes when paused
    Restart,         // reset button (after death)
    MenuSelect,      // menus: press the highlighted button
    MenuBack,        // menus: back / cancel / resume
    Count
};

const int kPadSlots = 3;

bool padActionIsMenu(PadAction action);
// "accelerate", ..., "menu: select".
const char* padActionName(PadAction action);
// Bound inputs of `action` (slot order, kPadSlots entries, pad::kNoInput for an empty slot).
std::vector<int> padInputsFor(PadAction action);
// Binds `code` to slot `slot` of `action` (pad::kNoInput clears it); the input is taken away from
// the other actions of the same group first.
void setPadInput(PadAction action, int slot, int code);
void resetPadBindings();
// True when any input of `action` is held / was pressed this frame (input/Gamepad.h).
bool padActionHeld(PadAction action);
bool padActionPressed(PadAction action);

// "a / d-pad up" ("-" when unbound); hint: in capitals.
std::string padInputsText(PadAction action);
std::string padHint(PadAction action);

}  // namespace qol
