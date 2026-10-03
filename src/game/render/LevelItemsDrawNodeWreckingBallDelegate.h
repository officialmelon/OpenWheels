#pragma once

// Pure interface (vptr only). Implemented by WreckingBall (sub-object at +0x98); the
// implementer's secondary vtable is {getPointA, getPointB} (no virtual destructor here).
// LevelItemsDrawNode draws the wrecking-ball chain between the two points (pixels).

#include "math/Vec2.h"

class LevelItemsDrawNodeWreckingBallDelegate
{
public:
    virtual cocos2d::Vec2 getPointA() = 0;  // +0x00
    virtual cocos2d::Vec2 getPointB() = 0;  // +0x08
};
