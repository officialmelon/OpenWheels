#pragma once

#include "cocos2d.h"

#include <vector>

class GameplayBtn;

// Which button set GameplayControls::addControls() builds. The values are from the binary (and, for
// 1..5, from the "controls" key of shared/Characters.plist, returned by
// Settings::getSelectedCharacterControlType()); the enumerator names are not in the binary.
// Shared with Settings (M10). RE-TODO(@005b5178): enumerator names are invented.
enum ControlsType
{
    ControlsTypeNone = 0,              // createWithControlsType skips addControls (pause button only)
    // Vehicle sets: move x2, lean x2, special, eject. The special button art is table @0041ba3c.
    ControlsTypeBusinessGuy = 1,       // BusinessGuy, EffectiveShopper: "controls_gameplay_btn_jump.png", special on the left
    ControlsTypeIrresponsibleDad = 2,  // "controls_gameplay_btn_break.png"
    ControlsTypeWheelchairGuy = 3,     // "controls_gameplay_btn_jet.png"
    ControlsTypeMoped = 4,             // MopedCouple: "controls_gameplay_btn_jet.png" + boost meter
    ControlsTypePogoStick = 5,         // PogoStickMan: "controls_gameplay_btn_jump.png", special on the left
    // States (event driven; jump table @0041b43e covers 1000..1004).
    ControlsTypeEjected = 1000,        // "characterEjected": superman/tuck/arch/pushup d-pad + grab
    ControlsTypeDead = 1001,           // "characterDead" (not replay): remove buttons, disable pause btn, lower it by the banner height
    ControlsTypeDeadReplay = 1002,     // "characterDead" in replay mode: remove buttons, enable pause btn
    // 1003 has no case: it falls into the default (vehicle) branch.
    ControlsTypeVictory = 1004,        // victory(): remove buttons, remove pause btn unless mode 1, hide timer bg
};

// GameplayControls::_mode. 2 is set by Gameplay::createScene/setReplayData. 1 is never set on
// Android (iOS HWGameplayControls: the level-editor test mode, which keeps the pause button on
// victory). RE-TODO(@005b8650): enumerator names are invented.
enum ControlsMode
{
    ControlsModeDefault = 0,
    ControlsModeTesting = 1,
    ControlsModeReplay = 2,
};

// Payload (int, the button's tag) of the custom event "gameplayControlsAction" that touchEnded
// dispatches for the pause/reset buttons; handled by Gameplay::handleControlsLayerAction.
// RE-TODO(@005babdc): enumerator names are invented.
enum ControlsLayerAction
{
    ControlsLayerActionPause = 0,   // _pauseBtn tag
    ControlsLayerActionReset = 1,   // _resetBtn tag
};

// Bits of the per-frame control byte = GameplayBtn::getStateValue() of the buttons in _buttons.
// The binary only has the numbers (GameplayBtn::createWithSpriteFrameName arguments in addControls);
// these names are ours. A button's bit is set while a touch holds it; getState() sums them.
// Consumer: CharacterB2D::setState(unsigned char) (vptr+0x118), see docs/modules/M7.md.
enum GameplayControlsStateBit
{
    // Vehicle controls (ControlsType 1..5)
    GameplayControlsStateForward = 0x01,       // "controls_gameplay_btn_move.png"  -> Vehicle::forwardButtonPressed
    GameplayControlsStateBackward = 0x02,      // same sprite, flippedX             -> Vehicle::backButtonPressed
    GameplayControlsStateLeanForward = 0x04,   // "controls_gameplay_btn_lean.png"  -> Vehicle::leanForwardButtonPressed
    GameplayControlsStateLeanBackward = 0x08,  // same sprite, flippedX             -> Vehicle::leanBackButtonPressed
    GameplayControlsStateSpecial = 0x10,       // jump/break/jet                    -> Vehicle::special1ButtonPressed
    GameplayControlsStateEject = 0x80,         // "controls_gameplay_btn_eject.png" -> Vehicle::ejectAllCharacters
    // Ejected controls (ControlsTypeEjected): same bits, different buttons
    GameplayControlsStateSuperman = 0x01,      // "controls_gameplay_btn_superman.png" -> CharacterB2D pose 1
    GameplayControlsStateTuck = 0x02,          // "controls_gameplay_btn_tuck.png"     -> pose 2
    GameplayControlsStateArch = 0x04,          // "controls_gameplay_btn_arch.png"     -> pose 3
    GameplayControlsStatePushup = 0x08,        // "controls_gameplay_btn_pushup.png"   -> pose 4
    GameplayControlsStateGrab = 0x10,          // "controls_gameplay_btn_grab.png"     -> CharacterB2D::startGrab
};

