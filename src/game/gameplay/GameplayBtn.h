#pragma once

#include "cocos2d.h"

#include <string>

// One on-screen gameplay button (forward, back, lean, special, eject, the ejected-pose buttons, and
// the separate pause/reset buttons). Owned and laid out by GameplayControls, which does all the touch
// tracking: a button is "pressed" while it holds a cocos2d::Touch (setTouch). The control byte of a
// frame is the sum of getStateValue() of every pressed button (see GameplayControls.h for the bits).
//
// Visual state: pressed = scale _adjustedScale * _userScale, opacity 255; released = scale
// _unpressedScale * _adjustedScale * _userScale, opacity _unpressedOpacity (102). Disabled buttons
// get opacity 0 (setEnabled(false)).
//
// arm64 sizeof 0x560 (createWithSpriteFrameName: operator new(0x560), no nothrow). The vtable only
// overrides the destructors (incl. the non-virtual thunks of Sprite's TextureProtocol base), so
// setPosition(Vec2) below is a new, non-virtual function that hides Sprite::setPosition.
// iOS counterpart: FFGameplayButton (defaultSprite/pressedSprite pair, _tag, _touch, _bounds).
class GameplayBtn : public cocos2d::Sprite
{
public:
    GameplayBtn();            // @005b41bc
    ~GameplayBtn() override;  // @005b424c (D1), @005b4258 (D0); thunks @005b4250, @005b427c

    // new + initWithSpriteFrameName (vptr+0x610) + autorelease; then _userScale = userScale,
    // released-state scale/opacity, _stateValue = stateValue. Returns nullptr (after delete) on
    // failure.
    static GameplayBtn* createWithSpriteFrameName(const std::string& spriteFrameName,
                                                  unsigned int stateValue, float userScale);  // @005b42a4
    // Same, then Sprite::setPosition(position), setHitArea(Rect::ZERO), setHitArea(hitArea).
    static GameplayBtn* createWithSpriteFrameName(const std::string& spriteFrameName,
                                                  cocos2d::Vec2 position, cocos2d::Rect hitArea,
                                                  unsigned int stateValue, float userScale);  // @005b43fc

    void setUserScale(float userScale);         // @005b4374  (no rescale)
    void showPressedState(bool pressed);        // @005b437c  scale + opacity only (replay display)
    void setStateValue(unsigned int value);     // @005b43f4
    // Not virtual: Sprite::setPosition(position) then setHitArea(Rect::ZERO).
    void setPosition(cocos2d::Vec2 position);   // @005b4564
    // Rect(0,0,0,0) => hit area = getBoundingBox() size centred on getPosition(); else as given.
    void setHitArea(cocos2d::Rect hitArea);     // @005b45d0
    // Shows the pressed (touch != nullptr) / released state, then stores the touch.
    void setTouch(cocos2d::Touch* touch);       // @005b4728
    cocos2d::Touch* getTouch();                 // @005b47ac
    cocos2d::Rect getHitArea();                 // @005b47b4
    unsigned int getStateValue();               // @005b47c0
    // Grows the hit area: width += right + left, height += top + bottom, x -= left, y -= bottom
    // (left-to-right float order: _hitArea.size.width + right + left).
    void nudgeBounds(float top, float right, float bottom, float left);  // @005b47c8
    // No-op if unchanged; else stores it and sets opacity (enabled ? _unpressedOpacity : 0).
    void setEnabled(bool enabled);              // @005b47fc
    bool getEnabled();                          // @005b482c
    void setAdjustedScale(float adjustedScale); // @005b4834  setScale(_unpressedScale * s * _userScale)

protected:
    float _userScale;              // +0x530  GameplayControls::_userScale ("controls_user_scale"); not set by the ctor
    bool _enabled;                 // +0x534  true
    float _adjustedScale;          // +0x538  1.0f
    unsigned int _stateValue;      // +0x53c  0; control bit(s) this button contributes (GameplayControls.h)
    cocos2d::Rect _hitArea;        // +0x540  Rect::ZERO (ctor default-constructs, then assigns ZERO)
    cocos2d::Touch* _touch;        // +0x550  nullptr; the touch currently holding this button
    // 102. Loaded with 4-byte loads (ldr w1) and passed to setOpacity(GLubyte), so an int, not a
    // GLubyte (setEnabled narrows the load to ldrb because only the low byte is used there).
    int _unpressedOpacity;         // +0x558
    float _unpressedScale;         // +0x55c  0.86f (0x3f5c28f6)
};
