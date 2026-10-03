#pragma once

// Backdrop: one parallax background sprite of a BackgroundLayer. Plain class, sizeof 0x18.

#include "math/Vec2.h"

namespace cocos2d {
class Sprite;
}

class Backdrop
{
public:
    Backdrop(cocos2d::Sprite* sprite, cocos2d::Vec2 origin, float multiplier);
    ~Backdrop();

    // sprite->setPosition(Vec2(origin.x + multiplier * pos.x, origin.y + multiplier * pos.y))
    void update(cocos2d::Vec2 pos);

private:
    float _multiplier;         // +0x00
    cocos2d::Vec2 _origin;     // +0x04
    cocos2d::Sprite* _sprite;  // +0x10
};
