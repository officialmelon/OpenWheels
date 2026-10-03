#pragma once

#include "cocos2d.h"

#include <string>

// The level-complete menu (Gameplay::handleLevelComplete adds it at z 7): a 9-slice "popUp_scale9.png"
// panel that slides up (Delay 0.15, EaseExponentialOut(MoveTo 0.5)), a row of pause-menu style
// buttons and a VictoryAnimation on top. Button tags are GameplayMenuAction values (Gameplay.h):
// exit 1 (pink), reset 2 (blue), change character 4 (blue, only when the level does not force a
// character), view replay 5 (blue), next level 6 (pink, not for chapters 5000/5001). btnPressed
// dispatches "gameplayMenuAction" with the tag (tag 3 is ignored).
//
// arm64 sizeof 0x340 (handleLevelComplete: operator new(0x340), no nothrow, autorelease, then
// init(time, placement, Size::ZERO)). 0x340 = data end 0x338 rounded up to Node's 16-byte alignment.
// iOS counterpart: VictoryMenu : GameplayMenu.
class VictoryMenu : public cocos2d::Layer
{
public:
    VictoryMenu();             // @00640b44
    ~VictoryMenu() override;   // @00640b90 (D1), @00640ba4 (D0)

    // Not virtual (hides Layer::init). Stores the arguments, addMenu(), returns true.
    // placement: Settings::addCompletionTime result (1..4 = rank, 0 = not placed / replay).
    bool init(float time, int placement, cocos2d::Size bannerSize);  // @00640bc8
    void addBg();              // @00640c04  (empty)
    void addMenu();            // @00640c08
    // Same art as PauseLayer::btnWithIcon (0 pink, 1 blue), callback btnPressed.
    cocos2d::MenuItemImage* btnWithIcon(std::string iconFrameName, int color, int tag);  // @00641534
    void introComplete();      // @00641938  _menu->setVisible(true)
    void btnPressed(cocos2d::Ref* sender);  // @006419b4

protected:
    cocos2d::Menu* _menu;       // +0x320  invisible until introComplete()
    float _time;                // +0x328
    int _placement;             // +0x32c  1..4 -> "your best time!", "your 2nd best time!",
                                //         "your 3rd best time!", "your worst time!" (table @0041e52c)
    // Height is subtracted from the panel's vertical placement (room for a banner ad); always
    // Size::ZERO in 1.1.3. RE-TODO(@00640c08): name guessed from the use.
    cocos2d::Size _bannerSize;  // +0x330  ctor default-constructs
};
