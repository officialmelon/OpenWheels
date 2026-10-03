#pragma once

// GroupArtShape: colour/outline description of an art shape. Plain class (no vtable, not a Ref).
// Dead code on Android: nothing constructs GroupArtShape / CircleArtShape / PolygonArtShape; only
// their methods were compiled. sizeof 0x38 (CircleArtShape/PolygonArtShape members start at 0x38).
// Members at 0x10..0x1b and 0x30 are never accessed on Android; their declarations follow the iOS
// original's ivars (fixtureRef, index) and are layout placeholders.

#include "base/ccTypes.h"

class b2Fixture;

class GroupArtShape
{
public:
    bool init(cocos2d::Color4F innerColor);  // also sets _originalOpacity = innerColor.a
    void setOpacity(float opacity);          // replaces _innerColor.a
    float getOpacity();                      // _innerColor.a
    void setOutlineColor(cocos2d::Color4F outlineColor);

protected:
    cocos2d::Color4F _innerColor;      // +0x00
    b2Fixture* _fixtureRef;            // +0x10  RE-TODO(@005beb88): never accessed; iOS ivar
    unsigned int _index;               // +0x18  RE-TODO(@005beb88): never accessed; iOS ivar
    cocos2d::Color4F _outlineColor;    // +0x1c
    unsigned int _outlineThickness;    // +0x2c  = (outlineColor.a != -1.0f)
    unsigned int _unk0x30;             // +0x30  RE-TODO(@005beb88): never accessed, purpose unknown
    float _originalOpacity;            // +0x34
};
