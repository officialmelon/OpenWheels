#pragma once

#include "cocos2d.h"

#include "CharacterB2D.h"

#include <string>

// Moped Couple: the driver is this CharacterB2D (voice "Char8") on a Moped (Moped::addDriver, then
// setVehicle); the passenger is a plain CharacterB2D ("moped_girl", voice "Char9", group -2) added
// with Moped::addPassenger. Grab and pose commands are forwarded to her; she is set as this
// character's mourner. arm64 sizeof 0x518 (one own field).
class MopedCouple : public CharacterB2D
{
public:
    // Not virtual (hides CharacterB2D::init): CharacterB2D::init(position, name, "Char8",
    // vehicleName, groupIndex, showGore, true), then the moped and the girl.
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);                                               // @005f3e8c

    MopedCouple();                                                          // @005f444c
    // Releases the girl and the vehicle (neither null-checked).
    ~MopedCouple() override;                                                // @005f4584 (D1), @005f45d8 (D0)

    // CharacterB2D overrides (vtable order); each forwards to _girl first (no null check).
    void setCurrentPose(CharacterPose pose) override;                       // @005f465c  vptr+0x128
    void startGrab() override;                                              // @005f45fc  vptr+0x130
    void endGrab() override;                                                // @005f462c  vptr+0x138

protected:
    CharacterB2D* _girl = nullptr;  // +0x510  retained (iOS `girl`, a MopedGirl)
};
