#pragma once

#include "cocos2d.h"

#include <string>

// Shared base of the full-screen secondary menus: OptionsMenu, InfoMenu, AdvancedOptionsMenu.
// init() builds the common chrome (MenuHelper background z0, overlay z1, back button z2), then the
// header label (z3) with its underline (z4), then calls the virtual addContent().
//
// arm64 sizeof 0x350. The first member sits in cocos2d::Layer's tail padding (Layer dsize 0x31d).
// The subclasses add HWWindowDelegate at 0x350 (OptionsMenu also adds IAPControllerDelegate at
// 0x358). They have no user-provided constructors and are created with
// `new (std::nothrow) X()`, which zero-fills the object first.
class SecondaryMenu : public cocos2d::Layer
{
public:
    // Plain menu scene: Scene::create() + SecondaryMenu::create(), which is inlined. The bool is
    // never read.
    static cocos2d::Scene* createScene(bool popSceneOnExit);  // @0060eabc

    // Inlined into createScene and pushScene, no symbol. The subclasses declare their own.
    CREATE_FUNC(SecondaryMenu);

    SecondaryMenu();            // @0060eb64  logs "SecondaryMenu: constructor"
    ~SecondaryMenu() override;  // @0060ebec (D1), @0060ec3c (D0)  logs "SecondaryMenu: destructor"

    bool init() override;       // @0060ec60  vptr+0x4f8

    // Header label from _title (ClarendonLTStd-Bold, _headerFontSize, colors::blue), only when
    // _title is non-empty.
    void addHeader();                                   // @0060eda0
    // true: the back button pops this scene; false: it pushes a new MainMenu scene.
    void setPopSceneOnExit(bool popSceneOnExit);        // @0060f030
    // Pushes TransitionFade(menuFadeTime, <new SecondaryMenu scene>, black). The argument is never
    // read (the binary builds a fresh SecondaryMenu scene instead). No callers.
    // RE-TODO(@0060f038): uses neither x0 nor x1, so it could equally be a static method.
    void pushScene(cocos2d::Scene* scene);              // @0060f038

    // New virtuals, in vtable order:
    virtual void addContent();      // @0060f240  vptr+0x648  (empty)
    virtual void backBtnPressed();  // @0060f168  vptr+0x650

protected:
    bool _popSceneOnExit;           // +0x31d  not initialised by the ctor (subclasses come zero-filled)
    cocos2d::Label* _headerLabel;   // +0x320  not initialised by the ctor
    std::string _title;             // +0x328  set by the subclass before SecondaryMenu::init()
    float _contentTop;              // +0x340  = header bottom - _headerBottomMargin; written, never read (ctor: 0)
    int _headerFontSize;            // +0x344  120
    float _headerTopMargin;         // +0x348  150: header top = visibleSize.height - this
    float _headerBottomMargin;      // +0x34c  150
};
