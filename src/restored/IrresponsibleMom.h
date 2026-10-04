#pragma once

// RESTORED (PC addition): Irresponsible Mom (browser game character 10, Flash IrresponsibleMom.as),
// a CharacterB2D (voice "Char4", which the mobile port still ships for the Effective Shopper) on a
// MomBike, with her daughter (IMDaughter, voice "Kid2", group -2) pedalling the trailer bike and
// her son (IMSon, voice "kid1", group -3) in the handlebar basket. The kids are plain
// CharacterB2Ds attached by MomBike; their gore follows the QoL "child gore" option, as
// Irresponsible Dad's kid.
//
// Controls (Flash IrresponsibleMom.checkKeyStates): every key also acts on the kids - the daughter
// pedals and brakes with the mom while she rides, shift (control bit 0x20) ejects the son, ctrl
// (0x40) the daughter; once thrown off, each kid poses and grabs with the same keys.

#include "CharacterB2D.h"

#include <string>

class IrresponsibleMom : public CharacterB2D
{
public:
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);
    ~IrresponsibleMom() override;

    void setState(unsigned char state) override;

private:
    CharacterB2D* _daughter = nullptr;
    CharacterB2D* _son = nullptr;
};
