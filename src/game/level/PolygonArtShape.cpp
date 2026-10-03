#include "PolygonArtShape.h"

USING_NS_CC;

// @006088a0
bool PolygonArtShape::init(Color4F innerColor, std::vector<Vec2> verts)
{
    bool result = GroupArtShape::init(innerColor);
    if (result)
    {
        _verts = verts;
    }
    return result;
}
