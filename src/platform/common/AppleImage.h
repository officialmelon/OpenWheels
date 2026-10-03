#pragma once
// Images from the player's iOS bundle. Most of its PNGs are Apple's "CgBI" variant (Xcode's
// pngcrush -iphone), which libpng rejects: an extra CgBI chunk, raw-deflate IDAT, BGRA pixels with
// premultiplied alpha. These helpers decode both kinds.

#include <string>

#include "cocos2d.h"

namespace openwheels {

bool isCgBI(const cocos2d::Data& file);

// A new Image (caller releases) from PNG/JPEG bytes, CgBI included; nullptr on failure.
cocos2d::Image* createImage(const cocos2d::Data& file);

// TextureCache::addImage(path) that also understands CgBI: the decoded texture is cached under
// the same key cocos uses for `path`, so later loads by path (SpriteFrameCache plists,
// SpriteBatchNode::create, Sprite::create) find it.
cocos2d::Texture2D* addTexture(const std::string& path);

}  // namespace openwheels
