#pragma once

#include "cocos2d.h"

#include "CharacterB2D.h"

#include <string>

// Pogostick Guy (iOS `PogoStickMan`): a CharacterB2D (voice "Char12") on a PogoStick, which init()
// creates, attaches and retains (CharacterB2D::_vehicle). No own fields: arm64 sizeof 0x510.
class PogostickGuy : public CharacterB2D
{
public:
    PogostickGuy();                                                         // @006083c4
    // Releases the vehicle (not null-checked).
    ~PogostickGuy() override;                                               // @00608504 (D1), @0060854c (D0)

    // Not virtual (hides CharacterB2D::init): CharacterB2D::init(position, name, "Char12",
    // vehicleName, groupIndex, showGore, true), then the vehicle.
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);                                               // @00608570
};
