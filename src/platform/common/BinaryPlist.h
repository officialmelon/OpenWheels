#pragma once
// Apple binary plists ("bplist00"). The iOS bundle's Localizable.strings and its TexturePacker
// atlases (editorui, levelEditorObjects1, ...) are binary; cocos2d-x only reads XML plists.

#include <string>

#include "cocos2d.h"

namespace openwheels {

// Parses bplist00 bytes into a Value (dict -> ValueMap, array -> ValueVector, string, int,
// real, bool; data and dates become empty strings). Returns a null Value on failure.
cocos2d::Value parseBinaryPlist(const cocos2d::Data& bytes);

// A plist file's top-level dict, binary or XML.
cocos2d::ValueMap readPlistDict(const std::string& path);

// SpriteFrameCache::addSpriteFramesWithFile for a plist that may be binary, with its texture
// (CgBI-aware, see AppleImage.h) loaded from the plist's directory.
bool addSpriteFramesWithPlist(const std::string& plistPath);

}  // namespace openwheels
