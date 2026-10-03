#pragma once

// CircleShape: circle art for a level shape, drawn with FFDrawNode::drawDotWithOffset.
// sizeof 0x48. No user-declared constructor/destructor (constructed with
// `new (std::nothrow) CircleShape()`; D1 is ShapeItem::~ShapeItem).

#include "ShapeItem.h"
#include "base/ccTypes.h"
#include "math/Vec2.h"

class CircleShape : public ShapeItem
{
public:
    // Fixture-attached circle: art follows the fixture's body every frame (update = true).
    bool init(b2Fixture* fixture, float ptmRatio, cocos2d::Color4F innerColor,
              cocos2d::Color4F outlineColor, float opacity, float borderWidth, float radius,
              FFDrawNode* drawNode);
    // Free circle (no fixture, ptmRatio 1): position in points; update = art follows the group.
    bool init(cocos2d::Vec2 pos, cocos2d::Vec2 initialOffset, float radius,
              cocos2d::Color4F innerColor, cocos2d::Color4F outlineColor, float opacity,
              float borderWidth, FFDrawNode* drawNode, bool update);

    float getArtOpacity() override;                       // +0x10
    cocos2d::AffineTransform getArtTransform() override;  // +0x18

protected:
    cocos2d::Vec2 _position;  // +0x40  set by the free-circle init
};
