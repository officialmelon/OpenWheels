#include "HighlightSprite.h"

USING_NS_CC;

// @00649008
HighlightSprite::HighlightSprite()
{
}

// @00649038 (D1), @0064903c (D0)
HighlightSprite::~HighlightSprite()
{
}

// @00649060
bool HighlightSprite::init()
{
    _yRange = 150.0f;

    _sprite = Sprite::create("images/yellow_arrow.png");
    _sprite->setOpacity(0);
    _sprite->setAnchorPoint(Vec2(0.5f, 0.0f));
    _sprite->setPosition(Vec2(0.0f, _yRange));
    addChild(_sprite);

    auto fadeIn = EaseSineInOut::create(FadeIn::create(0.5f));
    auto moveUp = EaseSineInOut::create(MoveTo::create(0.25f, Vec2(0.0f, _yRange * 0.25f)));
    auto moveDown = EaseSineInOut::create(MoveTo::create(0.25f, Vec2(0.0f, 0.0f)));
    auto bounce = RepeatForever::create(Sequence::create(moveDown, moveUp, nullptr));

    _sprite->runAction(fadeIn);
    _sprite->runAction(bounce);
    return true;
}

// @00649224
void HighlightSprite::fadeOut()
{
    if (_sprite != nullptr)
    {
        auto fade = EaseSineInOut::create(FadeOut::create(0.25f));
        auto done = CallFunc::create(CC_CALLBACK_0(HighlightSprite::fadeOutComplete, this));
        _sprite->runAction(Sequence::create(fade, done, nullptr));
    }
}

// @00649364
void HighlightSprite::fadeOutComplete()
{
    removeFromParent();
}
