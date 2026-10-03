#include "GroupItem.h"

#include <algorithm>

#include "Box2D/Box2D.h"
#include "LevelItem.h"
#include "ShapeItem.h"
#include "base/ccMacros.h"

USING_NS_CC;

// FUN_005bf4b8 / FUN_005bf4cc are std::vector<GroupSpecial> / std::vector<ShapeItem*> length
// errors (library code instantiated by push_back).

// @005bec34
GroupItem::GroupItem()
    : _body(nullptr)
{
}

// @005bec58 (D2), @005becf0 (D0)
GroupItem::~GroupItem()
{
    for (auto it = _shapeItems.begin(); it != _shapeItems.end(); ++it)
    {
        if (*it != nullptr)
        {
            delete *it;
        }
    }
    _shapeItems.clear();
    _specials.clear();
}

// @005bed14
bool GroupItem::init(b2Body* body, float ptmRatio)
{
    _opacity = 1.0f;
    _body = body;
    _ptmRatio = ptmRatio;
    _shapeItems.clear();
    return true;
}

// @005bed38
int GroupItem::getIndex()
{
    return _index;
}

// @005bed40
void GroupItem::setIndex(int index)
{
    _index = index;
}

// @005bed48
void GroupItem::setOpacity(float opacity)
{
    _opacity = opacity;
    for (auto it = _specials.begin(); it != _specials.end(); ++it)
    {
        if (it->special != nullptr)
        {
            it->special->setOpacity(_opacity);
        }
    }
}

// @005beda4
float GroupItem::getArtOpacity()
{
    return _opacity;
}

// @005bedac
void GroupItem::setBody(b2Body* body)
{
    _body = body;
    updateTransform();
}

// @005bee50
void GroupItem::updateTransform()
{
    if (_body != nullptr)
    {
        _rotation = _body->GetAngle();
        _transform = AffineTransformTranslate(AffineTransformIdentity,
                                              _body->GetPosition().x * _ptmRatio,
                                              _ptmRatio * _body->GetPosition().y);
        _transform = AffineTransformRotate(_transform, _rotation);
    }
}

// @005beef4
b2Body* GroupItem::getBody()
{
    return _body;
}

// @005beefc
void GroupItem::update()
{
    updateTransform();
    float rotation = CC_RADIANS_TO_DEGREES(_rotation);
    for (auto it = _specials.begin(); it != _specials.end(); ++it)
    {
        if (it->special != nullptr)
        {
            Vec2 offset = it->offset;
            it->special->paintWithOffsetPoints(
                Vec2(_transform.a * offset.x + _transform.c * offset.y + _transform.tx,
                     _transform.b * offset.x + _transform.d * offset.y + _transform.ty),
                it->rotation - rotation);
        }
    }
}

// @005bf020
void GroupItem::addSpecial(GroupSpecial special)
{
    _specials.push_back(special);
}

// @005bf170
void GroupItem::addShapeItem(ShapeItem* shapeItem)
{
    shapeItem->setGroup(this);
    _shapeItems.push_back(shapeItem);
}

// @005bf2f0
void GroupItem::removeSprites()
{
    for (auto it = _specials.begin(); it != _specials.end(); ++it)
    {
        it->special->removeSprites();
    }
    _specials.clear();
    for (auto it = _shapeItems.begin(); it != _shapeItems.end(); ++it)
    {
        ShapeItem* shapeItem = *it;
        shapeItem->removeFromDrawNode();
        delete shapeItem;
    }
    _shapeItems.clear();
}

// @005bf380
void GroupItem::stopInteractivity()
{
    for (auto it = _specials.begin(); it != _specials.end(); ++it)
    {
        it->special->stopInteractivity();
    }
    if (_body != nullptr)
    {
        _body->GetWorld()->DestroyBody(_body);
        _body = nullptr;
    }
}

// @005bf3d8
void GroupItem::setImmovable(bool immovable)
{
    _body->SetType(immovable ? b2_staticBody : b2_dynamicBody);
    _immovable = immovable;
}

// @005bf410
AffineTransform GroupItem::getArtTransform()
{
    return _transform;
}

// @005bf424
float GroupItem::artOpacity()
{
    return _opacity;
}

// @005bf42c
bool GroupItem::getImmovable()
{
    return _immovable;
}

// @005bf434
void GroupItem::removeShapeItem(ShapeItem* shapeItem)
{
    auto it = std::find(_shapeItems.begin(), _shapeItems.end(), shapeItem);
    if (it != _shapeItems.end())
    {
        delete shapeItem;
        _shapeItems.erase(it);
    }
}
