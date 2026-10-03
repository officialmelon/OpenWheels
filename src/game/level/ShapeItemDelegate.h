#pragma once

// Owner of ShapeItems (ShapeItem::_delegate, +0x38). Implemented by LevelB2D as its second base
// (sub-object at LevelB2D+0x08). Vtable: [~dtor D1, ~dtor D0, removeShapeItem].
// ShapeItem::removeFromOwner(bool) calls removeShapeItem (vptr+0x10).

class ShapeItem;

class ShapeItemDelegate
{
public:
    virtual ~ShapeItemDelegate() {}                                         // +0x00 / +0x08
    virtual void removeShapeItem(ShapeItem* shapeItem, bool deleteItem) = 0; // +0x10
};
