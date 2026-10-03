#pragma once

// CircleArtShape: circle art description (dead code on Android, see GroupArtShape.h).

#include "GroupArtShape.h"
#include "math/Vec2.h"

class CircleArtShape : public GroupArtShape
{
public:
    bool init(cocos2d::Color4F innerColor, cocos2d::Vec2 position, float radius);

protected:
    cocos2d::Vec2 _position;  // +0x38
    float _radius;            // +0x40
};
