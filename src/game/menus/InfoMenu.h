#pragma once

#include "cocos2d.h"
#include "HWWindowDelegate.h"
#include "SecondaryMenu.h"

class HWWindow;

// "Info" screen. Rows (tag): "more happy wheels?" (0), "support" (2), "rate" (4), "credits" (3),
// "privacy policy" (5). The tags are plain ints in the binary (no enum in any signature).
//
// arm64 sizeof 0x370, HWWindowDelegate at 0x350. There is no user-provided ctor (CREATE_FUNC
// zero-fills; init() is inlined into create()).
// New primary-vtable entries, in order: hwWindowButtonPressed (+0x658), hwWindowWasDismissed (+0x660).
class InfoMenu : public SecondaryMenu, public HWWindowDelegate
{
public:
    static cocos2d::Scene* createScene();                                   // @005c8bb0

    // @005c8bec  InfoMenu::create()
    CREATE_FUNC(InfoMenu);

    bool init() override;                                                   // @005c8cb0  vptr+0x4f8
    void addContent() override;                                             // @005c8ce0  vptr+0x648
    void menuItemPressed(cocos2d::Ref* sender);                             // @005c9214

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;   // @005c99c8 (thunk @005c9c20)
    void hwWindowWasDismissed(HWWindow* window) override;                   // @005c9c28 (thunk @005c9d84)

    // ~InfoMenu(): implicit. D0 @005c9d8c; the D1 slot is SecondaryMenu's (see OptionsMenu.h).

protected:
    float _menuItemPadding = 30.0f;     // +0x358  alignItemsVerticallyWithPadding
    cocos2d::Menu* _menu;               // +0x360
    HWWindow* _supportWindow;           // +0x368  "Visit support website?" prompt while open
};
