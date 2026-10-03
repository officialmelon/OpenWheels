#include "qol/QoL.h"

#include <algorithm>

#include "cocos2d.h"

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

bool fullscreenSupported() { return (bool)fullscreenHandler(); }
bool fullscreen() { return fullscreenSupported() && store()->getBoolForKey(kFullscreen, false); }
void setFullscreen(bool on) {
    store()->setBoolForKey(kFullscreen, on);
    if (fullscreenHandler()) fullscreenHandler()(on);
}
void setFullscreenHandler(std::function<void(bool)> handler) { fullscreenHandler() = std::move(handler); }

void applyDisplaySettings() {
    Director::getInstance()->setDisplayStats(showFps());
}

void markBlood(Node* emitter) {
    if (emitter) emitter->setName(kBloodName);
}

bool isBlood(const Node* emitter) { return emitter && emitter->getName() == kBloodName; }

}  // namespace qol
