#include "platform/common/EditorAssets.h"

#include <set>

#include "cocos2d.h"
#include "platform/common/IOSBundle.h"

USING_NS_CC;

namespace EditorAssets {
namespace {
const float kDesignHeight = 2000.0f;  // AppDelegate's design resolution height
const float kIPadPointsHeight = 768.0f;

bool largeTier() {
    // AppDelegate sets content scale = tier height / 2000; only the large tier is 1.0.
    return Director::getInstance()->getContentScaleFactor() >= 0.99f;
}
}  // namespace

const char* suffix() { return largeTier() ? "-ipadhd" : "-ipad"; }

float pixelsPerPoint() { return largeTier() ? 2.0f : 1.0f; }

float pointsToDesign() { return kDesignHeight / kIPadPointsHeight; }

float spriteScale() {
    // A texture pixel covers 1/contentScale design units; we want pixelsPerPoint pixels to span
    // pointsToDesign design units.
    const float csf = Director::getInstance()->getContentScaleFactor();
    return pointsToDesign() * csf / pixelsPerPoint();
}

bool loadAtlas(const std::string& base) {
    static std::set<std::string> loaded;
    if (loaded.count(base)) return true;
    if (!openwheels::hasIOSBundle()) {
        log("EditorAssets: no iOS bundle - cannot load %s", base.c_str());
        return false;
    }
    const std::string plist = openwheels::iosBundlePath() + base + suffix() + ".plist";
    if (!FileUtils::getInstance()->isFileExist(plist)) {
        log("EditorAssets: missing %s", plist.c_str());
        return false;
    }
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile(plist);
    loaded.insert(base);
    return true;
}

}  // namespace EditorAssets
