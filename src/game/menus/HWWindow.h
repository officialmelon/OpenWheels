#pragma once

#include "cocos2d.h"
#include "HWWindowDelegate.h"

#include <string>
#include <vector>

class Mascot;

// Window layout. The values come from the binary; the enumerator names do not survive in it and
// were chosen from the behaviour. Every caller in the Android 1.1.3 build passes 0
// (Settings::createWindow, HWWindow::createAlertWindow).
// RE-TODO(@005c58d0): enumerator names for values 1 and 2 are invented.
enum HWWindowAppearance
{
    // Frame starts at 1.0 x visibleSize and is resized to _windowSize (2100 x content height) once
    // content exists. Scales in from 1.25 with a fade.
    HWWindowAppearanceAlert = 0,
    // Frame = 0.8 x visibleSize; slides in from below the screen.
    HWWindowAppearanceLarge = 1,
    // Frame = 1.0 x visibleSize; slides in from below. The mascot is placed top-left inside the
    // frame and the close button is offset (-125,-175) from the top-right corner.
    HWWindowAppearanceFullScreen = 2,
};

// Button size for btnWithLabel. Only 2 is tested; 0 and 1 both give a 700x160 frame with a font
// size of 80, and 2 gives 1400x320 with 120. Every caller passes 1.
// RE-TODO(@005c700c): enumerator names are invented.
enum HWWindowButtonType
{
    HWWindowButtonTypeSmall = 0,
    HWWindowButtonTypeMedium = 1,
    HWWindowButtonTypeLarge = 2,
};

// The game's modal popup window: a dimmed full-screen LayerColor plus a 9-slice "window_frame.png"
// panel holding a header label, a body and a row of buttons. Any button press is forwarded to
// onWindowButtonPressed() and to every registered HWWindowDelegate. By default the window then
// dismisses itself (fade-out, hwWindowWasDismissed, removeFromParentAndCleanup).
//
// Normally created via Settings::createWindow(), which adds it to the running scene at
// globals::ui::alertWindowDepth.
//
// Button tags passed to delegates: showAlertMessage uses 1 for the confirm button (blue) and 0 for
// cancel (pink). The close button (createCloseBtn) uses -1. showScrollableText uses 1 for confirm
// when at least two delegates are registered, otherwise -1.
// btnWithLabel's int "color" argument: 0 = "window_btn_pink.png", 1 = "window_btn_blue.png",
// 2 = "window_btn_yellow.png" with a (70,70,70) label; the label is WHITE otherwise.
//
// arm64 sizeof 0x3a0. The HWWindowDelegate subobject is at 0x2f8; HWWindow overrides neither of
// its methods.
class HWWindow : public cocos2d::Node, public HWWindowDelegate
{
public:
    static HWWindow* create(HWWindowAppearance appearance, HWWindowDelegate* delegate, bool addCloseBtn,
                            bool addMascot);                                               // @005c5838
    bool init(HWWindowAppearance appearance, HWWindowDelegate* delegate, bool addCloseBtn,
              bool addMascot);                                                             // @005c58d0

    // Settings::getInstance()->createWindow(Alert, nullptr, addCloseBtn, addMascot), then
    // showAlertMessage(title, message, confirmLabel, cancelLabel, animated).
    static HWWindow* createAlertWindow(std::string title, std::string message, std::string confirmLabel,
                                       std::string cancelLabel, bool animated, bool addCloseBtn,
                                       bool addMascot);                                    // @005c5d70
    // Does nothing when all four strings are empty. Builds the header, an Arial body text, the
    // confirm button (tag 1) and, when cancelLabel is non-empty, the cancel button (tag 0).
    // animated: animateInWindow(), otherwise layoutContent() only.
    void showAlertMessage(std::string title, std::string message, std::string confirmLabel,
                          std::string cancelLabel, bool animated);                         // @005c5f54

    HWWindow();                                                                            // @005c63a8
    ~HWWindow() override;                                                                  // @005c6480 (D1), @005c64c8 (D0)

