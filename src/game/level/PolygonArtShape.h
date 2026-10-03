#pragma once

// PolygonArtShape: polygon art description (dead code on Android, see GroupArtShape.h).

#include <vector>

#include "GroupArtShape.h"
#include "math/Vec2.h"

class PolygonArtShape : public GroupArtShape
{
public:
    bool init(cocos2d::Color4F innerColor, std::vector<cocos2d::Vec2> verts);

protected:
    std::vector<cocos2d::Vec2> _verts;  // +0x38
};
