#pragma once

class HWWindow;

// Listener interface for HWWindow (the game's modal popup window).
//
// arm64: a bare vptr (sizeof 8), no virtual destructor. The vtable has exactly these two slots, in
// this order (verified in the secondary vtables of HWWindow, Gameplay, LevelSelectMenu, Mascot,
// DebugScene, PrivacyPolicyScene, OptionsMenu, InfoMenu, AdvancedOptionsMenu):
//   [0] hwWindowButtonPressed(int, HWWindow*)
//   [1] hwWindowWasDismissed(HWWindow*)
// Both have empty default bodies. Gameplay does not override hwWindowWasDismissed, and HWWindow
// (which itself derives from HWWindowDelegate) overrides neither. The out-of-line copies are COMDAT
// instances of these inline definitions, emitted in the Gameplay and HWWindow translation units.
class HWWindowDelegate
{
public:
    // buttonTag = getTag() of the pressed button: btnWithLabel tags 1 (confirm) / 0 (cancel), the
    // close button uses -1 (see HWWindow.h). Called before the window dismisses itself.
    // @005c7f9c
    virtual void hwWindowButtonPressed(int buttonTag, HWWindow* window) {}

    // Called once the window has been dismissed (after the fade-out when animated), right before it
    // removes itself from its parent.
    // @005bcf38
    virtual void hwWindowWasDismissed(HWWindow* window) {}
};