    void setDismissUponButtonPress(bool dismissUponButtonPress);                           // @005c64ec
    void addDelegate(HWWindowDelegate* delegate);       // @005c64f4  push_back if not present
    cocos2d::MenuItemSprite* createCloseBtn(int tag);   // @005c6698
    Mascot* getMascot();                                // @005c68bc
    void animateInWindow();                             // @005c68c4
    void layoutContent();                               // @005c6a1c
    void resizeWindowFrameToFitContent();               // @005c6bd4  (no callers)
    void dismissWindow(bool animated);                  // @005c6bf8  once only (_dismissed)
    void dismissAnimationComplete();                    // @005c6e14
    // Adds the header label to _contentNode. Returns the y coordinate (in _contentNode space) where
    // the body starts.
    float addHeader(std::string title);                 // @005c6e88
    // RE-TODO(@005c700c): the return type is not encoded in the symbol. Ghidra infers
    // MenuItemSprite* (the item is a MenuItemImage); createCloseBtn has the same issue.
    cocos2d::MenuItemSprite* btnWithLabel(std::string label, int color, int tag,
                                          HWWindowButtonType type);               // @005c700c
    // "Important" / privacy text, with "Privacy Policy" (tag 0, pink) and "ACCEPT" (tag 1, blue).
    void showPrivacyPolicyMessage();                    // @005c74d0
    // Header plus a ui::ScrollView holding an Arial Bold label, then buttons as in
    // showAlertMessage. No callers.
    void showScrollableText(std::string title, std::string text, std::string confirmLabel,
                            std::string cancelLabel);                                      // @005c7988
    void removeDelegate(HWWindowDelegate* delegate);    // @005c7d80  erases the first match
    void removeAllDelegates();                          // @005c7de4
    void addButtons();                                  // @005c7df0  single "CLOSE" button (no callers)
    // Menu callback of every button; ignored once _dismissed.
    void btnPressed(cocos2d::Ref* sender);              // @005c7ee4

    void update(float dt) override;                     // @005c7f90  vptr+0x3d8 (empty)

    // New virtuals (hooks for subclasses; no subclass exists in this build), in vtable order:
    virtual void onWindowButtonPressed(int buttonTag);  // @005c7f94  vptr+0x528 (empty)
    virtual void onWindowDismissed();                   // @005c7f98  vptr+0x530 (empty)

protected:
    cocos2d::MenuItemSprite* _closeBtn;                 // +0x300  from createCloseBtn(-1), inside its own Menu
    int _bgZOrder;                                      // +0x308  10000 (local z of _bgLayer)
    int _windowZOrder;                                  // +0x30c  10001 (local z of _windowNode)
    std::vector<HWWindowDelegate*> _delegates;          // +0x310
    cocos2d::Node* _windowNode;                         // +0x328  frame + content + close menu + mascot; centred
    cocos2d::Sprite* _windowFrame;                      // +0x330  "window_frame.png", 9-slice
    cocos2d::Label* _headerLabel;                       // +0x338
    cocos2d::Node* _contentNode;                        // +0x340  child of _windowNode
    // Alert frame size: width is fixed at 2100 in init(); height is set by the show*() methods.
    // Applied to _windowFrame for HWWindowAppearanceAlert.
    cocos2d::Size _windowSize;                          // +0x348
    cocos2d::LayerColor* _bgLayer;                      // +0x350  black, faded to 210
    cocos2d::Size _windowSizeRatio;                     // +0x358  initial frame size as a fraction of visibleSize
    cocos2d::EventListenerTouchOneByOne* _touchListener;  // +0x360  swallows all touches; retained.
                                                          // Not initialised by the ctor.
    int _headerFontSize;                                // +0x368  120
    int _textFontSize;                                  // +0x36c  80
    float _headerTopMargin;                             // +0x370  100: header top = -(_titleBarHeight + this)
    float _headerBottomMargin;                          // +0x374  100  RE-TODO: never read
    float _titleBarHeight;                              // +0x378  60: close button y = frameH/2 - this/2
    float _frameBorder;                                 // +0x37c  8    RE-TODO: never read, name is a guess
    float _unk380;                                      // +0x380  16   RE-TODO: never read
    float _textSideMargin;                              // +0x384  100: text width = _windowSize.width - 2*this
    float _buttonSpacing;                               // +0x388  50   RE-TODO: never read (code uses a literal 50)
    Mascot* _mascot;                                    // +0x390
    HWWindowAppearance _appearance;                     // +0x398
    bool _dismissed;                                    // +0x39c  false; set by dismissWindow()
    bool _dismissUponButtonPress;                       // +0x39d  true
};
