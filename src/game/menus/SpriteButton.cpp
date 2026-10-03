#include "SpriteButton.h"

USING_NS_CC;

// @006332f0
SpriteButton::SpriteButton()
{
    _defaultSprite = nullptr;
    _disabledSprite = nullptr;
    _pressSprite = nullptr;
}

// @00633344 (D1), @00633358 (D0)
SpriteButton::~SpriteButton()
{
}

// @0063337c
bool SpriteButton::init()
{
    return true;
}

// @00633384
void SpriteButton::setSprites(Sprite* defaultSprite, Sprite* disabledSprite, Sprite* pressSprite)
{
    _defaultSprite = defaultSprite;
    _disabledSprite = disabledSprite;
    _pressSprite = pressSprite;
}

// @00633390
void SpriteButton::setHitArea(Rect hitArea)
{
    // Only an empty rect is replaced (by the default sprite's bounding box); a real one is dropped.
    if ((hitArea.size.width <= 0.0f || hitArea.size.height <= 0.0f) && _defaultSprite != nullptr)
    {
        _hitArea = _defaultSprite->getBoundingBox();
    }
}
