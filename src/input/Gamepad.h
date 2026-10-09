#pragma once
// PAD (PC addition): game controller support (not part of the original game), on every platform.
//
// Layers:
//   * a platform provider (src/platform/<os>/...Gamepad.cpp) reports, once per frame, the state of
//     every connected controller: Windows / Linux / macOS poll GLFW's joysticks, Android forwards
//     the activity's gamepad key and motion events (any Android controller, including the built-in
//     controls of handhelds such as the AYN Odin / Thor), iOS polls GCController through cocos2d-x;
//   * this core turns the reports into one set of pressed inputs (all controllers together), with
//     press / release edges, and keeps the rumble timing;
//   * consumers: the remappable gameplay bindings (qol/PadBindings.h, driven through the same
//     virtual fingers as the keyboard, input/ControlBridge.h), menu focus navigation
//     (input/MenuFocus.h), the controller page's "press a button" capture, haptics
//     (input/Haptics.h) and tilt steering (input/Tilt.h). See input/PadInput.cpp.
//
// Inputs are int codes. Controllers of a known layout (every Android controller, XInput pads,
// PlayStation pads, Linux xpad-style pads, iOS / macOS MFi pads) report the standard controls
// below, named after the Xbox layout (kA = the bottom face button, ...). Controllers of an unknown
// layout report raw codes (kRawButton + n, kRawAxisPlus / kRawAxisMinus + n), which can be bound
// on the controller page like any other input.

#include <functional>
#include <string>
#include <vector>

namespace openwheels {
namespace pad {

enum Control
{
    kA = 0,  // bottom face button (PlayStation cross, Nintendo B)
    kB,      // right face button
    kX,      // left face button
    kY,      // top face button
    kLB,
    kRB,
    kLT,     // analog or digital triggers
    kRT,
    kBack,   // select / view / share
    kStart,  // start / menu / options
    kGuide,
    kLS,     // stick clicks
    kRS,
    kDUp,
    kDDown,
    kDLeft,
    kDRight,
    kLUp,    // left stick directions (0..1 each)
    kLDown,
    kLLeft,
    kLRight,
    kRUp,    // right stick directions
    kRDown,
    kRLeft,
    kRRight,
    kControlCount
};

const int kNoInput = -1;
const int kRawButton = 1000;     // + button index
const int kRawAxisPlus = 2000;   // + axis index, pushed towards +1
const int kRawAxisMinus = 3000;  // + axis index, pushed towards -1
const int kRawLimit = 1000;      // indices below this

// One controller as a provider reports it for a frame. Values are 0..1 (sticks split in four
// directions, triggers 0 at rest).
struct State
{
    int id = 0;
    std::string name;
    float control[kControlCount] = {};
    std::vector<std::pair<int, float>> raw;  // raw codes of unknown layouts and extra buttons
};

// ---- platform providers ------------------------------------------------------------------------
// Every platform defines this (desktop: GLFW, Android: JNI, iOS: GCController); pad::install()
// calls it once. It registers the poll / rumble functions below.
void installPlatformProvider();
// Fills `states` with the connected controllers (called once per frame on the GL thread).
void setPoll(std::function<void(std::vector<State>& states)> poll);
// Starts (strength 0..1, for `seconds`) or stops (strength 0) the controllers' rumble motors.
// Optional: platforms without rumble don't set it.
void setRumbleOutput(std::function<void(float strength, float seconds)> rumble);

// ---- core --------------------------------------------------------------------------------------
// Installs the per-frame controller processing (idempotent; AppDelegate calls it). Defined in
// input/PadInput.cpp, which runs update() and then the consumers every frame.
void install();
// Polls the provider and computes this frame's held inputs and edges; ends finished rumbles.
void update(float dt);

// Connected controllers, by name (empty when none).
std::vector<std::string> connectedNames();
bool anyConnected();
// True when a controller was the last thing used (a press since the last mouse / touch / key).
bool lastInputWasPad();
void noteOtherInput();  // a mouse, touch or keyboard input happened
// Set while the controller code injects touches / keys of its own (not "other input").
void setSyntheticInput(bool on);
bool syntheticInput();

// Inputs held this frame (all controllers together), and this frame's presses / releases.
bool held(int code);
bool pressed(int code);
bool released(int code);
// Analog value 0..1 of an input (largest over the controllers).
float value(int code);
// Left / right stick as -1..1 vectors (x right, y up), d-pad not included.
void leftStick(float* x, float* y);
void rightStick(float* x, float* y);
const std::vector<int>& heldInputs();

// Remap capture: the next input pressed (inputs held when the capture starts must be released
// first) is passed to `done`; nothing else sees controller input meanwhile. `done(kNoInput)`
// when cancelCapture() is called.
void beginCapture(std::function<void(int code)> done);
void cancelCapture();
bool capturing();

// Rumble: plays on every connected controller (or, on Android handhelds, the device) for
// `seconds`; a weaker request than the one playing is ignored. Scaled by the rumble setting.
void rumble(float strength, float seconds);
bool rumbleSupported();
// A controller is connected, or the phone vibration option is on.
bool hapticsWanted();

// "a", "lt", "d-pad up", "left stick up", "button 12", "axis 3+".
std::string inputName(int code);

}  // namespace pad
}  // namespace openwheels
