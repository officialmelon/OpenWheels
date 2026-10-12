#include "qol/QoL.h"

#include <algorithm>

#include "cocos2d.h"

#include "SoundController.h"
#include "input/Gamepad.h"  // PAD (PC addition)

USING_NS_CC;

namespace qol {
namespace {

const char* const kBlood = "qol_blood_style";
const char* const kMaxParticles = "qol_max_particles";
const char* const kCameraZoom = "qol_camera_zoom";
const char* const kShowFps = "qol_show_fps";
const char* const kUnlockAll = "qol_unlock_all_levels";
const char* const kFullscreen = "qol_fullscreen";
const char* const kChildGore = "qol_child_gore";
const char* const kBloodName = "qol:blood";
const char* const kTextureTier = "qol_texture_tier";
const char* const kFrameRate = "qol_frame_rate";
const char* const kEffectsVolume = "qol_effects_volume";
const char* const kMusicVolume = "qol_music_volume";
const char* const kTouchControls = "qol_touch_controls";
const char* const kRegrabVehicle = "qol_regrab_vehicle";
const char* const kRumble = "qol_pad_rumble";
const char* const kTiltSteering = "qol_tilt_steering";
const char* const kPhoneVibration = "qol_phone_vibration";

bool g_keyboardTouch = false;
bool g_regrabSuspended = false;
int g_regrab = -1;  // cached: UserDefault reads its file on every get on desktop builds
int g_rumble = -1;  // cached (read on every rumble)
int g_tilt = -1;    // cached (read every frame)
int g_phoneVibration = -1;

std::function<void(bool)>& fullscreenHandler() {
    static std::function<void(bool)> handler;
    return handler;
}

UserDefault* store() { return UserDefault::getInstance(); }

std::string& assetTier() {
    static std::string tier = "small";
    return tier;
}

std::string childGoreSheet() {
    return "generated/" + assetTier() + "/characters/irresponsible_dad_kid_gore_sprites.plist";
}

}  // namespace

BloodStyle bloodStyle() {
    const int v = store()->getIntegerForKey(kBlood, 0);
    return (BloodStyle)std::max(0, std::min(3, v));
}

void setBloodStyle(BloodStyle style) { store()->setIntegerForKey(kBlood, (int)style); }

const char* bloodStyleName(BloodStyle style) {
    switch (style) {
        case BloodStyle::Streaks: return "streaks";
        case BloodStyle::Liquid: return "liquid";
        case BloodStyle::Realistic: return "realistic";
        default: return "classic";
    }
}

int maxParticles() { return std::max(2000, store()->getIntegerForKey(kMaxParticles, 2000)); }
void setMaxParticles(int count) { store()->setIntegerForKey(kMaxParticles, count); }

float cameraZoom() {
    const float z = store()->getFloatForKey(kCameraZoom, 1.0f);
    return std::max(0.4f, std::min(1.0f, z));
}
void setCameraZoom(float zoom) { store()->setFloatForKey(kCameraZoom, zoom); }

bool showFps() { return store()->getBoolForKey(kShowFps, false); }
void setShowFps(bool on) {
    store()->setBoolForKey(kShowFps, on);
    applyDisplaySettings();
}

bool unlockAllLevels() { return store()->getBoolForKey(kUnlockAll, false); }
void setUnlockAllLevels(bool on) { store()->setBoolForKey(kUnlockAll, on); }

bool childGore() { return childGoreAvailable() && store()->getBoolForKey(kChildGore, true); }
void setChildGore(bool on) { store()->setBoolForKey(kChildGore, on); }

bool childGoreAvailable() { return FileUtils::getInstance()->isFileExist(childGoreSheet()); }

bool loadChildGoreSprites() {
    const std::string sheet = childGoreSheet();
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (cache->isSpriteFramesWithFileLoaded(sheet)) return true;
    if (!FileUtils::getInstance()->isFileExist(sheet)) return false;
    cache->addSpriteFramesWithFile(sheet);
    return cache->getSpriteFrameByName("irresponsible_dad_kid_head_2.png") != nullptr;
}

void setAssetTier(const std::string& tier) { assetTier() = tier; }
const std::string& runningAssetTier() { return assetTier(); }

std::string textureTier() {
    const std::string tier = store()->getStringForKey(kTextureTier, "");
    return tier == "large" || tier == "medium" || tier == "small" || tier == "tiny" ? tier : std::string();
}
void setTextureTier(const std::string& tier) { store()->setStringForKey(kTextureTier, tier); }

void overrideAssetTier(std::string* directory, float* resolutionHeight) {
    const std::string tier = textureTier();
    if (tier.empty()) return;
    // AppDelegate.h tier heights (design 3600 x 2000).
    const float height = tier == "large" ? 2000.0f : tier == "medium" ? 1000.0f : tier == "small" ? 750.0f : 500.0f;
    *directory = tier;
    *resolutionHeight = height;
}

int frameRate() { return store()->getIntegerForKey(kFrameRate, 60) == 30 ? 30 : 60; }
void setFrameRate(int fps) {
    store()->setIntegerForKey(kFrameRate, fps == 30 ? 30 : 60);
    installFrameRate(frameRate());
}

void installFrameRate(int fps) {
    static EventListenerCustom* secondTick = nullptr;
    Director* director = Director::getInstance();
    const bool half = fps == 30;
    director->setAnimationInterval(half ? 1.0f / 30.0f : 1.0f / 60.0f);
    director->getScheduler()->setTimeScale(half ? 0.5f : 1.0f);
    if (half && !secondTick) {
        // Director::drawScene: scheduler update (first half), EVENT_AFTER_UPDATE (second half),
        // then the scene is drawn once.
        secondTick = director->getEventDispatcher()->addCustomEventListener(
            Director::EVENT_AFTER_UPDATE, [](EventCustom*) {
                Director* d = Director::getInstance();
                d->getScheduler()->update(d->getDeltaTime());
            });
    } else if (!half && secondTick) {
        director->getEventDispatcher()->removeEventListener(secondTick);
        secondTick = nullptr;
    }
}

float effectsVolume() { return std::max(0.0f, std::min(1.0f, store()->getFloatForKey(kEffectsVolume, 1.0f))); }
void setEffectsVolume(float volume) { store()->setFloatForKey(kEffectsVolume, std::max(0.0f, std::min(1.0f, volume))); }
float musicVolume() { return std::max(0.0f, std::min(1.0f, store()->getFloatForKey(kMusicVolume, 1.0f))); }
void setMusicVolume(float volume) {
    store()->setFloatForKey(kMusicVolume, std::max(0.0f, std::min(1.0f, volume)));
    // setMasterVolume re-applies the playing music's volume (hooked to musicVolume()).
    SoundController::setMasterVolume(SoundController::getMasterVolume());
}

bool fullscreenSupported() { return (bool)fullscreenHandler(); }
bool fullscreen() { return fullscreenSupported() && store()->getBoolForKey(kFullscreen, false); }
void setFullscreen(bool on) {
    store()->setBoolForKey(kFullscreen, on);
    if (fullscreenHandler()) fullscreenHandler()(on);
}
void setFullscreenHandler(std::function<void(bool)> handler) { fullscreenHandler() = std::move(handler); }

bool desktopBuild() {
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_LINUX || \
    CC_TARGET_PLATFORM == CC_PLATFORM_MAC
    return true;
#else
    return false;
#endif
}

TouchControls touchControls() {
    const int v = store()->getIntegerForKey(kTouchControls, 0);
    return (TouchControls)std::max(0, std::min(2, v));
}
void setTouchControls(TouchControls mode) { store()->setIntegerForKey(kTouchControls, (int)mode); }

const char* touchControlsName(TouchControls mode) {
    switch (mode) {
        case TouchControls::Show: return "show";
        case TouchControls::Hide: return "hide";
        default:
            if (desktopBuild()) return "auto (hidden)";
            return openwheels::pad::anyConnected() ? "auto (hidden, controller)" : "auto (shown)";
    }
}

bool touchControlsShown() {
    switch (touchControls()) {
        case TouchControls::Show: return true;
        // PAD (PC addition): touch devices can hide them too (a controller, e.g. a handheld's
        // built-in one, drives the game).
        case TouchControls::Hide: return false;
        default:
            // Auto: hidden on desktop builds; on touch devices hidden while a controller is
            // connected (decided when the gameplay controls are laid out).
            if (desktopBuild()) return false;
            return !openwheels::pad::anyConnected();
    }
}

RumbleLevel rumbleLevel() {
    if (g_rumble < 0) g_rumble = std::max(0, std::min(3, store()->getIntegerForKey(kRumble, (int)RumbleLevel::High)));
    return (RumbleLevel)g_rumble;
}
void setRumbleLevel(RumbleLevel level) {
    g_rumble = (int)level;
    store()->setIntegerForKey(kRumble, g_rumble);
}
const char* rumbleLevelName(RumbleLevel level) {
    switch (level) {
        case RumbleLevel::Off: return "off";
        case RumbleLevel::Low: return "low";
        case RumbleLevel::Medium: return "medium";
        default: return "high";
    }
}
float rumbleScale() {
    switch (rumbleLevel()) {
        case RumbleLevel::Off: return 0.0f;
        case RumbleLevel::Low: return 0.35f;
        case RumbleLevel::Medium: return 0.65f;
        default: return 1.0f;
    }
}

bool phoneVibration() {
    if (g_phoneVibration < 0) g_phoneVibration = store()->getBoolForKey(kPhoneVibration, false) ? 1 : 0;
    return g_phoneVibration == 1 && phoneVibrationSupported();
}
void setPhoneVibration(bool on) {
    g_phoneVibration = on ? 1 : 0;
    store()->setBoolForKey(kPhoneVibration, on);
}
bool phoneVibrationSupported() {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    return true;
#else
    return false;
#endif
}

TiltSteering tiltSteering() {
    if (g_tilt < 0) g_tilt = std::max(0, std::min(3, store()->getIntegerForKey(kTiltSteering, 0)));
    return (TiltSteering)g_tilt;
}
void setTiltSteering(TiltSteering mode) {
    g_tilt = (int)mode;
    store()->setIntegerForKey(kTiltSteering, g_tilt);
}
const char* tiltSteeringName(TiltSteering mode) {
    switch (mode) {
        case TiltSteering::Low: return "low";
        case TiltSteering::Medium: return "medium";
        case TiltSteering::High: return "high";
        default: return "off";
    }
}
bool tiltSteeringSupported() {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS
    return true;
#else
    return false;
#endif
}

bool regrabVehicleSetting() {
    if (g_regrab < 0) g_regrab = store()->getBoolForKey(kRegrabVehicle, true) ? 1 : 0;
    return g_regrab == 1;
}
bool regrabVehicle() { return !g_regrabSuspended && regrabVehicleSetting(); }
void setRegrabVehicle(bool on) {
    store()->setBoolForKey(kRegrabVehicle, on);
    g_regrab = on ? 1 : 0;
}
void suspendRegrabVehicle(bool suspended) { g_regrabSuspended = suspended; }

void setKeyboardTouch(bool on) { g_keyboardTouch = on; }
bool keyboardTouch() { return g_keyboardTouch; }

void applyDisplaySettings() {
    Director::getInstance()->setDisplayStats(showFps());
}

void markBlood(Node* emitter) {
    if (emitter) emitter->setName(kBloodName);
}

bool isBlood(const Node* emitter) { return emitter && emitter->getName() == kBloodName; }

}  // namespace qol
