#pragma once
// PAD (PC addition): controller rumble from the game's own events (not part of the original).
//
// Every crash, hit, broken bone, lost limb and explosion in Happy Wheels plays a sound, so the
// rumble follows the sounds the level plays: SoundController reports them here and their name
// picks the strength (explosions strongest, then the character's injuries, smashes, and light
// hits), faded by the distance from the camera. Only while driving (not in menus or paused).
// Strength and on / off: QoL "controller" page (qol::rumbleLevel). Played by input/Gamepad.h's
// pad::rumble on every connected controller (Windows XInput, Linux force feedback, Android
// controller or handheld vibrator).

#include <string>

namespace openwheels {
namespace haptics {

// A sound started; `distance` in metres from the listener (0 for one-shot sounds).
void onSound(const std::string& name, float distance);
// Set every frame by input/PadInput.cpp.
void setGameplayActive(bool active);

}  // namespace haptics
}  // namespace openwheels
