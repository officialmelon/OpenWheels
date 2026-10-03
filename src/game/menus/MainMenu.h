#pragma once

#include "cocos2d.h"
#include "ui/UISlider.h"

#include <cstdint>
#include <string>
#include <vector>

class LevelSelectMenu;
class PerspectiveCharacters;

// What MainMenu::createScene shows. The values come from the binary; the enumerator names do not
// survive in it. Callers in this build pass MenuModeMain (CreditsLayer, PrivacyPolicyScene,
// SecondaryMenu) or MenuModeLevelSelect (Gameplay, CharacterSelectLayer).
enum MenuMode
{
    MenuModeMain = 0,         // main menu; buttons and logo animate in
    // RE-TODO(@005e7794): no caller passes 1; it builds exactly the same scene as MenuModeMain.
    MenuModeUnknown1 = 1,
    MenuModeLevelSelect = 2,  // main menu with the level select already slid in (no animation)
};

// Background colour of a MainMenu::btnWithIcon button ("menu_main_btn_<colour>_normal.png" /
// "_down.png"). Values from the binary; names from the sprite frames. Ignored for the play button.
enum Color
{
    ColorGrey = 0,
    ColorPink = 1,
    ColorBlue = 2,
};

// Title screen: background, perspective character row, logo and the play / options / info buttons
// (right-aligned, bottom of the screen). The play button slides LevelSelectMenu in from the right.
//
// arm64 sizeof 0x390 (create() / createScene() allocate 0x390 with new (std::nothrow)).
class MainMenu : public cocos2d::Layer
{
public:
    MainMenu();                                        // @005e767c
    ~MainMenu() override;                              // @005e76d0 (D1), @005e7770 (D0)

    // Second argument is unused.
    static cocos2d::Scene* createScene(MenuMode mode, cocos2d::Node* unused);  // @005e7794
    // new (std::nothrow) + virtual init(bool) + autorelease / virtual delete on failure.
    static MainMenu* create(bool showLevelSelectMenu);  // @005e78f0

    // show: slide the level select in (creating it on first use) or out; animated: 0.5s
    // EaseExponentialInOut moves, otherwise jump and call the completion directly.
    void showLevelSelectMenu(bool show, bool animated);  // @005e7984

    // New virtual (vptr+0x648, the only one MainMenu adds).
    virtual bool init(bool showLevelSelectMenu);       // @005e7c90

    void addPerspectiveCharacters();                   // @005e7e60
    // animated: logo bounces in from scale 0.01 and the buttons slide in from the right.
    void addMenu(bool animated);                       // @005e83a4
    void update(float dt) override;                    // @005e89e8  vptr+0x3d8, empty
    void levelSelectMenuExit();                        // @005e89ec  showLevelSelectMenu(false, true)
    void showLevelSelectMenuInComplete();              // @005e89f8
    void showLevelSelectMenuOutComplete();             // @005e8a00
    // Enables/shows the button menu and sets the logo opacity to 255 (shown) or 63 (hidden).
    void showMenu(bool show);                          // @005e8a08
    // One-time alerts ("New levels!" / the beta "Oh hi, Mark." message) after the logo animation.
    void logoScaleInComplete();                        // @005e8a64
    // RE-TODO(@005e8f74): return type is not in the symbol; the item is a MenuItemImage and
    // Ghidra types it MenuItemSprite*.
    cocos2d::MenuItemSprite* btnWithIcon(std::string iconFrameName, Color color, bool isPlayBtn,
                                         int tag);    // @005e8f74
    // Texture preloading test (7 "test/preloading_textures/..." images). No caller in this build.
    void preloadTextures();                            // @005e9264
    void preloadNextTexture();                         // @005e9908
    void preloadTexturesComplete();                    // @005e9b8c
    // Menu callback of every button; dispatches on the sender's tag (0 play, 1 options, 2 info,
    // 3 editor).
    void btnPressed(cocos2d::Ref* sender);             // @005e9be0
    void playBtnPressed();                             // @005e9c64
    void infoBtnPressed();                             // @005e9dc4
    void optionsBtnPressed();                          // @005e9f64
    void editorBtnPressed();                           // @005ea100  tracking only
    // Leftover of an old cocos2d-x draw signature: not virtual (absent from the vtable), takes the
    // Renderer by value, and never called. Hides Node::draw for MainMenu-typed calls only.
    void draw(cocos2d::Renderer renderer, const cocos2d::Mat4& transform, bool transformUpdated);  // @005ea240
    // Same as MenuHelper::addBg.
    cocos2d::Sprite* addBgToNode(cocos2d::Node* node, int zOrder);  // @005ea278
    void addDebugUI();                                 // @005ea3c0  empty
    // Debug slider callback: tags 0..3 drive PerspectiveCharacters setZC / setOffsetX / setEyeX /
    // setSpaceZ with percent / 100.
    void sliderEvent(cocos2d::Ref* sender, cocos2d::ui::Slider::EventType type);  // @005ea3c4

protected:
    // Names: iOS MainMenuLayer ivars where the field clearly descends from one (_logo,
    // _perspectiveCharacters, _chapterMenu); the rest are Android additions or unknown.
    cocos2d::Sprite* _logo;                            // +0x320  "menu_main_logo.png"
    // Target of the ProgressTo actions in preloadNextTexture(); never created in this build.
    cocos2d::ProgressTimer* _preloadProgressBar;       // +0x328
    std::vector<std::string> _texturesToPreload;       // +0x330
    unsigned int _preloadIndex;                        // +0x348
    PerspectiveCharacters* _perspectiveCharacters;     // +0x350
    cocos2d::Menu* _menu;                              // +0x358  play / options / info
    cocos2d::Node* _menuNode;                          // +0x360  logo + _menu; slides out for the level select
    // RE-TODO(@005e767c): two bools zeroed by the constructor, never used (iOS has
    // `BOOL animateIn, _showLevelSelect` side by side).
    bool _unk368;                                      // +0x368
    bool _unk369;                                      // +0x369
    LevelSelectMenu* _chapterMenu;                     // +0x370  created on first showLevelSelectMenu(true, ...)
    // RE-TODO(@005e767c): 16 bytes at +0x378 are never accessed and not initialised.
    uint8_t _unk378[16];                               // +0x378
    // Gates the beta "Oh hi, Mark." alert in logoScaleInComplete(). Cleared by init(), never set.
    // (iOS ends with a BOOL _showMenu at the same relative place; the meaning differs.)
    bool _showBetaMessage;                             // +0x388
};
