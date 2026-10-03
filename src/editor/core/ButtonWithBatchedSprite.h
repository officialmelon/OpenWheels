#pragma once
// iOS ButtonWithBatchedSprite : CCSprite (instanceSize 0x2d8) - a sprite button that can live in a
// CCSpriteBatchNode: it handles its own touches (targeted delegate) and swaps in sibling state
// sprites (press / disabled) instead of using a CCMenu. Base class of EditorLayerButton; the
// Android build has no counterpart (its menus use cocos2d::Menu), so the editor ports it.
//
// Touch handling (iOS: CCTouchDispatcher targeted delegate, priority 0, swallowsTouches =
// _swallowsTouch, registered in onEnterTransitionDidFinish, removed in onExit) -> a cocos2d-x
// EventListenerTouchOneByOne with fixed priority kTouchPriority. cocos2d-iphone gives equal-priority
// handlers registered later precedence (insertion before equals), cocos2d-x the opposite, so the
// editor's fixed priorities are chosen to keep the iOS order (see EditorLayer.h, "Touch order").
//
// Targets/selectors (NSInvocation with the button as argument) -> std::function<void(sender)>.
// `userObject` (id) -> cocos2d::Value (EditorLayer stores an NSNumber float in the undo button).

#include <functional>

#include "2d/CCSprite.h"
#include "base/CCValue.h"
#include "EditorGeometry.h"

namespace cocos2d {
class EventListenerTouchOneByOne;
class Touch;
class Event;
}

class ButtonWithBatchedSprite : public cocos2d::Sprite
{
public:
    using Callback = std::function<void(ButtonWithBatchedSprite* sender)>;

    // Fixed priority of the touch listener (iOS targeted priority 0; see header comment).
    static const int kTouchPriority;

    // +[CCSprite spriteWithSpriteFrameName:] on this class.
    static ButtonWithBatchedSprite* createWithSpriteFrameName(const std::string& spriteFrameName);

    // bounds = (0, 0, contentSize), center = contentSize / 2, enabled, swallowsTouch,
    // hideDefaultOnPress, defaultSprite = self.
    bool initWithSpriteFrame(cocos2d::SpriteFrame* spriteFrame) override;  // @ios 1000297a4
    // dealloc                                                             // @ios 1000298c8

    bool isPressed();                                                      // @ios 100029948
    // A held holdable button is released (default state) before the flag changes.
    virtual void setIsEnabled(bool isEnabled);                             // @ios 100029978
    void setBounds(const cg::Rect& bounds);                                // @ios 1000299d0
    void onEnterTransitionDidFinish() override;                            // @ios 1000299e8
    void onExit() override;                                                // @ios 100029a50
    // Scheduled (interval rateLimit) after a non-holdable press: back to the default state.
    void setToUpState(float dt);                                           // @ios 100029ab4

    virtual bool ccTouchBegan(cocos2d::Touch* touch, cocos2d::Event* event);      // @ios 100029aec
    virtual void ccTouchMoved(cocos2d::Touch* touch, cocos2d::Event* event);      // @ios 100029cc4
    virtual void ccTouchEnded(cocos2d::Touch* touch, cocos2d::Event* event);      // @ios 100029db0
    void invokeOnReleaseMethod();                                          // @ios 100029e38
    virtual void ccTouchCancelled(cocos2d::Touch* touch, cocos2d::Event* event);  // @ios 100029e48

    void showDefaultState();                                               // @ios 100029e94
    void showPressedState();                                               // @ios 100029efc
    void showDisabledState();                                              // @ios 100029f78
    void setDisabledSprite(cocos2d::Sprite* sprite);                       // @ios 100029fe4
    void setPressSprite(cocos2d::Sprite* sprite);                          // @ios 10002a0a0
    void setIsHoldable(bool isHoldable);                                   // @ios 10002a14c
    void setOnPressTarget(Callback callback);                              // @ios 10002a15c  setOnPressTarget:selector:
    void setOnRollOffTarget(Callback callback);                            // @ios 10002a1ec  setOnRollOffTarget:selector:
    void setOnReleaseTarget(Callback callback);                            // @ios 10002a27c  setOnReleaseTarget:selector:
    // Moves the state sprites along (they are siblings in the batch node).
    void setPosition(const cocos2d::Vec2& position) override;              // @ios 10002a320
    void setPosition(float x, float y) override;  // port: holds the body (cocos routes Vec2 here)
    void removeStateSprites();                                             // @ios 10002a3a0  (empty)
    void die();                                                            // @ios 10002a3a4
    void alignSprites();                                                   // @ios 10002a3e0
    void padHitSpace(float padding);                                       // @ios 10002a4d8
    void setIsPressed(bool isPressed);                                     // @ios 10002a510
    bool isHoldable();                                                     // @ios 10002a520
    bool isToggleable();                                                   // @ios 10002a530
    void setIsToggleable(bool isToggleable);                               // @ios 10002a540
    float rateLimit();                                                     // @ios 10002a550
    void setRateLimit(float rateLimit);                                    // @ios 10002a560
    cocos2d::Sprite* disabledSprite();                                     // @ios 10002a570
    cocos2d::Sprite* pressSprite();                                        // @ios 10002a580
    bool isEnabled();                                                      // @ios 10002a590
    cg::Rect bounds();                                                     // @ios 10002a5a0
    const cocos2d::Value& userObject();                                    // @ios 10002a5b8
    void setUserObject(const cocos2d::Value& userObject);                  // @ios 10002a5c8
    bool swallowsTouch();                                                  // @ios 10002a5d4
    void setSwallowsTouch(bool swallowsTouch);                             // @ios 10002a5e4
    bool hideDefaultOnPress();                                             // @ios 10002a5f4
    void setHideDefaultOnPress(bool hideDefaultOnPress);                   // @ios 10002a608

protected:
    ButtonWithBatchedSprite();
    ~ButtonWithBatchedSprite() override;

    // iOS ivars (offsets from the ObjC metadata).
    cg::Point center;                       // +0x258
    cg::Rect _bounds;                       // +0x268  iOS "bounds" (clashes with bounds())
    bool _isPressed = false;                // +0x288  iOS "isPressed"
    bool _isEnabled = true;                 // +0x289  iOS "isEnabled"
    bool _isHoldable = false;               // +0x28a  iOS "isHoldable"
    bool _isToggleable = false;             // +0x28b  iOS "isToggleable"
    float _rateLimit = 0.0f;                // +0x28c  iOS "rateLimit"
    cocos2d::Sprite* defaultSprite = nullptr;   // +0x290  (self; not retained)
    cocos2d::Sprite* _disabledSprite = nullptr; // +0x298  iOS "disabledSprite" (retained)
    cocos2d::Sprite* _pressSprite = nullptr;    // +0x2a0  iOS "pressSprite" (retained)
    Callback onPressInvocation;             // +0x2a8
    Callback onRollOffInvocation;           // +0x2b0
    Callback onReleaseInvocation;           // +0x2b8
    std::string name;                       // +0x2c0
    bool _swallowsTouch = true;             // +0x2c8
    bool _hideDefaultOnPress = true;        // +0x2c9
    cocos2d::Value _userObject;             // +0x2d0  iOS "userObject"

    // port: the targeted-delegate registration.
    cocos2d::EventListenerTouchOneByOne* _touchListener = nullptr;
};
