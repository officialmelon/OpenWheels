#pragma once

#include "cocos2d.h"

// Empty intermediate scene used by Gameplay to restart a level. It waits 5 update ticks, then
// replaces itself with a fresh Gameplay::createScene("", nullptr).
//
// arm64 sizeof 0x330.
class ResetWorkaroundScene : public cocos2d::Layer
{
public:
    static cocos2d::Scene* createScene();       // @0064c8c4  (create() inlined)

    CREATE_FUNC(ResetWorkaroundScene);          // (inlined into createScene, no symbol)

    ResetWorkaroundScene();                     // @0064c96c
    ~ResetWorkaroundScene() override;           // @0064ca6c (D1), @0064ca80 (D0)

    // Does not call Layer::init().
    bool init() override;                       // @0064c99c  vptr+0x4f8
    void update(float dt) override;             // @0064c9b8  vptr+0x3d8

protected:
    int _frameCount;    // +0x320  not initialised by the ctor; reset in init()
};
