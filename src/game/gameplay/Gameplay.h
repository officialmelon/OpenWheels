#pragma once

#include "cocos2d.h"
#include "AdControllerDelegate.h"
#include "DebugLayer.h"          // DebugLayerAction (M10)
#include "GameplayControls.h"    // ControlsLayerAction
#include "HWWindowDelegate.h"

#include <string>

class BackgroundLayer;
class DebugLayer;
class DyingVignette;
class GameplayTimer;
class HWWindow;
class LevelB2D;
class PauseLayer;
class ReplayData;
class Session;
class VictoryMenu;

// Payload (int*) of the custom event "gameplayMenuAction" = tag of the pressed PauseLayer /
// VictoryMenu button; handled by Gameplay::handleMenuAction. Values from the binary, names ours.
// RE-TODO(@005ba340): enumerator names are invented.
enum GameplayMenuAction
{
    GameplayMenuActionResume = 0,           // unpauseGameplay (Tracker action is "options_pressed")
    GameplayMenuActionExit = 1,             // "exit_level_pressed" -> MainMenu::createScene(2, 0)
    GameplayMenuActionReset = 2,            // "reset_level_pressed" -> ResetWorkaroundScene
    GameplayMenuActionRemoveAds = 3,        // handled by PauseLayer; ignored here
    GameplayMenuActionChangeCharacter = 4,  // "change_character_pressed" -> CharacterSelectLayer
    GameplayMenuActionViewReplay = 5,       // "view_replay_pressed" -> _isReplay = true, createScene("", _replayData)
    GameplayMenuActionNextLevel = 6,        // "advance_level_pressed" -> advanceLevelIndex ? createScene("", nullptr) : MainMenu
    GameplayMenuActionOptions = 7,          // handled by PauseLayer (OptionsMenu); ignored here
};

// The in-level scene layer. createScene(xml, replay) wraps it in a Scene.
//
// Start-up: init(xml) keeps the level XML text and adds a blue LayerColor (z -100);
// onEnterTransitionDidFinish -> (DelayTime 1/30) oneFrameAfterOnEnterTransitionDidFinish, which asks
// AdController for an interstitial (this as delegate) and, if none is shown, calls
// beginGameplayFollowingInterstitial (else interstitialDidEnd does, after 0.25 s, possibly via the
// internal "want more Happy Wheels" ad). beginGameplayFollowingInterstitial creates the ReplayData
// (unless replaying), the Session (z 1), the BackgroundLayer (z 0), sets up the level, creates the
// GameplayControls (z 4) and the GameplayTimer (z 6), adds the listeners and schedules update().
//
// Per frame (update): replay ? _controls->setState(_replayData->getEntry())
//                            : _replayData->addEntry(_controls->getState());
// then, while the level runs, every character of the level gets CharacterB2D::setState(state); then
// BackgroundLayer::update(gameplay container position) and Session::update(dt).
//
// Custom events: "gameplayMenuAction", "gameplayControlsAction", "levelCompleted" (scene-graph
// listeners); "characterDying", "characterDead", "system_trigger", "characterEjected".
//
// arm64 sizeof 0x490 (create: new(nothrow) 0x490; = data end 0x488 rounded to Node's 16-byte
// alignment). AdControllerDelegate at +0x320, HWWindowDelegate at +0x328. New primary-vtable entries,
// in order: hwWindowButtonPressed (vptr+0x648), init(std::string) (vptr+0x650, a new virtual),
// interstitialDidEnd (vptr+0x658) - declared below in that order.
// iOS counterpart: GameplayLayer (ivar names reused where they match).
// TU statics (Gameplay.cpp, _INIT_7 - listed under HWWindowDelegate in the index):
// std::string "level" @00ac6170 (Tracker category) and a zero-initialised bool @00ac6188
// (displayInternalAd: show the "please turn on data" window only the first time).
class Gameplay : public cocos2d::Layer, public AdControllerDelegate, public HWWindowDelegate
{
public:
    Gameplay();             // @005b900c
    ~Gameplay() override;   // @005b8e44 (D1), @005b8edc (D0)

    // new(nothrow) Gameplay; init(levelXml) (virtual call); autorelease or delete.
    static Gameplay* create(std::string levelXml);  // @005b8f00
    // Scene::create(); Gameplay::create(levelXml); replayData ? (_replayData = it, _isReplay = true);
    // if replaying and the controls exist: setMode(ControlsModeReplay); scene->addChild(layer).
    static cocos2d::Scene* createScene(std::string levelXml, ReplayData* replayData);  // @005b90e0

