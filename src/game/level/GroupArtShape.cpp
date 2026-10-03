#include "GroupArtShape.h"

USING_NS_CC;

// @005beb88
bool GroupArtShape::init(Color4F innerColor)
{
    _innerColor = innerColor;
    _originalOpacity = innerColor.a;
    return true;
}

// @005beba8
void GroupArtShape::setOpacity(float opacity)
{
    _innerColor = Color4F(_innerColor.r, _innerColor.g, _innerColor.b, opacity);
}

// @005bec08
float GroupArtShape::getOpacity()
{
    return _innerColor.a;
}

// @005bec10
void GroupArtShape::setOutlineColor(Color4F outlineColor)
{
    _outlineColor = outlineColor;
    _outlineThickness = outlineColor.a != -1.0f;
}
