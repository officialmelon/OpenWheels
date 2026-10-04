#pragma once
// NET (PC addition): presentation helpers for the "Send to Nearby" / "Receive Levels" panels,
// built from the online browser's widget kit (online/OnlineUi.h) and the game's own HWWindow art.
//
// Modal: a full-screen node added to the running scene above everything (z = uikit window + 1,
// like levelui alerts, so a later alert still lands on top): the background dimmed to black 190
// as HWWindow does, the HWWindow panel (window_frame.png: light grey body under the darker strip)
// centred, a white Clarendon title in the HWWindow header style and HWWindow's round close
// button on the top-right corner. It swallows touches, Esc closes it, and it is named
// online::ui::kModalNodeName so the screens below ignore their keyboard while it is open.

#include <functional>
#include <string>

#include "cocos2d.h"

class HWWindow;

namespace net {
namespace ui {

class Modal : public cocos2d::Node {
public:
    // Adds the modal to the running scene and animates it in (HWWindow alert: scale-in + fade).
    void present();
    // Animates out and removes it; onClosed() runs first. Safe to call twice.
    void dismiss();

protected:
    bool initModal(const cocos2d::Size& panelSize, const std::string& title);
    virtual void onClosed() {}
    // Keys while the modal is on top (Esc already closes). Return true when handled.
    virtual bool onKey(cocos2d::EventKeyboard::KeyCode key) { return false; }
    virtual void onScroll(const cocos2d::Vec2& world, float amount) {}
    // A touch that no button took (text fields see it first and do not swallow).
    virtual void onTouch(const cocos2d::Vec2& world) {}
    virtual void onHover(const cocos2d::Vec2& world) {}

    cocos2d::Node* _panel = nullptr;     // panel space: (0,0) bottom-left .. _panelSize
    cocos2d::Size _panelSize;
    float _contentTop = 0.0f;            // y just below the header strip
    cocos2d::Label* _title = nullptr;
    bool _closing = false;

private:
    cocos2d::LayerColor* _dim = nullptr;
};

// A simple line drawing of the device: "phone" (handset) or anything else (monitor on a stand).
cocos2d::Node* deviceIcon(const std::string& platform, float size, const cocos2d::Color4F& color);
// Turns the HWWindow button with `buttonTag` (0 = cancel) grey, for secondary choices
// ("Decline", "Later") next to the blue confirm button.
void greyButton(HWWindow* window, int buttonTag);
// Label helper (anchor left-middle unless given).
cocos2d::Label* label(const std::string& text, const std::string& font, float size, const cocos2d::Color3B& color,
                      const cocos2d::Vec2& anchor = cocos2d::Vec2(0.0f, 0.5f));

}  // namespace ui
}  // namespace net
