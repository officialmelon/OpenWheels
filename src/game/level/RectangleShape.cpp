#include "RectangleShape.h"

#include "Box2D/Box2D.h"
#include "FFDrawNode.h"
#include "GroupItem.h"

USING_NS_CC;

// @00608da4
RectangleShape::RectangleShape()
    : _position(Vec2::ZERO)
    , _rotation(0.0f)
{
}

// @00608de8 (D1), @00608dec (D0)
RectangleShape::~RectangleShape()
{
}

// @00608e10
bool RectangleShape::init(b2Fixture* fixture, float ptmRatio, Color4F innerColor,
                          Color4F outlineColor, float opacity, float borderWidth, float width,
                          float height, FFDrawNode* drawNode)
{
    bool result = ShapeItem::init(fixture, ptmRatio, opacity, drawNode);
    if (result)
    {
        Vec2 verts[4];
        verts[0] = Vec2(-(width * 0.5f), height * 0.5f);
        verts[1] = Vec2(-(width * 0.5f), -(height * 0.5f));
        verts[2] = Vec2(width * 0.5f, -(height * 0.5f));
        verts[3] = Vec2(width * 0.5f, height * 0.5f);
        // The colours' alpha is replaced by the shape opacity.
        innerColor.a = opacity;
        outlineColor.a = opacity;
        drawNode->drawPolyWithVerts(verts, 4, innerColor, borderWidth, outlineColor, true,
                                    AffineTransformIdentity, AffineTransformIdentity, this);
    }
    return result;
}

// @00608f2c
bool RectangleShape::init(Vec2 pos, float rotationRadians, AffineTransform initialTransform,
                          Color4F innerColor, Color4F outlineColor, float opacity,
                          float borderWidth, float width, float height, FFDrawNode* drawNode,
                          bool update)
{
    bool result = ShapeItem::init(nullptr, 1.0f, opacity, drawNode);
    if (result)
    {
        _position = pos;
        _rotation = rotationRadians;
        AffineTransform transform = AffineTransformRotate(
            AffineTransformTranslate(AffineTransformMakeIdentity(), _position.x, _position.y),
            _rotation);

        Vec2 verts[4];
        verts[0] = Vec2(-(width * 0.5f), height * 0.5f);
        verts[1] = Vec2(-(width * 0.5f), -(height * 0.5f));
        verts[2] = Vec2(width * 0.5f, -(height * 0.5f));
        verts[3] = Vec2(width * 0.5f, height * 0.5f);
        innerColor.a = opacity;
        outlineColor.a = opacity;
        drawNode->drawPolyWithVerts(verts, 4, innerColor, borderWidth, outlineColor, update,
                                    initialTransform, transform, this);
    }
    return result;
}

// @006090d8
float RectangleShape::getArtOpacity()
{
    return ShapeItem::getArtOpacity();
}

// @006090dc
AffineTransform RectangleShape::getArtTransform()
{
    if (_fixtureRef != nullptr)
    {
        return AffineTransformRotate(
            AffineTransformTranslate(AffineTransformMakeIdentity(),
                                     _fixtureRef->GetBody()->GetPosition().x * _ptmRatio,
                                     _ptmRatio * _fixtureRef->GetBody()->GetPosition().y),
            _fixtureRef->GetBody()->GetAngle());
    }
    else if (_group != nullptr)
    {
        return _group->getArtTransform();
    }
    return AffineTransformRotate(
        AffineTransformTranslate(AffineTransformMakeIdentity(), _position.x, _position.y),
        _rotation);
}
