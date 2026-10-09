#pragma once
// Quality-of-life options (not part of the 1:1 reconstruction). Every option defaults to the
// original game's behaviour; game code consults these at points marked "QOL (PC addition):".
// Persisted in UserDefault under "qol_*".

#include <functional>
#include <string>

namespace cocos2d {
class Node;
}

namespace qol {

// Blood rendering, after the browser game's four blood settings (Flash v1.60+):
//   Classic   - the mobile game's particles (original)
//   Streaks   - particles drawn as short streaks along their velocity (browser setting 2)
//   Liquid    - particles merged into flowing liquid blobs (browser setting 3)
//   Realistic - liquid with a glossy, lit surface (browser setting 4)
enum class BloodStyle { Classic = 0, Streaks = 1, Liquid = 2, Realistic = 3 };

BloodStyle bloodStyle();
void setBloodStyle(BloodStyle style);
const char* bloodStyleName(BloodStyle style);

// Session particle budget (original: 2000, Session::canAddEmitter).
int maxParticles();
void setMaxParticles(int count);

// Gameplay camera zoom: 1 = original; < 1 shows more of the level.
float cameraZoom();
void setCameraZoom(float zoom);

bool showFps();
void setShowFps(bool on);

// Treat every campaign level as unlocked (level select).
bool unlockAllLevels();
void setUnlockAllLevels(bool on);

// Gore for Irresponsible Dad's kid. The mobile port shipped him without his gore art; the sheet
// is rebuilt from the player's own browser-game SWF by tools/assets/extract_kid_gore.py into
// generated/<tier>/characters/ (next to the exe on Win32, in the APK's assets on Android).
// On by default when the sheet is there.
bool childGore();
void setChildGore(bool on);
bool childGoreAvailable();
// Adds the kid's gore frames to the SpriteFrameCache; false when the sheet is missing.
bool loadChildGoreSprites();
// The asset size tier AppDelegate picked ("large", "medium", "small" or "tiny").
void setAssetTier(const std::string& tier);

// The asset tier AppDelegate picked for this run.
const std::string& runningAssetTier();

// Texture resolution (asset tier): "" = auto, the original's choice from the screen height and
// Options' "graphics: high-res / low-res"; or "large", "medium", "small", "tiny". Read once at
// start-up (AppDelegate), like the original's graphics option: changes apply on the next start.
std::string textureTier();
void setTextureTier(const std::string& tier);
// AppDelegate hook: replaces the original's tier and its resolution height when overridden.
void overrideAssetTier(std::string* directory, float* resolutionHeight);

// Display frame rate, after the browser game's 30 / 60 FPS option (v1.94): 60 (original) or 30.
// The game logic keeps its 1/60 s ticks: at 30 FPS every drawn frame runs two scheduler ticks of
// half the frame time (Scheduler time scale 0.5 + a second tick after the Director's update), so
// physics, controls, replays and timers see exactly the same sequence of steps.
int frameRate();
void setFrameRate(int fps);  // stores and applies
// Applies a frame rate (AppDelegate: the stored one; --dump-world: its own).
void installFrameRate(int fps);

// Sound effect and music volume (browser v1.98.3 sliders), 0..1, default 1. They scale the
// original's master volume (Options "sound: high / low / off") separately for effects and for
// the menu music.
float effectsVolume();
void setEffectsVolume(float volume);
float musicVolume();
void setMusicVolume(float volume);  // also re-applies the playing music's volume

// Fullscreen (desktop builds only; the platform layer installs the handler).
bool fullscreenSupported();
bool fullscreen();
void setFullscreen(bool on);
void setFullscreenHandler(std::function<void(bool)> handler);

// Desktop build (Windows, Linux, macOS): the keyboard drives the game (src/platform/desktop/PCInput).
bool desktopBuild();

// On-screen driving controls (move / lean / special / eject, the ejected d-pad and grab, the
// restored characters' extra buttons). Auto = hidden on desktop builds, shown on touch devices
// unless a game controller is connected (PAD). Hidden buttons are still laid out and pressed by the keyboard
// bridge, but are not drawn and ignore real touches (the mouse); pause, reset, the timer and the
// boost meter stay, the reset button shows its key and tutorial arrows show the keys.
enum class TouchControls { Auto = 0, Show = 1, Hide = 2 };
TouchControls touchControls();
void setTouchControls(TouchControls mode);
const char* touchControlsName(TouchControls mode);
bool touchControlsShown();  // resolved for this build
// PAD (PC addition): controller rumble (input/Haptics.h): off, low, medium or high (default).
enum class RumbleLevel { Off = 0, Low = 1, Medium = 2, High = 3 };
RumbleLevel rumbleLevel();
void setRumbleLevel(RumbleLevel level);
const char* rumbleLevelName(RumbleLevel level);
float rumbleScale();  // 0 (off) .. 1 (high)
// PAD (PC addition): the same haptics on the phone's own vibrator when no controller is connected
// (Android). Off by default (the original never vibrates).
bool phoneVibration();
void setPhoneVibration(bool on);
bool phoneVibrationSupported();

// PAD (PC addition): tilt steering on phones, tablets and handhelds with a motion sensor
// (input/Tilt.h): turning the device like a steering wheel leans. Off by default; the level sets
// how far it has to turn (low = 20 degrees, medium = 12, high = 7).
enum class TiltSteering { Off = 0, Low = 1, Medium = 2, High = 3 };
TiltSteering tiltSteering();
void setTiltSteering(TiltSteering mode);
const char* tiltSteeringName(TiltSteering mode);
bool tiltSteeringSupported();  // Android and iOS builds

// Set by the keyboard bridge while it injects a virtual finger, so GameplayControls can tell it
// from a real touch (GLView renumbers touch ids).
void setKeyboardTouch(bool on);
bool keyboardTouch();

// Re-grab vehicle: an ejected main character whose hand grabs his own (unsmashed) vehicle at least
// 0.5 s (in physics steps) after the ejection gets back on and rides again (Vehicle::qolTryRemount).
// On by default - the one option that departs from the original by default; it only acts on a grab
// of the rider's own vehicle, which the original treats as an ordinary grip.
bool regrabVehicle();               // the setting, unless suspended for this run
bool regrabVehicleSetting();        // the stored setting
void setRegrabVehicle(bool on);
// --dump-world verification: the original's behaviour for this run, whatever the setting.
void suspendRegrabVehicle(bool suspended);

// Applies display options (FPS counter, fullscreen) - call once the GL view exists.
void applyDisplaySettings();

// Blood emitters are tagged at creation so renderers can single them out.
void markBlood(cocos2d::Node* emitter);
bool isBlood(const cocos2d::Node* emitter);

}  // namespace qol
