#pragma once
// Art for the iOS-only level editor, loaded from the player's own iOS app bundle at runtime.
//
// iOS ships every atlas three times (-hd: iPhone retina, -ipad: 1x, -ipadhd: 2x) and lays the
// editor out in iPad points (1024x768). OpenWheels runs at the Android design resolution
// (3600x2000, FIXED_HEIGHT) with the original's asset tiers, so:
//   * atlas suffix: large tier -> "-ipadhd", every other tier -> "-ipad"
//   * pointsToDesign(): multiply an iPad point coordinate/size by this to get design units
//     (scaled by height: 2000 / 768)
//   * spriteScale(): setScale() for sprites from these atlases so they appear at their iPad
//     point size in design units.

#include <string>

namespace EditorAssets {

const char* suffix();                          // "-ipadhd" or "-ipad"
float pixelsPerPoint();                        // 2 for -ipadhd, 1 for -ipad
float pointsToDesign();                        // design units per iPad point
float spriteScale();                           // scale for sprites from editor atlases

// Adds "<bundle>/<base><suffix>.plist" (e.g. "editorui", "levelEditorObjects1") to the
// SpriteFrameCache once. Returns false if the iOS bundle or the atlas is missing.
bool loadAtlas(const std::string& base);

}  // namespace EditorAssets
