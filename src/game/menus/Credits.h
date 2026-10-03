#pragma once

#include "cocos2d.h"

// Auto-scrolling credits list built from "credits.plist" ("credits": array of {"category",
// "items"}), optionally preceded by the ending header/message. Scrolls up _scrollSpeed per frame
// unless touched, can be dragged vertically, and wraps between 0 and _maxScrollY.
// Child of CreditsLayer.
//
// arm64 sizeof 0x320.
class Credits : public cocos2d::Node
{
public:
    Credits();                                                      // @005a6a3c  zeroes every field
    // Plain throwing `new Credits()` (no nothrow, no null check); init's result is ignored.
    static Credits* create(bool showEnding);                        // @005a6a80
    bool init(bool showEnding);                                     // @005a6ad8
    ~Credits() override;                                            // @005a774c (D1), @005a77e4 (D0)

    void removeTouchListener();                                     // @005a77a4
    void addTouchListener();                                        // @005a7930  fixed priority 1, swallows

    void update(float dt) override;                                 // @005a7b00  vptr+0x3d8

    // The listener lambdas forward to these.
    bool touchBegan(cocos2d::Touch* touch, cocos2d::Event* event);      // @005a7bc4
    void touchMoved(cocos2d::Touch* touch, cocos2d::Event* event);      // @005a7bd8
    void touchEnded(cocos2d::Touch* touch, cocos2d::Event* event);      // @005a7c94
    void touchCancelled(cocos2d::Touch* touch, cocos2d::Event* event);  // @005a7c9c

protected:
    cocos2d::Node* _container;                              // +0x2f8  all labels; x = visibleSize.width/2
    float _scrollSpeed;                                     // +0x300  2.5 (set in init)
    bool _showEnding;                                       // +0x304  ending header + message first
    bool _touching;                                         // +0x305  pauses auto-scroll
    float _maxScrollY;                                      // +0x308  wrap limit for _container's y
    cocos2d::EventListenerTouchOneByOne* _touchListener;    // +0x310  retained
};
