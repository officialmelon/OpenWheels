#pragma once

// TriangleShape: isosceles-triangle art for a level shape (3-vertex drawPolyWithVerts:
// (-w/2,-h/3), (w/2,-h/3), (0,2h/3)). sizeof 0x50.

#include "ShapeItem.h"
#include "base/ccTypes.h"
#include "math/CCAffineTransform.h"
#include "math/Vec2.h"

class TriangleShape : public ShapeItem
{
public:
    TriangleShape();
    virtual ~TriangleShape();

    bool init(b2Fixture* fixture, float ptmRatio, cocos2d::Color4F innerColor,
              cocos2d::Color4F outlineColor, float opacity, float borderWidth, float width,
              float height, FFDrawNode* drawNode);
    bool init(cocos2d::Vec2 pos, float rotationRadians, cocos2d::AffineTransform initialTransform,
              cocos2d::Color4F innerColor, cocos2d::Color4F outlineColor, float opacity,
              float borderWidth, float width, float height, FFDrawNode* drawNode, bool update);

    float getArtOpacity() override;                       // +0x10
    cocos2d::AffineTransform getArtTransform() override;  // +0x18

protected:
    cocos2d::Vec2 _position;  // +0x40  ctor Vec2::ZERO
    float _rotation;          // +0x48  radians, ctor 0
};
