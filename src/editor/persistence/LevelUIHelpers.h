#pragma once
// Small UIKit/Foundation stand-ins shared by the E5 screens (port helpers, no iOS counterpart).
//
// Alerts: UIAlertView -> HWWindow. The window is added to the running scene ABOVE uikit::window()
// (z uikit::kWindowZOrder + 1; Settings::createWindow's alertWindowDepth 10000 would be hidden
// behind the editor's view controllers). window->getTag() = the UIAlertView tag.
// UIAlertView button indices: cancel button = 0, other buttons = 1, 2. HWWindow tags: confirm 1,
// cancel 0, close -1. buttonIndex() converts back:
//   cancel only            -> one confirm-style button, index 0
//   cancel + other         -> other = HWWindow confirm (1 -> index 1), cancel = HWWindow cancel (0)
//   cancel + two others    -> close button = cancel (index 0), first other = confirm (index 1),
//                             second other = HWWindow cancel slot (index 2)

#include "UIKitCompat.h"

#include <string>

class HWWindow;
class HWWindowDelegate;

namespace levelui {

// -[NSString capitalizedString]: first letter of every whitespace-separated word upper case, the
// rest lower case (ASCII letters; other bytes unchanged).
std::string capitalized(const std::string& text);

// -[NSString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]
std::string trimmed(const std::string& text);

// [[UIAlertView alloc] initWithTitle:message:delegate:cancelButtonTitle:otherButtonTitles:...];
// alert.tag = tag; [alert show]. otherTitle2 non-empty = a third button (see above).
HWWindow* showAlert(int tag, const std::string& title, const std::string& message,
                    const std::string& cancelTitle, const std::string& otherTitle,
                    HWWindowDelegate* delegate, const std::string& otherTitle2 = "");
// UIAlertView buttonIndex for an HWWindow button tag of a window made by showAlert.
long buttonIndex(int buttonTag, HWWindow* window);

// A nib UIButton: ui::Button skinned later by EditorViewController::skinButton; title font
// bold `pointSize` (iOS points), callback on touch-up-inside (UIControlEventTouchUpInside = 0x40).
cocos2d::ui::Button* makeButton(const std::string& title, float pointSize,
                                const std::function<void(cocos2d::Ref*)>& action);
// [button setTitle:title forState:UIControlStateNormal]
void setButtonTitle(cocos2d::ui::Button* button, const std::string& title);
// UIButton.enabled (also dims the button like UIKit's disabled state).
void setButtonEnabled(cocos2d::ui::Button* button, bool enabled);

// A nib UILabel: ui::Text with a fixed frame size (set by addSubview), alignment
// 0 left / 1 centre / 2 right (NSTextAlignment), single line, text colour.
cocos2d::ui::Text* makeLabel(const std::string& text, float pointSize, bool bold, int alignment,
                             const cocos2d::Color4B& color);

}  // namespace levelui
