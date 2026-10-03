#pragma once

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <string>
#include <vector>

// Gore: the spine shown when a character's torso is torn apart (created by CharacterB2D, drawn
// by LevelItemsDrawNode via getJoints()). A chain of small circle bodies ("vertebrae") between the
// chest and the head, linked by distance joints, each with a "<tag>_spine.png" sprite added next
// to the chest sprite.
//
// arm64 sizeof 0x98; first field at 0x24 (cocos2d::Ref dsize 0x21). No user-provided constructor or
// destructor: create() value-initialises (zero fill) the object; the implicit constructor only
// constructs the two vectors.
class SpinalCord : public cocos2d::Ref
{
public:
    // new (std::nothrow) SpinalCord() + initWithTag (inlined; always succeeds) + autorelease.
    // tag = the character name (sprite frame prefix).
    static SpinalCord* create(std::string tag, float ptmRatio, float timeStep, b2Body* chestBody,
                              b2Vec2 chestAnchor, b2Body* headBody, int totalVertebrae,
                              float spineLength);                           // @00630f10
    // Stores the parameters (see setTimeStep for the derived values), then create(tag).
    // Always returns true.
    bool initWithTag(std::string tag, float ptmRatio, float timeStep, b2Body* chestBody,
                     b2Vec2 chestAnchor, b2Body* headBody, int totalVertebrae,
                     float spineLength);                                    // @006310e4
    // _timeStepInverse = 1 / timeStep; _breakLimitSquared = (_timeStepInverse * 30 / 60)^2
    void setTimeStep(float timeStep);                                       // @006311b4
    // Builds the sprites (totalVertebrae + 1), the vertebra bodies and the distance joints.
    void create(std::string tag);                                           // @006311dc
    // Places/rotates the sprites along the joints.
    void paint();                                                           // @00631adc
    // Break the chest end / the head end (destroy the joint, hide the end sprite).
    void spineBreak1();                                                     // @00631ce8
    void spineBreak2();                                                     // @00631d3c
    // Breaks an end joint whose reaction force exceeds the limit.
    void checkJoints();                                                     // @00631d90
    // Copy of _joints (the inner vertebra-to-vertebra joints).
    std::vector<b2DistanceJoint*> getJoints();                              // @00631e74

protected:
    float _ptmRatio;                                 // +0x24  sprite positions = world * _ptmRatio
    float _timeStepInverse;                          // +0x28  GetReactionForce argument
    float _breakLimitSquared;                        // +0x2c  squared reaction force that breaks an end joint
    int _totalVertebrae;                             // +0x30
    float _spineLength;                              // +0x34  length of every distance joint
    // RE-TODO(@00630f10): 4-byte member never written or read (zero from value-initialisation);
    // named after IntestineChain's field at the same offset.
    float _spriteWidth;                              // +0x38
    b2Vec2 _chestAnchor;                             // +0x3c  local point on _chestBody
    b2Body* _chestBody;                              // +0x48
    b2Body* _headBody;                               // +0x50
    std::vector<cocos2d::Sprite*> _spineSprites;     // +0x58
    std::vector<b2DistanceJoint*> _joints;           // +0x70
    b2DistanceJoint* _headToVertebraJoint;           // +0x88
    b2DistanceJoint* _vertebraToChestJoint;          // +0x90
};
