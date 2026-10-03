#pragma once

#include "cocos2d.h"
#include "HWWindowDelegate.h"
#include "PageControlDelegate.h"

#include <cstdint>
#include <map>
#include <vector>

class HWWindow;
class LevelSelectBtn;
class MainMenu;
class PageControl;
class PerspectiveCharacters;

// The chapter / level picker that MainMenu slides in over its main buttons. Chapters are laid out
// horizontally (one visible-width page each) inside a ClippingNode; _index is the fractional
// scroll position, driven by swipes, the page dots and the chapter-completion animation (check
// marks pop, a "stamp" slams onto the foremost character portrait, the screen shakes, then the
// next chapter is shown).
//
// Starting a level: levelBtnPressed() calls Settings::setSelectedLevel(chapter, level), which reads
// "levels/" + <level data "dataFile"> via FileUtils::getStringFromFile into Settings; the menu then
// runs Gameplay::createScene("", nullptr) (empty string = use Settings' selected level), or
// CharacterSelectLayer::createScene first when the chapter has characterIndex -1.
//
// arm64 sizeof 0x430 (create() does a plain `new`, 0x430 bytes). HWWindowDelegate subobject at
// +0x2f8, PageControlDelegate subobject at +0x300.
class LevelSelectMenu : public cocos2d::Node, public HWWindowDelegate, public PageControlDelegate
{
public:
    LevelSelectMenu();                                 // @005dfe10
    ~LevelSelectMenu() override;                       // @005dff3c (D1), @005dffe8 (D0)

    // Plain new + init (inlined) + autorelease.
    static LevelSelectMenu* create(PerspectiveCharacters* perspectiveCharacters, MainMenu* mainMenu);  // @005e000c
    // Non-virtual; stores the pointers, clears _menuSliding / _animatingChapterAdvance, returns true.
    bool init(PerspectiveCharacters* perspectiveCharacters, MainMenu* mainMenu);          // @005e0074

    void onEnter() override;                           // @005e0090  vptr+0x320
    void addMenu();                                    // @005e0410
    void checkForChapterCompletion();                  // @005e1538
    void backBtnPressed();                             // @005e16ec  -> MainMenu::levelSelectMenuExit()
    void onEnterTransitionDidFinish() override;        // @005e16f4  vptr+0x328
    void showAlertIfLevelsUnlocked();                  // @005e1720
    void addTouchListeners();                          // @005e1d78

    // Touch handlers bound with CC_CALLBACK_2 (LevelSelectMenu is a Node, not a Layer: these are
    // new non-virtual methods).
    bool onTouchBegan(cocos2d::Touch* touch, cocos2d::Event* event);      // @005e1f5c
    void onTouchMoved(cocos2d::Touch* touch, cocos2d::Event* event);      // @005e2060
    void onTouchEnded(cocos2d::Touch* touch, cocos2d::Event* event);      // @005e2158
    void onTouchCancelled(cocos2d::Touch* touch, cocos2d::Event* event);  // @005e2294  -> onTouchEnded
    // (sic: "remote") removes every listener targeting this node.
    void remoteTouchListeners();                       // @005e2298

    void startLevelsAnimation();                       // @005e22a8
    void advanceChapter();                             // @005e27c0
    void showCredits();                                // @005e296c
    void playClick();                                  // @005e2ab4
    void levelsAnimationComplete();                    // @005e2b6c  -> stampAnimationBegin()
    void stampAnimationBegin();                        // @005e2b70
    void shake();                                      // @005e304c  _shakeDuration = 45
    void stampAnimationComplete();                     // @005e3058
    bool allLevelsCompleted();                         // @005e30b4
    // Always returns null.
    // RE-TODO(@005e32c0): return type is not in the symbol (the function only clears x0).
    LevelSelectBtn* createBtn(int chapter, int level, bool locked);                       // @005e32c0
    void slideInComplete();                            // @005e32c8  -> addTouchListeners()
    void slideOutComplete();                           // @005e32cc  removes this node's listeners
    void setShowOffscreenElementsOnEnter(bool show);   // @005e32dc
    void levelBtnPressed(int chapter, int level, LevelSelectBtn* btn);                    // @005e32e4
    void update(float dt) override;                    // @005e43a8  vptr+0x3d8

