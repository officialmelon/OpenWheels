#pragma once

// LevelItemsDrawNode: cocos2d::DrawNode redrawn every frame (update(), from Session::update)
// with the segments of registered ligaments, spinal cords, harpoon-gun ropes and
// wrecking-ball chains. sizeof 0x500 (arm64); fields start in DrawNode's tail (dsize 0x45c).
// No user-declared constructor: create() value-initialises (memset 0) then runs the members'
// default initialisers.

#include <vector>

#include "2d/CCDrawNode.h"
#include "base/ccTypes.h"

class HarpoonGun;
class Ligament;
class LevelItemsDrawNodeWreckingBallDelegate;
class SpinalCord;
class WreckingBall;

class LevelItemsDrawNode : public cocos2d::DrawNode
{
public:
    static LevelItemsDrawNode* create(float ptmRatio);
    bool init(float ptmRatio);
    void update();  // not Node::update(float)
    void addLigament(Ligament* ligament);
    void addSpinalCord(SpinalCord* spinalCord);
    void addWreckingBall(WreckingBall* wreckingBall);
    void addWreckingBallDelegate(LevelItemsDrawNodeWreckingBallDelegate* wreckingBallDelegate);
    void addHarpoonGun(HarpoonGun* harpoonGun);
    void removeAll();
    virtual ~LevelItemsDrawNode();

protected:
    // Names from the iOS original's LigamentsNode.
    cocos2d::Color4F _redColor = cocos2d::Color4F::RED;     // +0x45c ligaments, spinal cords
    cocos2d::Color4F _blackColor = cocos2d::Color4F::BLACK; // +0x46c harpoon ropes, ball chains
    float _ptmRatio;                                        // +0x47c
    float _thickness;                                       // +0x480 ptmRatio * 0.02 (line width)
    std::vector<Ligament*> _ligaments;                      // +0x488
    std::vector<SpinalCord*> _spinalCords;                  // +0x4a0
    std::vector<HarpoonGun*> _harpoons;                     // +0x4b8
    std::vector<WreckingBall*> _wreckingBalls;              // +0x4d0
    std::vector<LevelItemsDrawNodeWreckingBallDelegate*> _wreckingBallDelegates;  // +0x4e8
};
