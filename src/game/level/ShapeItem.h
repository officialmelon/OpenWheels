#pragma once

// ShapeItem: art + physics bookkeeping for one level shape (a fixture's polygon/circle drawn in an
// FFDrawNode). Not a cocos2d::Ref. Primary (only) base FFDrawNodeDelegate; own fields end at 0x40.
// Vtable: [~ShapeItem D1, D0, getArtOpacity, getArtTransform, getArtPos, getArtRotation].
// Concrete kinds: CircleShape, RectangleShape, PolygonShape, TriangleShape, TerrainShape.

#include "FFDrawNodeDelegate.h"
#include "math/CCAffineTransform.h"
#include "math/Vec2.h"

class b2Fixture;
class FFDrawNode;
class GroupItem;
class ShapeItemDelegate;

class ShapeItem : public FFDrawNodeDelegate
{
public:
    ShapeItem();
    virtual ~ShapeItem();

    // Stores the arguments, clears _delegate, returns true.
    bool init(b2Fixture* fixture, float ptmRatio, float opacity, FFDrawNode* drawNode);

    void setIndex(int index);
    int getIndex();
    float getPtmRatio();
    void setPtmRatio(float ptmRatio);
    FFDrawNode* getDrawNode();
    void setDrawNode(FFDrawNode* drawNode);
    ShapeItemDelegate* getDelegate();
    void setDelegate(ShapeItemDelegate* delegate);
    void setFixtureRef(b2Fixture* fixture);
    b2Fixture* getFixtureRef();
    float getOpacity();
    void setOpacity(float opacity);

    // FFDrawNodeDelegate
    float getArtOpacity() override;                       // +0x10
    cocos2d::AffineTransform getArtTransform() override;  // +0x18

    // New virtuals (declaration order == vtable order)
    virtual cocos2d::Vec2 getArtPos();  // +0x20  Vec2::ZERO
    virtual float getArtRotation();     // +0x28  0

    void removeFromDrawNode();               // _drawNode->removeDelegate(this)
    void removeFromOwner(bool deleteItem);   // _delegate->removeShapeItem(this, deleteItem)
    void setStatic(bool isStatic);           // _drawNode->setArtDelegateToStatic(...)
    void setGroup(GroupItem* group);

protected:
    b2Fixture* _fixtureRef;         // +0x08
    float _ptmRatio;                // +0x10  ctor 1.0
    GroupItem* _group;              // +0x18  ctor nullptr
    bool _isStatic;                 // +0x20  ctor false. RE-TODO(@00612b24): never read or written
                                    //        anywhere else in the binary; name is a guess.
    int _index;                     // +0x24  ctor -1
    float _opacity;                 // +0x28  ctor 1.0
    FFDrawNode* _drawNode;          // +0x30  ctor nullptr
    ShapeItemDelegate* _delegate;   // +0x38  ctor nullptr
};
