#pragma once
// PAD (PC addition): tilt steering (not part of the original game). On phones, tablets and
// handhelds with a motion sensor (Android: the gravity sensor, fused with the gyroscope where the
// device has one; iOS: the accelerometer), turning the device like a steering wheel leans: left
// side down = lean back, right side down = lean forward. The game's controls are on / off, so the
// lean is pressed once the turn passes the QoL threshold (qol::tiltSteering) and let go a little
// before it comes back. It adds to the touch / controller controls, it doesn't replace them.

namespace openwheels {
namespace tilt {

// Platform side: the device's roll in screen space, radians, counter-clockwise positive (0 =
// held level). Any thread.
void setRoll(float radians);
// Platform side: start / stop the sensor (Android: AppActivity, iOS: cocos2d-x's accelerometer).
// Every platform with a sensor defines it; others use a no-op.
void enableSensor(bool on);

// Once per frame (input/PadInput.cpp): starts the sensor while the option is on and the game is
// being driven, and works out the lean.
void update(bool gameplayActive);
bool leanBack();
bool leanForward();

}  // namespace tilt
}  // namespace openwheels