// On-screen touch controls of the level (z 4 in Gameplay). Owns the state buttons (_buttons), the
// pause/reset buttons, the timer background, the moped boost meter and the replay indicator.
// Listens (EventDispatcher) to: touches (one-by-one, swallowing, fixed priority 1),
// "characterEjected" -> addControls(ControlsTypeEjected), "characterDead" -> handleDeath(),
// "banner_shown"/"banner_removed" -> only query the banner size (layout adjustment was removed).
//
// Public API used by the rest of the game: createWithControlsType, getState/setState (Gameplay::update,
// replay), setMode, setHidden (pause), victory, getTimerBg (GameplayTimer), get*Pos (tutorial
// highlights, Gameplay::systemTrigger), setMeterPercentage (moped boost).
//
// arm64 sizeof 0x450 (createWithControlsType: operator new(0x450), no nothrow). Overrides only init()
// and the destructor. iOS counterpart: HWGameplayControls : FFGameplayControls (ivar names reused
// below where they match).
class GameplayControls : public cocos2d::Layer
{
public:
    GameplayControls();             // @005b4858
    ~GameplayControls() override;   // @005b48dc (D1), @005b49d8 (D0)  removes + releases the listeners

    // new GameplayControls; init(); autorelease; if (type != 0) addControls(type); addPauseBtn().
    static GameplayControls* createWithControlsType(ControlsType type);  // @005b50e8

    // UserDefault "adjust_controls_for_notch" (* 180 = _notchOffset) and "controls_user_scale"
    // (_userScale = n * 0.25 + 1), layout metrics, addSprites(), addEvents(), _enabled = true.
    bool init() override;           // @005b49fc  vptr+0x4f8

    void addSprites();              // @005b4b54  loads "controls/gameplay/controls_gameplay.plist" if not loaded
    void addEvents();               // @005b4c28
    // Removes all state buttons, then builds the set for `type` (no-op after victory, or if
    // type == _controlsType, which is never assigned). Case 1004 is only visible in the asm.
    void addControls(ControlsType type);       // @005b5178
    void addPauseBtn();                        // @005b738c  pause button (tag 0) + timer background
    void updateUpperUIToAccommodateBanner();   // @005b75ec  getBannerAdSize() only
    void addResetBtn();                        // @005b763c  reset button (tag 1)
    void handleDeath();                        // @005b77ec
    void removeMeterBar();                     // @005b79cc
    void addPostDeathControls();               // @005b7a28  reset button + bounce-in of reset/pause
    cocos2d::Sprite* getTimerBg();             // @005b7b24
    // Sum of getStateValue() of every button in _buttons that holds a touch. RE-TODO(@005b7b2c):
    // returns int per the decompilation; iOS FFGameplayControls::state returns unsigned char and all
    // consumers take unsigned char, so the width is not observable.
    int getState();                            // @005b7b2c
    // Replay playback: showPressedState(stateValue & state) on every button (display only).
    void setState(unsigned char state);        // @005b7ba0
    void victory();                            // @005b7c00  addControls(ControlsTypeVictory); _levelComplete = true
    bool touchBegan(cocos2d::Touch* touch);    // @005b7c2c
    void addReplayIndicator();                 // @005b7d88  "REPLAY MODE" overlay (+ ring on the pause button)
    void touchMoved(cocos2d::Touch* touch);    // @005b80c0
    void touchEnded(cocos2d::Touch* touch);    // @005b82a4  pause/reset -> "gameplayControlsAction"
    void removeReplayIndicator();              // @005b84e8
    void touchCancelled(cocos2d::Touch* touch);  // @005b8518  -> touchEnded (the listener calls touchEnded directly)
    void setHidden(bool hidden);               // @005b851c  pause: drop touches, remove the touch listener, setVisible
    void setEnabled(bool enabled);             // @005b85ec  add/remove the touch listener
    ControlsMode getMode();                    // @005b8648
    void setMode(ControlsMode mode);           // @005b8650
    void setMeterPercentage(float percentage); // @005b8658  crops _meterBar's texture rect
    void addAnimationToRing(cocos2d::Sprite* ring);  // @005b8714
    void bannerAdShown(int height);            // @005b87cc  getBannerAdSize() only; no callers
    void bannerRemoved();                      // @005b881c  getBannerAdSize() only; no callers
    // ONLINE (PC addition): the ejected layout's buttons for a browser user vehicle's shift / ctrl
    // actions and its eject (shown only while riding one; GameplayControls.cpp).
    void onlineAddUserVehicleButtons(const cocos2d::Vec2& grabPos, const cocos2d::Size& grabSize);

