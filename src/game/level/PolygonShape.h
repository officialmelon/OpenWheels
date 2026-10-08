#pragma once

// PolygonShape: polygon art for a level shape / art shape (FFDrawNode::drawPolyWithVerts).
// sizeof 0x50. Note the member order differs from Rectangle/TriangleShape (rotation first).

#include "ShapeItem.h"
#include "base/ccTypes.h"
#include "math/CCAffineTransform.h"
#include "math/Vec2.h"

class PolygonShape : public ShapeItem
{
public:
    PolygonShape();
    virtual ~PolygonShape();

    // Vertices are read from the fixture's b2PolygonShape (scaled by ptmRatio).
    bool init(b2Fixture* fixture, float ptmRatio, cocos2d::Color4F innerColor,
              cocos2d::Color4F outlineColor, float opacity, float borderWidth,
              FFDrawNode* drawNode);
    bool init(cocos2d::Vec2 pos, float rotationRadians, cocos2d::Vec2* verts, unsigned int count,
              cocos2d::AffineTransform initialTransform, cocos2d::Color4F innerColor,
              cocos2d::Color4F outlineColor, float opacity, float borderWidth,
              FFDrawNode* drawNode, bool update);
    // ONLINE (PC addition): like the fixture init, but drawn from `verts` (body-local points:
    // a browser polygon's own, possibly concave outline) instead of the fixture's convex hull.
    bool onlineInit(b2Fixture* fixture, float ptmRatio, cocos2d::Color4F innerColor,
                    cocos2d::Color4F outlineColor, float opacity, float borderWidth,
                    cocos2d::Vec2* verts, unsigned int count, FFDrawNode* drawNode);

    float getArtOpacity() override;                       // +0x10
    cocos2d::AffineTransform getArtTransform() override;  // +0x18

protected:
    float _rotation;          // +0x40  radians, ctor 0
    cocos2d::Vec2 _position;  // +0x44  ctor Vec2::ZERO
};