    // ---- new primary-vtable entries (order = vtable order) ----
    // HWWindowDelegate: the "please turn on data" window was closed -> internalAdComplete().
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;  // @005bce48  vptr+0x648; thunk @005bcebc
    // New virtual (hides Layer::init()). Empty levelXml -> Settings::getSelectedLevelFilePath().
    virtual bool init(std::string levelXml);                              // @005b9a74  vptr+0x650
    // AdControllerDelegate (rewardedVideoDidUpdateStatus keeps the default).
    void interstitialDidEnd(InterstitialStatus status) override;          // @005bbaf4  vptr+0x658; thunk @005bc4dc

    // ---- cocos2d::Node overrides ----
    void update(float dt) override;                                        // @005b9e20  vptr+0x3d8
    // Node::draw, then Box2D debug draw when the session has a debug draw.
    void draw(cocos2d::Renderer* renderer, const cocos2d::Mat4& transform,
              uint32_t flags) override;                                    // @005bb5cc  vptr+0x348
    void onEnterTransitionDidFinish() override;                            // @005bb908  vptr+0x328
    // _gameplayStarted = false, _exitTransitionStarted = true (one 16-bit store).
    void onExitTransitionDidStart() override;                              // @005bbacc  vptr+0x338

    // ---- non-virtual ----
    void setReplayData(ReplayData* replayData);   // @005b91e4
    void beginGameplayFollowingInterstitial();    // @005b9228
    void addListeners();                          // @005b9518
    void removeBanner(bool force);                // @005b9b68  (no callers)
    void die();                                   // @005b9bac  (no callers)
    void removeListeners();                       // @005b9bec
    // Camera focus y < -20 m -> dispatch "characterDead" once (_characterDroppedOffscreen).
    void checkCharacterPosition();                // @005b9d3c
    LevelB2D* getLevel();                         // @005b9fb4
    void pauseGameplay();                         // @005b9fbc
    cocos2d::Node* getHighlightNode();            // @005ba118  created on demand (z 5)
    void unpauseGameplay();                       // @005ba160
    // Toggles a DebugLayer (z 11). No callers in 1.1.3.
    void debugBtnPressed(cocos2d::Ref* sender);   // @005ba270
    void handleMenuAction(GameplayMenuAction action);          // @005ba340
    void exitToMenu();                            // @005bab78  (no callers; uses no member)
    void exitToSelectCharacter();                 // @005baba8  (no callers; uses no member)
    void handleControlsLayerAction(ControlsLayerAction action);  // @005babdc
    void handleDying();                           // @005badbc  "characterDying": DyingVignette (z 3)
    void handleDead();                            // @005bae78  "characterDead"
    void clearHighlights();                       // @005baec4  HighlightSprite::fadeOut on all
    void handleDebugLayerAction(DebugLayerAction action);      // @005bb028  (empty)
    void handleLevelComplete();                   // @005bb02c  "levelCompleted"
    // "system_trigger" payload int: 0..4 highlight forward/back/leanFwd/leanBack/special, 5 clear.
    void systemTrigger(void* data);               // @005bb634
    void highlightSpriteAtPos(cocos2d::Vec2 position, int tag);  // @005bb6e4
    void characterEjected();                      // @005bb8bc
    void oneFrameAfterOnEnterTransitionDidFinish();  // @005bba50
    void displayInternalAd();                     // @005bbcb0
    void countDownInternalAd();                   // @005bc4e4
    void internalAdError();                       // @005bc650  -> internalAdComplete()
    void internalAdComplete();                    // @005bc654
    void clearAndRemovePleaseTurnOnDataWindow();  // @005bc6f4  (no callers)
    void showContinueButtonInCaseInterstitialDoesNotLoadAndNoFailureMessageIsReported();  // @005bc730  (no callers)
    void internalAdContinueBtnPressed(cocos2d::Ref* sender);  // @005bca34  -> internalAdComplete()
    void addInternalAdBtn();                      // @005bca38

protected:
    // +0x320 AdControllerDelegate vptr, +0x328 HWWindowDelegate vptr
    bool _gameplayStarted;                 // +0x330  set by beginGameplayFollowingInterstitial (guard)
    bool _exitTransitionStarted;           // +0x331  set by onExitTransitionDidStart
    bool _characterDead;                   // +0x332  set by handleDead; blocks new highlights
    cocos2d::Node* _internalAdNode;        // +0x338  internal "want more HW" ad container (z 10000000)
    cocos2d::Label* _internalAdLabel;      // +0x340  "continue in N" countdown
    int _internalAdCountdown;              // +0x348  internalAdDisplayTime, -1 per second
    bool _paused;                          // +0x350  (iOS paused)
    PauseLayer* _pauseLayer;               // +0x358
    GameplayControls* _controls;           // +0x360
    GameplayTimer* _timer;                 // +0x368
    Session* _session;                     // +0x370
    // Level XML text passed to Session::setupLevel (LevelXMLParser parses it directly).
    std::string _levelXml;                 // +0x378  ctor: zeroed, then assign("")
    // Never set in 1.1.3 (zeroed by the ctor). iOS GameplayLayer::_isTesting: skip ads, completion
    // times and the VictoryMenu.
    bool _isTesting;                       // +0x390
    bool _levelComplete;                   // +0x391  handleLevelComplete guard (iOS levelComplete)
    bool _isReplay;                        // +0x392
    BackgroundLayer* _backgroundLayer;     // +0x398
    VictoryMenu* _victoryMenu;             // +0x3a0
    ReplayData* _replayData;               // +0x3a8  owned unless _isReplay (deleted by the dtor)
    cocos2d::EventListenerCustom* _gameplayMenuActionListener;      // +0x3b0  retained
    // +0x3b8 zeroed by the ctor, never used (between two listeners; maybe a removed
    // "debugLayerAction" listener - handleDebugLayerAction exists but is empty).
    void* _unk0x3b8;                       // +0x3b8
    cocos2d::EventListenerCustom* _gameplayControlsActionListener;  // +0x3c0  retained
    cocos2d::EventListenerCustom* _levelCompletedListener;          // +0x3c8  retained
    cocos2d::EventListenerCustom* _characterDyingListener;          // +0x3d0  retained, not zeroed by the ctor
    cocos2d::EventListenerCustom* _characterDeadListener;           // +0x3d8  retained, not zeroed by the ctor
    cocos2d::EventListenerCustom* _characterEjectedListener;        // +0x3e0  retained, not zeroed by the ctor
    void* _unk0x3e8;                       // +0x3e8  zeroed by the ctor, never used
    DebugLayer* _debugLayer;               // +0x3f0  zeroed by the ctor and by a failed Layer::init
    // RE-TODO(@005b900c): +0x3f8..+0x430 are never used. The ctor zeroes all of them except +0x428
    // (consistent with an uninitialised b2Vec2); possibly the iOS mouse-drag ivars (m_mouseJoint1/2,
    // touch1/2, mouseJointBody1/2, mousePos).
    void* _unk0x3f8;                       // +0x3f8
    void* _unk0x400;                       // +0x400
    void* _unk0x408;                       // +0x408
    void* _unk0x410;                       // +0x410
    void* _unk0x418;                       // +0x418
    void* _unk0x420;                       // +0x420
    void* _unk0x428;                       // +0x428  not zeroed by the ctor
    void* _unk0x430;                       // +0x430
    DyingVignette* _dyingVignette;         // +0x438
    LevelB2D* _level;                      // +0x440  Session::getLevel(), set in beginGameplayFollowingInterstitial
    bool _characterDroppedOffscreen;       // +0x448  (iOS name) also blocks update()'s character feed
    cocos2d::EventListenerCustom* _systemTriggerListener;           // +0x450  retained
    cocos2d::Node* _highlightNode;         // +0x458  HighlightSprite container
    void* _unk0x460;                       // +0x460  only cleared by the dtor
    HWWindow* _pleaseTurnOnDataWindow;     // +0x468  Settings::createWindow with this (+0x328) as delegate
    cocos2d::MenuItemImage* _internalAdBtn;  // +0x470  "window_btn_close.png" close button
    cocos2d::Menu* _internalAdMenu;        // +0x478
    // Parent of showContinueButton...'s menu; never created in 1.1.3 (only read/cleared).
    cocos2d::Node* _continueBtnParent;     // +0x480
};
