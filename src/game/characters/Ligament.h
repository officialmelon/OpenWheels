#pragma once

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

// Gore: the stretchy ligament left between an upper and a lower limb when an elbow/knee breaks
// (created by CharacterB2D, drawn as segments by LevelItemsDrawNode via getJoints()). A chain of
// small bodies linked by distance joints; _mainJoint is the one that snaps.
//
// arm64 sizeof 0x60; first field at 0x24 (cocos2d::Ref dsize 0x21). No user-provided constructor or
// destructor: create() value-initialises (zero fill) the object.
class Ligament : public cocos2d::Ref
{
public:
    // new (std::nothrow) Ligament() + init (inlined; always succeeds) + autorelease.
    static Ligament* create(float ptmRatio, float timeStep, b2Body* upperBody, b2Body* lowerBody,
                            float length);                                  // @005e5788
    // Stores the bodies and the time-step values, then create(...). Always returns true.
    // ptmRatio and length are not used.
    bool init(float ptmRatio, float timeStep, b2Body* upperBody, b2Body* lowerBody,
              float length);                                                // @005e5860
    // _timeStepInverse = 1 / timeStep; _breakLimitSquared = (_timeStepInverse * 30 / 60)^2
    void setTimeStep(float timeStep);                                       // @005e58a0
    // Builds the chain bodies and joints.
    // RE-TODO(@005e58c8): the float argument is never read (the call sites leave a dead value in
    // s0); its meaning is unknown.
    void create(float length);                                              // @005e58c8
    // Breaks _mainJoint when its reaction force exceeds the limit.
    void checkJoints();                                                     // @005e6190
    // Unconditionally removes and destroys _mainJoint.
    void breakJoint();                                                      // @005e622c
    // Copy of _joints.
    std::vector<b2DistanceJoint*> getJoints();                              // @005e62a0

protected:
    // RE-TODO(@005e5860): 4-byte member never written or read (zero from value-initialisation);
    // named after the ptm ratio SpinalCord/IntestineChain keep at this offset.
    float _ptmRatio;                                 // +0x24
    float _timeStepInverse;                          // +0x28
    float _breakLimitSquared;                        // +0x2c
    b2Body* _upperBody;                              // +0x30
    b2Body* _lowerBody;                              // +0x38
    std::vector<b2DistanceJoint*> _joints;           // +0x40  every joint of the chain (incl. _mainJoint)
    b2DistanceJoint* _mainJoint;                     // +0x58
};
