#pragma once

#include "cocos2d.h"

#include <string>

class Session;

// The level timer label ("fonts/timer.fnt" BMFont, initial text "0.00"), z 6 in Gameplay. Text is
// [M:]SS[.hh]: minutes only when >= 1, hundredths only while minutes < 10. The label is centred
// horizontally on the timer background sprite (GameplayControls::getTimerBg) and re-centred when the
// text first grows a minutes digit / a second minutes digit.
//
// arm64 sizeof 0x380 (create: new(nothrow) 0x380). Only the destructors and init() are virtual
// overrides; update() is a new non-virtual function that hides Node::update(float).
// No iOS class (GameplayLayer used a CCLabelBMFont "timer" ivar and updateTime).
class GameplayTimer : public cocos2d::Node
{
public:
    // Inline: there is no constructor symbol; its member initialisation is inlined into create().
    GameplayTimer() {}
    ~GameplayTimer() override;   // @005bde9c (D2), @005bdf28 (D0)

    // Inline in the header; the only out-of-line copy is emitted in Gameplay's TU.
    // @005b9460
    CREATE_FUNC(GameplayTimer);

    // _session = Settings::getInstance()->getCurrentSession(); label; _time = 0; updateLabel().
    bool init() override;                              // @005bd744  vptr+0x4f8
    void updateLabel();                                // @005bd898
    void setTimeLimit(float timeLimit);                // @005bdd44  (no callers)
    void setBackgroundSprite(cocos2d::Sprite* sprite); // @005bdd4c  then alignText()
    void alignText();                                  // @005bdd54
    float getTime();                                   // @005bde30
    void reset(float timeLimit);                       // @005bde38  (no callers)
    // _time += Session::getTimeStep(); updateLabel(); always returns true.
    bool update();                                     // @005bde48
    void setHidden(bool hidden);                       // @005bde84  _label->setVisible(!hidden)

protected:
    Session* _session = nullptr;                    // +0x2f8  set by init; update() reads its time step
    cocos2d::Label* _label = nullptr;               // +0x300
    cocos2d::Sprite* _backgroundSprite = nullptr;   // +0x308  GameplayControls::_timerBg (not retained)
    float _time = 0.0f;                             // +0x310  seconds
    float _timeLimit = 0.0f;                        // +0x314  written only (setTimeLimit/reset)
    bool _realignedForMinutes = false;              // +0x318  alignText() done for 1..9 minutes
    bool _realignedForTenMinutes = false;           // +0x319  alignText() done for >= 10 minutes
    std::string _timeString;                        // +0x320  label text
    std::string _minutesString;                     // +0x338  "M:" or ""
    std::string _secondsString;                     // +0x350  "0S" or "SS"
    std::string _hundredthsString;                  // +0x368  ".0h"/".hh", "" from 10 minutes on
};
