#pragma once
// PC-only input bridge (not part of the original game).
//
// Keyboard keys are turned into *virtual fingers* on the on-screen gameplay buttons (see
// src/input/ControlBridge.h, shared with the game controller bindings): pressing a key injects a
// touch at the centre of the button carrying the mapped bit, releasing it lifts that finger.
// Mouse clicks already arrive as touches.
//
//   Up / W      0x01  forward          (ejected: superman)
//   Down / S    0x02  back             (ejected: tuck)
//   Right / D   0x04  lean forward     (ejected: arch)
//   Left / A    0x08  lean back        (ejected: pushup)
//   Space       0x10  special          (ejected: grab)
//   Z           0x80  eject
//   Esc / P     pause button,   R  reset button (after death)
//   Shift 0x20, Ctrl 0x40: the restored characters' / user vehicles' extra actions
//
// These are the defaults: the keys are remappable on the QoL "controls" page
// (src/qol/KeyBindings.h).

namespace openwheels {
namespace pc {

// Registers the keyboard listener on the Director's event dispatcher (scene independent).
void installKeyboardControls();

}  // namespace pc
}  // namespace openwheels
