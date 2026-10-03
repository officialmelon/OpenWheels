#pragma once

#include "cocos2d.h"

// Credits screen: MenuHelper background, overlay and back button, plus a translucent black
// LayerColor. The Credits node is added once the enter transition has finished.
// Pushed by InfoMenu (showEnding = false) and by LevelSelectMenu (true, after the ending).
//
// arm64 sizeof 0x320. The only field lives in cocos2d::Layer's tail padding.
class CreditsLayer : public cocos2d::Layer
{
public:
    CreditsLayer();                                         // @005a82e0
    ~CreditsLayer() override;                               // @005a8310 (D1), @005a8324 (D0)

    static cocos2d::Scene* createScene(bool showEnding);    // @005a8348  (create() inlined)
    static CreditsLayer* create(bool showEnding);           // @005a83f8
    bool init(bool showEnding);                             // @005a8484

    // Back button: replaceScene with a fade to MainMenu. Does not use `this`.
    void backBtnPressed();                                  // @005a8604

    void onEnterTransitionDidFinish() override;             // @005a86a8  vptr+0x328

protected:
    bool _showEnding;   // +0x31d  not initialised by the ctor; set in init()
};
