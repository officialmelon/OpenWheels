#pragma once

#include "cocos2d.h"

#include <string>

// The "victory" banner of the level-complete screen, created by VictoryMenu::addMenu (z -1, just
// above the menu panel): "victory_banner.png" scaling in, the placement text ("your best time!",
// ...) in globals::colors::yellow, the completion time ("<t> seconds", 2 decimals) in globals::colors::blue when _time > 0, and four
// decoration sprites that pop out to both sides of the panel.
//
// arm64 sizeof 0x330 (VictoryMenu::addMenu: operator new(0x330), no nothrow, then autorelease and
// init; there is no create()). 0x330 = data end 0x328 rounded up to Node's 16-byte alignment.
// TU statics: an unused std::string "ClarendonLTStd-Bold" (_INIT_19, @00ac65c8).
// iOS counterpart: VictoryAnimation : CCLayer (victoryBanner, sbn, _placementString, counter,
// _timeLabel, _placementLabel, bgWidth, spriteScale, time).
class VictoryAnimation : public cocos2d::Node
{
public:
    VictoryAnimation();             // @006400d8  _placementString = "", labels nullptr, _bgWidth 0
    ~VictoryAnimation() override;   // @0064015c (D1), @0064019c (D0)

    // Not virtual (hides Node::init). Stores the arguments, createAnimation(), returns true.
    bool init(float time, std::string placementString, float bgWidth);  // @006401c0
    void createAnimation();         // @00640204

protected:
    float _time;                       // +0x2f8  completion time in seconds; not set by the ctor
    float _bgWidth;                    // +0x2fc  width of VictoryMenu's panel (decoration x offsets)
    std::string _placementString;      // +0x300  "" when the run did not place
    cocos2d::Label* _placementLabel;   // +0x318  "fonts/ClarendonLTStd-Bold.ttf" 73, yellow
    cocos2d::Label* _timeLabel;        // +0x320  "fonts/ClarendonLTStd-Bold.ttf" 96, blue (only when _time > 0)
};
