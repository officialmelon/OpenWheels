#include "Backdrop.h"

#include "2d/CCSprite.h"

USING_NS_CC;

// @005824b0
Backdrop::Backdrop(Sprite* sprite, Vec2 origin, float multiplier)
{
    _multiplier = multiplier;
    _origin = origin;
    _sprite = sprite;
}

// @005824c4
Backdrop::~Backdrop()
{
}

// @005824c8
void Backdrop::update(Vec2 pos)
{
    _sprite->setPosition(Vec2(_origin.x + _multiplier * pos.x, _origin.y + _multiplier * pos.y));
}
