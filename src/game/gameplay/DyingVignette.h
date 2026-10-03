#pragma once

#include "cocos2d.h"

// Red pulsing screen-edge vignette + looping heartbeat while the character is dying
// (Gameplay::handleDying adds it at z 3; Gameplay::handleDead calls stop()).
// init(): Sprite::create("images/dying_vignette.png") loaded as RGBA8888
// (Texture2D default pixel format 2 = RGBA8888 while creating), 9-slice centre rect (0.49,0.49,0.02,0.02),
// stretched to the visible size, opacity 0, RepeatForever(FadeTo(0.2,200), FadeTo(1.0,0));
// Sound::playSound("HeartBeat", looping).
//
// arm64 sizeof 0x310 (Gameplay::handleDying: new(nothrow) 0x310, an inlined CREATE_FUNC).
// No iOS class (GameplayLayer::addDyingVignette with _dyingVignette/_heartbeat ivars).
class DyingVignette : public cocos2d::Node
{
public:
    DyingVignette();             // @005ac61c
    ~DyingVignette() override;   // @005ac658 (D1), @005ac65c (D0)

    // Inlined into Gameplay::handleDying; no symbol.
    CREATE_FUNC(DyingVignette);

    bool init() override;        // @005ac680  vptr+0x4f8
    // Stops the heartbeat (if still playing), then Node::onExit().
    void onExit() override;      // @005ac8f0  vptr+0x330
    // FadeOut(1.0) + CallFunc(fadeOutComplete) on the vignette; stops the heartbeat.
    void stop();                 // @005ac920
    void fadeOutComplete();      // @005aca78  (empty)

protected:
    cocos2d::Sprite* _vignette;  // +0x2f8  nullptr
    int _heartbeatSoundId;       // +0x300  -1 = none (Sound::playSound id)
    // sizeof 0x310 = 0x304 rounded up to Node's 16-byte alignment (std::function members).
};
