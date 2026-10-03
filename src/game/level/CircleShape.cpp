#include "CircleShape.h"

#include "Box2D/Box2D.h"
#include "FFDrawNode.h"
#include "GroupItem.h"

USING_NS_CC;

// ~CircleShape() @005a560c (D0 only): implicit destructor.

// @005a53a4
bool CircleShape::init(b2Fixture* fixture, float ptmRatio, Color4F innerColor, Color4F outlineColor,
                       float opacity, float borderWidth, float radius, FFDrawNode* drawNode)
{
    bool result = ShapeItem::init(fixture, ptmRatio, opacity, drawNode);
    if (result)
    {
        drawNode->drawDotWithOffset(Vec2::ZERO, Vec2::ZERO, radius, innerColor, borderWidth,
                                    outlineColor, true, this);
    }
    return result;
}

// @005a5450
bool CircleShape::init(Vec2 pos, Vec2 initialOffset, float radius, Color4F innerColor,
                       Color4F outlineColor, float opacity, float borderWidth, FFDrawNode* drawNode,
                       bool update)
{
    bool result = ShapeItem::init(nullptr, 1.0f, opacity, drawNode);
    if (result)
    {
        _position = pos;
        drawNode->drawDotWithOffset(pos, initialOffset, radius, innerColor, borderWidth,
                                    outlineColor, update, this);
    }
    return result;
}

// @005a5534
float CircleShape::getArtOpacity()
{
    return ShapeItem::getArtOpacity();
}

// @005a5538
AffineTransform CircleShape::getArtTransform()
{
    if (_fixtureRef != nullptr)
    {
        b2Vec2 position = _fixtureRef->GetBody()->GetPosition();
        return AffineTransformTranslate(AffineTransformMakeIdentity(), position.x * _ptmRatio,
                                        position.y * _ptmRatio);
    }
    else if (_group != nullptr)
    {
        return _group->getArtTransform();
    }
    return AffineTransformTranslate(AffineTransformMakeIdentity(), _position.x, _position.y);
}
