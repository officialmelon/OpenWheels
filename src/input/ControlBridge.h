#pragma once
// Virtual fingers on the gameplay buttons (not part of the original game), shared by the keyboard
// bridge (platform/desktop/PCInput) and the game controller bindings (input/PadInput.cpp).
//
// The original is touch-only: GameplayControls hit-tests touches against its GameplayBtn sprites
// and the per-frame control byte is the sum of the held buttons' state bits. To keep that logic
// 100% original, a held key / controller action is a *virtual finger*: pressing it injects a touch
// (through GLView, exactly like a real touch) at the centre of the on-screen button carrying the
// mapped bit; releasing it lifts that finger. A held finger follows layout changes (eject, death,
// restart, pause): once per frame a finger whose button went away is lifted off screen and put
// down on the current button. With the QoL touch controls hidden the buttons are transparent and
// only take these virtual fingers (qol::keyboardTouch).

#include <cstdint>

class GameplayControls;

namespace openwheels {
namespace controls {

enum Target { kStateBit, kPause, kReset };

struct Binding
{
    Target target;
    unsigned int bit;  // state bit for kStateBit
};

// `finger` identifies the source (a key, a controller action); it is the injected touch's id, so
// keep sources apart (keyboard: 1000 + key code, controller: 5000 + action).
void press(intptr_t finger, const Binding& binding);
void release(intptr_t finger);
bool isPressed(intptr_t finger);

// The running scene's GameplayControls (breadth-first search), or null.
GameplayControls* runningControls();
// GameplayControls on screen and visible (playing, not paused / in a menu).
bool gameplayActive();

}  // namespace controls
}  // namespace openwheels
