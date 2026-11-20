#pragma once

#include "cocos2d.h"

#include "CharacterB2D.h"

#include <string>

// Wheelchair Guy: a CharacterB2D (voice "Char1") in a Wheelchair, which init() creates, attaches
// and retains (CharacterB2D::_vehicle). No own fields: arm64 sizeof 0x510.
class WheelchairGuy : public CharacterB2D
{
public:
    WheelchairGuy();                                                        // @00646df8
    // Releases the vehicle (null-checked).
    ~WheelchairGuy() override;                                              // @00646f38 (D1), @00646f84 (D0)

    // Not virtual (hides CharacterB2D::init): CharacterB2D::init(position, name, "Char1",
    // vehicleName, groupIndex, showGore, true), then the vehicle.
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);                                               // @00646fa8

    // CharacterB2D::taperBodies() plus chest tapers 0.8 (false) and 0.7 (true).
    void taperBodies() override;                                            // @006472d4  vptr+0x190

    // Port of WheelChairGuy.shift / ctrlPressedActions: riding, Shift / Ctrl turn the jet by +/-0.1
    // rad per Flash frame (Wheelchair::turnJet); ejected, they are the wheel poses.
    void setState(unsigned char state) override;
    CharacterPose ejectedExtraPose(bool ctrl) override { return ctrl ? CharacterPoseWheelRight : CharacterPoseWheelLeft; }
};
