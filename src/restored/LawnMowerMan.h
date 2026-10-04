#pragma once

// RESTORED (PC addition): Lawnmower Man (browser game character 6, Flash LawnMowerMan.as), a
// CharacterB2D (voice "Char11", which the mobile port still ships) riding a LawnMower. Art,
// bodies and sounds are generated from the browser game's character6.swf by
// tools/assets/extract_character.py.

#include "CharacterB2D.h"

#include <string>

class LawnMowerMan : public CharacterB2D
{
public:
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);
    ~LawnMowerMan() override;
};
