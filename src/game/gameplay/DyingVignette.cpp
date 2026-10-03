#include "DyingVignette.h"

#include "Sound.h"

USING_NS_CC;

// @005ac61c
DyingVignette::DyingVignette()
    : _vignette(nullptr),
      _heartbeatSoundId(-1)
{
}

// @005ac658 (D1), @005ac65c (D0)
DyingVignette::~DyingVignette()
{
}

// @005ac680
bool DyingVignette::init()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    Texture2D::PixelFormat pixelFormat = Texture2D::getDefaultAlphaPixelFormat();
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    _vignette = Sprite::create("images/dying_vignette.png");
    _vignette->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    _vignette->setCenterRectNormalized(Rect(0.49f, 0.49f, 0.02f, 0.02f));
    _vignette->setPosition(origin.x + visibleSize.width * 0.5f,
                           origin.y + visibleSize.height * 0.5f);
    _vignette->setContentSize(visibleSize);
    _vignette->setOpacity(0);
    addChild(_vignette);
    Texture2D::setDefaultAlphaPixelFormat(pixelFormat);

    _vignette->runAction(RepeatForever::create(
        Sequence::create(FadeTo::create(0.2f, 200), FadeTo::create(1.0f, 0), nullptr)));
    _heartbeatSoundId = Sound::playSound("HeartBeat", 1.0f, 1.0f, 0.0f, true);
    return true;
}

// @005ac8f0
void DyingVignette::onExit()
{
    if (_heartbeatSoundId != -1)
    {
        Sound::stopSound(_heartbeatSoundId);
    }
    Node::onExit();
}

// @005ac920
void DyingVignette::stop()
{
    if (_vignette)
    {
        _vignette->stopAllActions();
        _vignette->runAction(Sequence::create(
            FadeOut::create(1.0f), CallFunc::create(CC_CALLBACK_0(DyingVignette::fadeOutComplete, this)),
            nullptr));
    }
    if (_heartbeatSoundId != -1)
    {
        Sound::stopSound(_heartbeatSoundId);
        _heartbeatSoundId = -1;
    }
}

// @005aca78
void DyingVignette::fadeOutComplete()
{
}
