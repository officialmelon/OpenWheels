#pragma once

// RESTORED (PC addition): Santa Claus (browser game character 8, Flash SantaClaus.as), a CharacterB2D
// (voice "Santa", polygon chest and pelvis) on a Sleigh pulled by two elves (Flash SleighElf: plain
// CharacterB2Ds, voice "Elf1", groups -2 and -4). The elves follow the controls every frame (they
// run with up/down), and once thrown off pose and grab like any ejected character. Shift
// (control bit 0x20) lets go of elves that cannot run; once Santa is off, the eject bit releases
// both elves (Flash zPressedActions).

#include "CharacterB2D.h"

#include <string>

class SantaClaus : public CharacterB2D
{
public:
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);
    ~SantaClaus() override;

    void setState(unsigned char state) override;

private:
    CharacterB2D* _elves[2] = {nullptr, nullptr};
};
