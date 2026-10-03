#include "CircleArtShape.h"

USING_NS_CC;

// @005a5358
bool CircleArtShape::init(Color4F innerColor, Vec2 position, float radius)
{
    bool result = GroupArtShape::init(innerColor);
    if (result)
    {
        _position = position;
        _radius = radius;
    }
    return result;
}
