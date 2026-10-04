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
// apart. Here each piece becomes a sprite of the BackgroundLayer, moved by updateCityBackground
// (BackgroundLayer::update passes the gameplay container position).
//
// QoL camera zoom (qol::cameraZoom() < 1 scales the session about its origin): the backdrops are
// placed for the camera position the same view centre has at zoom 1 (Flash's container
// position), then scaled by the zoom about the screen centre, so the city is the browser's view
// of that spot zoomed out with the level. The static sky still fills the screen, and the building
// strips are tiled further down to cover the taller view at the stage's bottom.

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

// Places the city backdrops of `parent` (created above) for the gameplay container position
// `containerPosition` (zoom 1: origin + multiplier * position, as a mobile Backdrop). No-op when
// `parent` has no city.
void updateCityBackground(cocos2d::Node* parent, const cocos2d::Vec2& containerPosition);

}  // namespace online
