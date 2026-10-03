#pragma once

// Pure interface (vptr only) for objects whose polygons are batched into an FFDrawNode.
// Implemented by ShapeItem and GroupItem (both have it as their primary base at offset 0).
// GroupItem's vtable is exactly {~dtor, ~dtor, getArtOpacity, getArtTransform}, so the virtual
// destructor belongs to this interface; ShapeItem's getArtPos/getArtRotation are its own.
// FFDrawNode::updateVerts calls getArtOpacity (+0x10) and getArtTransform (+0x18).

#include "math/CCAffineTransform.h"

class FFDrawNodeDelegate
{
public:
    virtual ~FFDrawNodeDelegate() {}                         // +0x00 / +0x08
    virtual float getArtOpacity() = 0;                       // +0x10
    virtual cocos2d::AffineTransform getArtTransform() = 0;  // +0x18
};
