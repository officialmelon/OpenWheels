#pragma once

#include "cocos2d.h"

#include "CharacterB2D.h"

#include <string>

// Effective Shopper (iOS `Shopper`): a CharacterB2D (voice "Char4") pushing a MotorCart, which
// init() creates, attaches and retains (CharacterB2D::_vehicle). No own fields: arm64 sizeof 0x510.
class EffectiveShopper : public CharacterB2D
{
public:
    // Not virtual (hides CharacterB2D::init): CharacterB2D::init(position, name, "Char4",
    // vehicleName, groupIndex, showGore, true), then the vehicle.
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);                                               // @005acb38

    EffectiveShopper();                                                     // @005ace64
    // Releases the vehicle (not null-checked).
    ~EffectiveShopper() override;                                           // @005acfa4 (D1), @005acfec (D0)
};
