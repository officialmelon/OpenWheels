#pragma once

// RESTORED (PC addition): Explorer Guy (browser game character 7, Flash MiddleAgedExplorer.as), a
// CharacterB2D (voice "Char2", as in Flash) standing in a MineCart. His lower legs are hidden in
// the cart until he leaves it or breaks a hip or knee; his hat (the helmet frames) flies off at
// impulse 0.75 instead of a helmet's 2. Shift / ctrl (control bits 0x20 / 0x40) stand him up and
// crouch him while he rides (Flash shift/ctrlPressedActions).

#include "CharacterB2D.h"

#include <string>

class ExplorerGuy : public CharacterB2D
{
public:
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);
    ~ExplorerGuy() override;

    void setState(unsigned char state) override;

    // Shows a lower leg the cart hid (1 or 2; 0 = both).
    void showLowerLeg(int leg);
    // QOL (PC addition): re-grab vehicle - back in the cart, the legs still on are hidden again.
    void hideLowerLegs(bool leg1, bool leg2);
};
