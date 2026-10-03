#pragma once

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <string>
#include <vector>

// Base of the joint-chain gore objects in the iOS original (where SpinalCord, IntestineChain and
// Ligament derive from it). On Android it is a stand-alone cocos2d::Node that nothing uses: no
// caller of create() exists in the binary.
//
// arm64 sizeof 0x320; own fields 0x2f8..0x318. No user-provided constructor or destructor: create()
// value-initialises (zero fill) the object.
class Composite : public cocos2d::Node
{
public:
    // new (std::nothrow) Composite() + initWithFileName (devirtualised and inlined) + autorelease.
    static Composite* create(const std::string& fileName, float timeStep);  // @005a5630

    // New virtual (after Node's). Node::init(); then _timeStepInverse = 1 / timeStep.
    // fileName is not used.
    virtual bool initWithFileName(const std::string& fileName, float timeStep);  // @005a56f8  vptr+0x528

    // Copy of _distanceJoints.
    std::vector<b2DistanceJoint*> getDistanceJoints();                      // @005a5738
    // Computes the joint's reaction force (null-checked) and discards it.
    void checkDistJoint(b2DistanceJoint* joint);                            // @005a5834
    // _breakLimitSquared = (timeStepInverse * 30 / 60)^2
    void timeStepChanged(float timeStepInverse);                            // @005a5850

protected:
    std::vector<b2DistanceJoint*> _distanceJoints;   // +0x2f8
    float _timeStepInverse;                          // +0x310
    float _breakLimitSquared;                        // +0x314
};
