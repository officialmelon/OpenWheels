#pragma once

#include "cocos2d.h"

#include "CharacterB2D.h"

#include <string>

// Irresponsible Dad: a CharacterB2D (voice "Char3") on a RoadBike (created, RoadBike::addDad,
// retained) with his kid in the child seat. The kid is a plain CharacterB2D
// ("irresponsible_dad_kid", voice "kid1", group -2) attached with RoadBike::addKid; grab and pose
// commands are forwarded to it, and the kid mourns the dad (setMourner).
// arm64 sizeof 0x518 (one own field).
class IrresponsibleDad : public CharacterB2D
{
public:
    // Not virtual (hides CharacterB2D::init): CharacterB2D::init(position, name, "Char3",
    // vehicleName, groupIndex, showGore, true), then the bike and the kid.
    bool init(cocos2d::Vec2 position, std::string name, std::string vehicleName, int groupIndex,
              bool showGore);                                               // @005cb20c

    IrresponsibleDad();                                                     // @005cb8c0
    // Releases the kid, then the vehicle (both null-checked).
    ~IrresponsibleDad() override;                                           // @005cb9f8 (D2), @005cba54 (D0)

    // CharacterB2D overrides (vtable order). setCurrentPose/startGrab/endGrab forward to _kid
    // (if any) first.
    void setCurrentPose(CharacterPose pose) override;                       // @005cbb88  vptr+0x128
    void startGrab() override;                                              // @005cba78  vptr+0x130
    void endGrab() override;                                                // @005cbaac  vptr+0x138
    // ONLINE (PC addition): the kid's injury limits follow a time step change too (only the
    // level's own characters get timeStepChanged; browser physics, online/FlashPhysics.h).
    void timeStepChanged() override
    {
        CharacterB2D::timeStepChanged();
        if (_kid) _kid->timeStepChanged();
    }
    // addVocalsWithName("Damnit", priority 1)
    void mourn() override;                                                  // @005cbae0  vptr+0x148
    // CharacterB2D::taperBodies() plus a 0.85 taper of the chest.
    void taperBodies() override;                                            // @005cbbc8  vptr+0x190

protected:
    CharacterB2D* _kid = nullptr;   // +0x510  retained (iOS `kid`, a ChildSeatKid)
};
