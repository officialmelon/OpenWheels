#pragma once

// GroupItem: a level "group" — one dynamic b2Body carrying several shapes (ShapeItems) and
// specials (LevelItems) that move together. Not a Ref. Primary (only) base FFDrawNodeDelegate:
// the group's ShapeItems delegate their art opacity/transform to it. sizeof 0x70.
// Vtable: [~GroupItem D1, D0, getArtOpacity, getArtTransform] (no new virtuals).

#include <vector>

#include "FFDrawNodeDelegate.h"
#include "math/CCAffineTransform.h"
#include "math/Vec2.h"

class b2Body;
class LevelItem;
class ShapeItem;

// A special inside a group (element of GroupItem::_specials, sizeof 0x18). Built by
// LevelB2D::addGroup; GroupItem::update repositions the special every frame with
// special->paintWithOffsetPoints(transform * offset, rotation - groupAngle (deg)).
struct GroupSpecial
{
    LevelItem* special;    // +0x00
    float rotation;        // +0x08  degrees (XML "p2", or "p4" for special type 3)
    cocos2d::Vec2 offset;  // +0x0c  points, relative to the group body
};

class GroupItem : public FFDrawNodeDelegate
{
public:
    GroupItem();
    virtual ~GroupItem();  // deletes the ShapeItems

    bool init(b2Body* body, float ptmRatio);  // _opacity = 1, returns true
    int getIndex();
    void setIndex(int index);
    void setOpacity(float opacity);  // also LevelItem::setOpacity on every special

    // FFDrawNodeDelegate
    float getArtOpacity() override;                       // +0x10  _opacity
    cocos2d::AffineTransform getArtTransform() override;  // +0x18  _transform

    void setBody(b2Body* body);  // also recomputes the transform when body != nullptr
    void updateTransform();
    b2Body* getBody();
    void update();
    void addSpecial(GroupSpecial special);
    void addShapeItem(ShapeItem* shapeItem);  // shapeItem->setGroup(this) + push_back
    void removeSprites();
    void stopInteractivity();
    void setImmovable(bool immovable);  // body type static (true) / dynamic (false)
    float artOpacity();
    bool getImmovable();
    void removeShapeItem(ShapeItem* shapeItem);  // erase + delete

protected:
    bool _immovable;                          // +0x08
    float _opacity;                           // +0x0c
    b2Body* _body;                            // +0x10  ctor nullptr
    int _index;                               // +0x18
    std::vector<GroupSpecial> _specials;      // +0x20
    std::vector<ShapeItem*> _shapeItems;      // +0x38
    cocos2d::AffineTransform _transform;      // +0x50  translate(body pos * ptm) . rotate(angle)
    float _ptmRatio;                          // +0x68
    float _rotation;                          // +0x6c  body angle (radians) at the last update
};
