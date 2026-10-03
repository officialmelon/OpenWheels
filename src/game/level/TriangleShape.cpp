#include "TriangleShape.h"

#include "Box2D/Box2D.h"
#include "FFDrawNode.h"
#include "GroupItem.h"

USING_NS_CC;

// @00636ef4
TriangleShape::TriangleShape()
    : _position(Vec2::ZERO)
    , _rotation(0.0f)
{
}

// @00636f38 (D1), @00636f3c (D0)
TriangleShape::~TriangleShape()
{
}

// @00636f60
bool TriangleShape::init(b2Fixture* fixture, float ptmRatio, Color4F innerColor,
                         Color4F outlineColor, float opacity, float borderWidth, float width,
                         float height, FFDrawNode* drawNode)
{
    bool result = ShapeItem::init(fixture, ptmRatio, opacity, drawNode);
    if (result)
    {
        float third = height / 3.0f;
        Vec2 verts[3];
        verts[0] = Vec2(-(width * 0.5f), -third);
        verts[1] = Vec2(width * 0.5f, -third);
        verts[2] = Vec2(0.0f, third + third);
        // The colours' alpha is replaced by the shape opacity.
        innerColor.a = opacity;
        outlineColor.a = opacity;
        drawNode->drawPolyWithVerts(verts, 3, innerColor, borderWidth, outlineColor, true,
                                    AffineTransformIdentity, AffineTransformIdentity, this);
    }
    return result;
}

// @00637084
bool TriangleShape::init(Vec2 pos, float rotationRadians, AffineTransform initialTransform,
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

        float third = height / 3.0f;
        Vec2 verts[3];
        verts[0] = Vec2(-(width * 0.5f), -third);
        verts[1] = Vec2(width * 0.5f, -third);
        verts[2] = Vec2(0.0f, third + third);
        innerColor.a = opacity;
        outlineColor.a = opacity;
        drawNode->drawPolyWithVerts(verts, 3, innerColor, borderWidth, outlineColor, update,
                                    initialTransform, transform, this);
    }
    return result;
}

// @00637238
float TriangleShape::getArtOpacity()
{
    return ShapeItem::getArtOpacity();
}

// @0063723c
AffineTransform TriangleShape::getArtTransform()
{
    if (_fixtureRef != nullptr)
    {
        return AffineTransformRotate(
            AffineTransformTranslate(AffineTransformIdentity,
                                     _fixtureRef->GetBody()->GetPosition().x * _ptmRatio,
                                     _ptmRatio * _fixtureRef->GetBody()->GetPosition().y),
            _fixtureRef->GetBody()->GetAngle());
    }
    else if (_group != nullptr)
    {
        return _group->getArtTransform();
    }
    return AffineTransformRotate(
        AffineTransformTranslate(AffineTransformIdentity, _position.x, _position.y), _rotation);
}