    // Overrides of secondary-base virtuals. Declaration order = order of their new primary-vtable
    // slots (vptr+0x528, +0x530, +0x538).
    void hwWindowWasDismissed(HWWindow* window) override;                  // @005e4b28 (thunk @005e4b68)
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;  // @005e4ba8 (thunk @005e4c08)
    void pageControl(PageControl* control, int page) override;            // @005e4c68 (thunk @005e4c74)

protected:
    // Member names: the iOS LevelSelectMenu ivars where the Android field clearly descends from one
    // (python tools/re/ios_ivars.py LevelSelectMenu); the windows, _mainMenu, _clippingNode,
    // _chapterLevelBtns and _levelBtns are Android additions.
    HWWindow* _levelUnavailableWindow;                 // +0x308  "This level doesn't exist. Yet."
    HWWindow* _levelLockedWindow;                      // +0x310  "This level is locked."
    HWWindow* _levelsUnlockedWindow;                   // +0x318  "Hooray!"; not initialised by the constructor
    MainMenu* _mainMenu;                               // +0x320
    cocos2d::ClippingNode* _clippingNode;              // +0x328  stencil = visible rect
    cocos2d::Node* _namesHolder;                       // +0x330  chapter titles; scrolls at 1.5x
    cocos2d::Node* _commentsHolder;                    // +0x338  chapter comments; scrolls at 2x
    // iOS in-app purchase holder: scrolled at 2.5x in update() when non-null, never created.
    cocos2d::Node* _buyHolder;                         // +0x340
    cocos2d::Node* _comingSoonHolder;                  // +0x348  "COMING LATER" labels; follows _buttonsSBN
    cocos2d::Touch* _touch;                            // +0x350  touch currently scrolling
    float _startX;                                     // +0x358  touch start x
    // Zeroed by the constructor, never used on Android. Named after the iOS ivars that follow
    // _startX (iOS also has float _menuZ there; which of the four Android dropped is unknown).
    float _holderStartX;                               // +0x35c
    float _targetX;                                    // +0x360
    unsigned int _menuIndex;                           // +0x364
    bool _menuSliding;                                 // +0x368  |target - _buttonsSBN x| >= 10; write-only
    bool _checkUnlockedLevelsOnEnterTransition;        // +0x369
    float _index;                                      // +0x36c  scroll position, in chapters
    float _prevIndex;                                  // +0x370  _index at touch start
    int _lockedLevelChapter;                           // +0x374  chapter offered by the "Go to levels" button
    // RE-TODO(@005dff3c): only constructed and freed; element type unknown (trivially destructible).
    // Probably the iOS NSMutableArray _unlockedLevels.
    std::vector<cocos2d::Node*> _unlockedLevels;       // +0x378
    unsigned int _max;                                 // +0x390  number of chapters (unsigned: ucvtf/unsigned compares)
    // iOS CCSpriteBatchNode; a plain Node holding every LevelSelectBtn on Android.
    cocos2d::Node* _buttonsSBN;                        // +0x398
    std::map<int, std::vector<cocos2d::Node*>> _chapterLevelBtns;  // +0x3a0  real levels per chapter
    cocos2d::Vec2 _buttonSBNPos;                       // +0x3b8  _buttonsSBN position saved for the shake
    cocos2d::Vec2 _stampsSBNPos;                       // +0x3c0  unused on Android
    cocos2d::Vec2 _pageControlPos;                     // +0x3c8  unused on Android
    cocos2d::Vec2 _charactersLayerPos;                 // +0x3d0  _charactersLayer position saved for the shake
    cocos2d::ValueVector _chapters;                    // +0x3d8  Settings::getAllChaptersData(...)
    PerspectiveCharacters* _charactersLayer;           // +0x3f0
    std::vector<LevelSelectBtn*> _levelBtns;           // +0x3f8  every slot, empty ones included
    LevelSelectBtn* _pressedBtn;                       // +0x410
    PageControl* _pageControl;                         // +0x418
    bool _unlockedFullGame;                            // +0x420  zeroed, never used on Android
    bool _animatingChapterAdvance;                     // +0x421  chapter completion running; blocks touches
    bool _showOffscreenElementsOnEnter;                // +0x422  only set (setShowOffscreenElementsOnEnter)
    unsigned int _shakeDuration;                       // +0x424  frames left of the stamp shake (45)
    // RE-TODO(@005e000c): 8 bytes at +0x428 are never accessed; kept so arm64 sizeof stays 0x430.
    uint8_t _unk428[8];                                // +0x428
};
