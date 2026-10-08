#include "PolygonShape.h"

#include "Box2D/Box2D.h"
#include "FFDrawNode.h"
#include "GroupItem.h"

USING_NS_CC;

// @006088ec
PolygonShape::PolygonShape()
    : _rotation(0.0f)
    , _position(Vec2::ZERO)
{
}

// @00608930 (D1), @00608934 (D0)
PolygonShape::~PolygonShape()
{
}

// @00608958
bool PolygonShape::init(b2Fixture* fixture, float ptmRatio, Color4F innerColor,
                        Color4F outlineColor, float opacity, float borderWidth,
                        FFDrawNode* drawNode)
{
    bool result = ShapeItem::init(fixture, ptmRatio, opacity, drawNode);
    if (result)
    {
        b2PolygonShape* shape = static_cast<b2PolygonShape*>(fixture->GetShape());
        Vec2 verts[100];
        int count = shape->m_count;
        for (unsigned int i = 0; i < (unsigned int)count; i++)
        {
            verts[i] = Vec2(shape->m_vertices[i].x * _ptmRatio, shape->m_vertices[i].y * _ptmRatio);
        }
        drawNode->drawPolyWithVerts(verts, count, innerColor, borderWidth, outlineColor, true,
                                    AffineTransformIdentity, AffineTransformIdentity, this);
    }
    return result;
}

// @00608b3c
bool PolygonShape::init(Vec2 pos, float rotationRadians, Vec2* verts, unsigned int count,
                        AffineTransform initialTransform, Color4F innerColor,
                        Color4F outlineColor, float opacity, float borderWidth,
                        FFDrawNode* drawNode, bool update)
{
    _position = pos;
    _rotation = rotationRadians;
    ShapeItem::init(nullptr, 1.0f, opacity, drawNode);  // result ignored in the original
    AffineTransform transform = AffineTransformRotate(
        AffineTransformTranslate(AffineTransformIdentity, pos.x, pos.y), rotationRadians);
    drawNode->drawPolyWithVerts(verts, count, innerColor, borderWidth, outlineColor, update,
                                initialTransform, transform, this);
    return true;
}

// ONLINE (PC addition): see PolygonShape.h.
bool PolygonShape::onlineInit(b2Fixture* fixture, float ptmRatio, Color4F innerColor,
                              Color4F outlineColor, float opacity, float borderWidth, Vec2* verts,
                              unsigned int count, FFDrawNode* drawNode)
{
    bool result = ShapeItem::init(fixture, ptmRatio, opacity, drawNode);
    if (result)
    {
        drawNode->drawPolyWithVerts(verts, (int)count, innerColor, borderWidth, outlineColor, true,
                                    AffineTransformIdentity, AffineTransformIdentity, this);
    }
    return result;
}

// @00608cb4
float PolygonShape::getArtOpacity()
{
    return ShapeItem::getArtOpacity();
}

// @00608cb8
AffineTransform PolygonShape::getArtTransform()
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
        AffineTransformTranslate(AffineTransformMakeIdentity(), _position.x, _position.y),
        _rotation);
}
