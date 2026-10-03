#pragma once

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <string>
#include <vector>

// Gore: the intestines hanging between a character's chest and pelvis once the torso breaks
// (created by CharacterB2D). A chain of segment bodies linked by distance joints, each drawn with
// a "<tag>_intestine.png" sprite stretched over the segment.
//
// arm64 sizeof 0x90; first field at 0x24 (cocos2d::Ref dsize 0x21). Unlike SpinalCord it has an
// inline user-provided constructor (create() does not zero-fill; every member gets the default
// below) and a destructor with a body.
class IntestineChain : public cocos2d::Ref
{
public:
    // new (std::nothrow) IntestineChain() + initWithTag (inlined; always succeeds) + autorelease.
    // tag = the character name (sprite frame prefix).
    static IntestineChain* create(std::string tag, float ptmRatio, float timeStep,
                                  b2Body* chestBody, b2Body* pelvisBody, int totalIntestines,
                                  float intestineLength);                   // @005c9f80

    // Inline in the original header (no out-of-line copy exists).
    IntestineChain() {}
    // Nulls the sprite pointers, clears both vectors.
    ~IntestineChain() override;                                             // @005cb0f4 (D2), @005cb17c (D0)

    // Stores the parameters (see setTimeStep for the derived values), then create(tag).
    // Always returns true.
    bool initWithTag(std::string tag, float ptmRatio, float timeStep, b2Body* chestBody,
                     b2Body* pelvisBody, int totalIntestines, float intestineLength);  // @005ca13c
    // _timeStepInverse = 1 / timeStep; _breakLimitSquared = (_timeStepInverse * 30 / 60)^2
    void setTimeStep(float timeStep);                                       // @005ca208
    // Builds the sprites, the segment bodies and the distance joints.
    void create(std::string tag);                                           // @005ca230
    void paint();                                                           // @005cac34
    // Break the pelvis end / the chest end (destroy the joint, hide the end sprite).
    void intestineBreak1();                                                 // @005caf68
    void intestineBreak2();                                                 // @005cafbc
    // Breaks an end joint whose reaction force exceeds the limit.
    void checkJoints();                                                     // @005cb010

protected:
    float _ptmRatio = 1.0f;                                 // +0x24
    float _timeStepInverse = 1.0f;                          // +0x28
    float _breakLimitSquared = 0.0001f;                     // +0x2c  (0x38d1b717)
    int _totalIntestines = 0;                               // +0x30
    float _intestineLength = 0.0001f;                       // +0x34  length of every distance joint
    float _spriteWidth = 0.0001f;                           // +0x38  content width of the intestine sprite (create)
    b2Body* _chestBody = nullptr;                           // +0x40
    b2Body* _pelvisBody = nullptr;                          // +0x48
    std::vector<cocos2d::Sprite*> _intestineSprites;        // +0x50
    std::vector<b2DistanceJoint*> _joints;                  // +0x68  inner segment-to-segment joints
    b2DistanceJoint* _pelvisIntestineJoint = nullptr;       // +0x80
    b2DistanceJoint* _intestineChestJoint = nullptr;        // +0x88
};
