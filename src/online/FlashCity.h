#pragma once
// ONLINE (PC addition): the browser game's city background (<info bg="2">) for converted browser
// levels. The Android build has no city art (its BackgroundCity case loads a gradient the APK
// doesn't ship); the pieces are rendered from the player's SWF at build time by
// tools/assets/flash_city.py into generated/flash/city/ (city.tsv + PNGs).
//
// Flash (UserLevel.createBackDrops, StageCamera.adjustBackDrops): three BackDrops at the bottom of
// the session - the static sky CitySource3, CityBackDrop2 (multiplier 0.25) and CityBackDrop1
// (0.5) - each moved by round(container position * multiplier). Buildings are their top part plus
// a row tiled down to 10000 * m + 500 * (1 - m); every backdrop is drawn twice, 5000 / 2500 px
// apart. Here each piece becomes a sprite of the BackgroundLayer with a mobile Backdrop of the
// same multiplier (BackgroundLayer::update passes the gameplay container position).

#include <vector>

#include "math/Vec2.h"

namespace cocos2d {
class Node;
class Sprite;
}

namespace online {

struct CityBackdropPiece {
    cocos2d::Sprite* sprite;
    cocos2d::Vec2 origin;   // position (points) when the container is at (0, 0)
    float multiplier;
};

// Creates the sprites as children of `parent` (bottom to top) and returns them; empty when the
// generated art is missing (the caller then shows its fallback).
std::vector<CityBackdropPiece> createCityBackground(cocos2d::Node* parent, float ptmRatio);

}  // namespace online
