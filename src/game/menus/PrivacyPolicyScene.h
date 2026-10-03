#pragma once

#include "cocos2d.h"
#include "HWWindowDelegate.h"

#include <string>

class HWWindow;

// First scene run by AppDelegate. A black screen that, until "terms_of_use_accepted" is set in
// UserDefault, shows HWWindow::showPrivacyPolicyMessage() ("Privacy Policy" opens the policy URL,
// "ACCEPT" stores the flag). It then initialises the AdController and fades to the main menu.
//
// arm64 sizeof 0x330, HWWindowDelegate at 0x320, no own fields.
// New primary-vtable entries, in order: hwWindowButtonPressed (+0x648), hwWindowWasDismissed (+0x650).
class PrivacyPolicyScene : public cocos2d::Layer, public HWWindowDelegate
{
public:
    static cocos2d::Scene* createScene();                   // @0064caa4  (create() inlined)

    CREATE_FUNC(PrivacyPolicyScene);                        // (inlined into createScene, no symbol)

    PrivacyPolicyScene();                                   // @0064cb4c (C2; C1 is an alias)
    ~PrivacyPolicyScene() override;                         // @0064ce8c (D2), @0064cea8 (D0)

    bool init() override;                                   // @0064cb84  vptr+0x4f8
    void onEnterTransitionDidFinish() override;             // @0064cbc8  vptr+0x328

    // No callers; neither uses `this`.
    // RE-TODO(@0064cce4): could equally be static.
    void initAds();                                         // @0064cce4
    void goToNextScene();                                   // @0064ccfc
    // Full-screen background sprite stretched to the window size. No callers.
    cocos2d::Sprite* addFillBGWithFile(std::string file);   // @0064cda0

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;   // @0064cecc (thunk @0064cf40)
    void hwWindowWasDismissed(HWWindow* window) override;                   // @0064cfb4 (thunk @0064d058)
};
