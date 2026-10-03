#include "ShapeItem.h"

#include "FFDrawNode.h"
#include "GroupItem.h"
#include "ShapeItemDelegate.h"

// @00612b24
ShapeItem::ShapeItem()
    : _fixtureRef(nullptr)
    , _ptmRatio(1.0f)
    , _group(nullptr)
    , _isStatic(false)
    , _index(-1)
    , _opacity(1.0f)
    , _drawNode(nullptr)
    , _delegate(nullptr)
{
}

// @00612b58 (D1), @00612b5c (D0)
ShapeItem::~ShapeItem()
{
}

// @00612b80
bool ShapeItem::init(b2Fixture* fixture, float ptmRatio, float opacity, FFDrawNode* drawNode)
{
    _opacity = opacity;
    _ptmRatio = ptmRatio;
    _fixtureRef = fixture;
    _drawNode = drawNode;
    _delegate = nullptr;
    return true;
}

// @00612b9c
void ShapeItem::setIndex(int index)
{
    _index = index;
}

// @00612ba4
int ShapeItem::getIndex()
{
    return _index;
}

// @00612bac
float ShapeItem::getPtmRatio()
{
    return _ptmRatio;
}

// @00612bb4
void ShapeItem::setPtmRatio(float ptmRatio)
{
    _ptmRatio = ptmRatio;
}

// @00612bbc
FFDrawNode* ShapeItem::getDrawNode()
{
    return _drawNode;
}

// @00612bc4
void ShapeItem::setDrawNode(FFDrawNode* drawNode)
{
    _drawNode = drawNode;
}

// @00612bcc
ShapeItemDelegate* ShapeItem::getDelegate()
{
    return _delegate;
}

// @00612bd4
void ShapeItem::setDelegate(ShapeItemDelegate* delegate)
{
    _delegate = delegate;
}

// @00612bdc
void ShapeItem::setFixtureRef(b2Fixture* fixture)
{
    _fixtureRef = fixture;
}

// @00612be4
b2Fixture* ShapeItem::getFixtureRef()
{
    return _fixtureRef;
}

// @00612bec
float ShapeItem::getOpacity()
{
    return _opacity;
}

// @00612bf4
void ShapeItem::setOpacity(float opacity)
{
    _opacity = opacity;
}

// @00612bfc
cocos2d::Vec2 ShapeItem::getArtPos()
{
    return cocos2d::Vec2::ZERO;
}

// @00612c0c
float ShapeItem::getArtRotation()
{
    return 0.0f;
}

// @00612c14
float ShapeItem::getArtOpacity()
{
    if (_group != nullptr)
    {
        return _group->getArtOpacity() * _opacity;
    }
    return _opacity;
}

// @00612c54
cocos2d::AffineTransform ShapeItem::getArtTransform()
{
    return cocos2d::AffineTransformIdentity;
}

// @00612c70
void ShapeItem::removeFromDrawNode()
{
    if (_drawNode != nullptr)
    {
        _drawNode->removeDelegate(this);
    }
}

// @00612c84
void ShapeItem::removeFromOwner(bool deleteItem)
{
    if (_delegate != nullptr)
    {
        _delegate->removeShapeItem(this, deleteItem);
    }
}

// @00612ca8
void ShapeItem::setStatic(bool isStatic)
{
    _drawNode->setArtDelegateToStatic(isStatic, getArtTransform(), this);
}

// @00612d20
void ShapeItem::setGroup(GroupItem* group)
{
    _group = group;
}