    // Centres of the state buttons, recorded by addControls (vehicle sets) for the tutorial arrows.
    cocos2d::Vec2 getForwardPos();             // @005b886c
    cocos2d::Vec2 getBackwardPos();            // @005b8878
    cocos2d::Vec2 getLeanForwardPos();         // @005b8884
    cocos2d::Vec2 getLeanBackwardPos();        // @005b8890
    cocos2d::Vec2 getSpecialPos();             // @005b889c
    cocos2d::Vec2 getEjectPos();               // @005b88a8

protected:
    float _userScale;                  // +0x320  1 + "controls_user_scale" * 0.25 (ctor: 1.0f, _notchOffset 0.0f)
    float _notchOffset;                // +0x324  "adjust_controls_for_notch" * 180
    bool _enabled;                     // +0x328  touch listener registered (FFGameplayControls::_enabled)
    bool _hide;                        // +0x329  setHidden
    bool _unk0x32a;                    // +0x32a  zeroed by the ctor, never used (iOS had _ejected / _specialOnLeft)
    bool _levelComplete;               // +0x32b  set by victory(); blocks addControls/addPostDeathControls
    ControlsMode _mode;                // +0x32c  zeroed by the ctor (8-byte store with the bools)
    // Compared in addControls but never assigned anywhere (bug kept): only type 0 is ever skipped.
    ControlsType _controlsType;        // +0x330  0 (ctor, init)
    GameplayBtn* _specialBtn;          // +0x338  state 0x10
    GameplayBtn* _ejectBtn;            // +0x340  state 0x80
    cocos2d::Sprite* _meterBar;        // +0x348  "controls_meter_bar.png" (moped only)
    cocos2d::Sprite* _meterBG;         // +0x350  "controls_meter_back.png" (moped only)
    cocos2d::Sprite* _timerBg;         // +0x358  "controls_timer_bg.png" (addPauseBtn, once)
    cocos2d::Vec2 _pauseBtnPos;        // +0x360  set by addControls (default branch)
    cocos2d::Vec2 _resetBtnPos;        // +0x368  set by addResetBtn
    // RE-TODO(@005b4858): +0x370/+0x378 are zeroed by the ctor and never used; named after the iOS
    // HWGameplayControls ivars that sit between pause/reset/timerBg positions there.
    cocos2d::Vec2 _cameraBtnPos;       // +0x370
    cocos2d::Vec2 _meterPos;           // +0x378
    float _originalMeterY;             // +0x380  _meterBar texture rect y (x if rotated)
    float _originalMeterHeight;        // +0x384  _meterBar texture rect height
    cocos2d::Vec2 _forwardPos;         // +0x388
    cocos2d::Vec2 _backwardPos;        // +0x390
    cocos2d::Vec2 _leanForwardPos;     // +0x398
    cocos2d::Vec2 _leanBackwardPos;    // +0x3a0
    cocos2d::Vec2 _specialPos;         // +0x3a8
    cocos2d::Vec2 _ejectPos;           // +0x3b0
    cocos2d::Vec2 _timerBgPos;         // +0x3b8
    std::vector<GameplayBtn*> _buttons;  // +0x3c0  state buttons (not retained; children of this layer)
    GameplayBtn* _pauseBtn;            // +0x3d8  tag 0, not in _buttons
    GameplayBtn* _resetBtn;            // +0x3e0  tag 1, not in _buttons
    cocos2d::EventListenerTouchOneByOne* _touchListener;  // +0x3e8  retained
    cocos2d::EventListenerCustom* _characterEjectedListener;  // +0x3f0  retained
    cocos2d::EventListenerCustom* _characterDeadListener;     // +0x3f8  retained
    cocos2d::EventListenerCustom* _bannerShownListener;       // +0x400  retained
    cocos2d::EventListenerCustom* _bannerRemovedListener;     // +0x408  retained
    // Layout metrics (init), all multiplied by _userScale; names are ours.
    float _dpadArmLength;              // +0x410  225: superman/tuck/arch/pushup offset from the d-pad centre
    float _dpadCenterX;                // +0x414  (_notchOffset + 450): ejected d-pad centre x from the left edge
    float _dpadCenterY;                // +0x418  450: ejected d-pad centre y from the bottom edge
    float _bottomMargin;               // +0x41c  100
    float _sideMargin;                 // +0x420  (_notchOffset + 60)
    float _topMargin;                  // +0x424  60
    float _buttonSpacing;              // +0x428  90: gap between move buttons / lean buttons
    float _specialButtonSpacing;       // +0x42c  125: gap below the special button
    bool _drawBounds;                  // +0x430  false; true would draw every hit area (red DrawNode, z 9999)
    cocos2d::LayerColor* _replayIndicator;  // +0x438  "REPLAY MODE" overlay while a touch is down (mode 2)
    cocos2d::Touch* _replayIndicatorTouch;  // +0x440
    bool _unk0x448;                    // +0x448  zeroed by the ctor, never used
};
