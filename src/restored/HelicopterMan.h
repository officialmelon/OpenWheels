#pragma once

// RESTORED (PC addition): Helicopter Man (browser game character 11, Flash HelicopterMan.as), a
// CharacterB2D (voice "Heli", as in Flash) flying a Helicopter: seated with both hands on the
// handle, his polygon chest and pelvis from the browser rig. Shift / ctrl (control bits 0x20 /
// 0x40) reel the magnet's rope in / out while he flies (Flash shift/ctrlPressedActions).

#include "CharacterB2D.h"

#include <string>

class HelicopterMan : public CharacterB2D
{
public:
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);
    ~HelicopterMan() override;

    void setState(unsigned char state) override;
};
