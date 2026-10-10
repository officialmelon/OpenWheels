#pragma once
// ONLINE (PC addition): the browser game's exact rag-doll and vehicle geometry.
//
// The mobile body files (characters/bodies/*.plist, vehicles/bodies/*.plist) round every part's
// position, size and angle, and every joint anchor, to the millimetre (and the mobile game slimmed
// a few chests). The browser game builds the same parts from its own art, to a tenth of a
// millimetre, and a browser replay diverges from those small differences within a few seconds (the
// "don't move" levels most of all). CharacterB2D::loadBodies and Vehicle::loadBodies call these on
// a browser level to put the browser values back.

#include <string>

#include "cocos2d.h"

namespace online {

// Replaces the parts and joint anchors of `bodiesDict` (a loaded body file named `name`, such as
// "wheelchair_guy_wheelchair") with the browser game's. Characters without browser values are left
// as they are. The moped riders also get the browser's "handleAnchor" and "footAnchor" joints (where
// both hands and both feet hold on; see Moped::attachCharacter).
void applyFlashCharacterShapes(const std::string& name, cocos2d::ValueMap& bodiesDict);

// The same for a vehicle's body file (named like "moped").
void applyFlashVehicleShapes(const std::string& name, cocos2d::ValueMap& bodiesDict);

}  // namespace online
