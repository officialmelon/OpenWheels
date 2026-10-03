#pragma once

#include "cocos2d.h"

#include "CharacterB2D.h"

#include <string>

// Business Guy: a CharacterB2D (voice "Char2") riding a PersonalTransporter, which init() creates,
// attaches and retains (CharacterB2D::_vehicle). No own fields: arm64 sizeof 0x510.
class BusinessGuy : public CharacterB2D
{
public:
    BusinessGuy();                                                          // @00588e74
    // Releases the vehicle (null-checked).
    ~BusinessGuy() override;                                                // @00588fb4 (D1), @00589000 (D0)

    // Not virtual (hides CharacterB2D::init): CharacterB2D::init(position, name, "Char2",
    // vehicleName, groupIndex, showGore, true), then the vehicle.
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);                                               // @00589024

    // CharacterB2D::taperBodies() plus a 0.75 taper of the chest.
    void taperBodies() override;                                            // @00589410  vptr+0x190
};
