#pragma once
// ONLINE (PC addition): widgets for the account and replay screens of the online level browser,
// in the browser's style (online/OnlineUi.h: the game's HWWindow panel art, menu buttons,
// Clarendon / Arial). Not part of the 1:1 reconstruction.

#include <functional>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "online/OnlineUi.h"

namespace online {
namespace tjfui {

// White generated icons (like ui::iconSprite): "heart", "heart_outline", "user", "replay",
// "upload", "trophy", "check", "save", "trash".
cocos2d::Sprite* icon(const std::string& name, float size);

class TextInput;

// A full-screen modal panel: dimmed background, the HWWindow panel centred, a header title and
// HWWindow's close button. Swallows touches; Esc closes; named ui::kModalNodeName so the browser
// below ignores its keys. Added to the running scene below HWWindow alerts (which stay on top).
class Panel : public cocos2d::Node {
public:
    void present();
    void dismiss();
    bool closing() const { return _closing; }

protected:
    bool initPanel(const cocos2d::Size& size, const std::string& title);
    virtual void onClosed() {}
    virtual bool onKey(cocos2d::EventKeyboard::KeyCode key) { return false; }
    virtual void onScroll(const cocos2d::Vec2& world, float amount) {}
    // Content coordinates: (0,0) bottom-left of the panel .. _size; _top = below the title.
    cocos2d::Node* _content = nullptr;
    cocos2d::Size _size;
    float _top = 0.0f;
    cocos2d::Label* _title = nullptr;
    bool _closing = false;
    bool _ctrlDown = false;
    TextInput* _firstField = nullptr;   // focused by present() (takes the keyboard from the screen below)
    cocos2d::LayerColor* _dim = nullptr;
};

// Single-line text input (the browser's SearchField look, no magnifier), optional password mask.
// Tab / Enter are left to the owner.
class TextInput : public cocos2d::Node, public cocos2d::IMEDelegate {
public:
    static TextInput* create(const cocos2d::Size& size, const std::string& placeholder, bool password = false,
                             int maxChars = 64);
    const std::string& text() const { return _text; }
    void setText(const std::string& text);
    void clearSecret();   // wipes the text buffer (password fields)
    void focus();
    void unfocus();
    bool focused() const { return _focused; }
    void paste();
    // Characters allowed (empty = any printable); e.g. digits only.
    void setAllowed(const std::string& allowed) { _allowed = allowed; }
    std::function<void()> onChange;
    std::function<void()> onSubmit;   // Android soft keyboard "Done"

    bool canAttachWithIME() override { return true; }
    bool canDetachWithIME() override { return true; }
    void didAttachWithIME() override;
    void didDetachWithIME() override;
    void insertText(const char* text, size_t len) override;
    void deleteBackward() override;
    const std::string& getContentText() override { return _text; }

protected:
    ~TextInput() override;
    bool init(const cocos2d::Size& size, const std::string& placeholder, bool password, int maxChars);
    void refresh();
    std::string _text;
    std::string _placeholder;
    std::string _allowed;
    bool _password = false;
    int _maxChars = 64;
    cocos2d::Label* _label = nullptr;
    cocos2d::Sprite* _caret = nullptr;
    cocos2d::ui::Scale9Sprite* _rim = nullptr;
    bool _focused = false;
};

// A check box with a label (white box, blue tick).
class CheckBox : public cocos2d::Node {
public:
    static CheckBox* create(const std::string& text, bool checked, const cocos2d::Color3B& textColor);
    bool checked() const { return _checked; }
    void setChecked(bool checked);
    std::function<void(bool)> onToggle;

protected:
    bool init(const std::string& text, bool checked, const cocos2d::Color3B& textColor);
    bool _checked = false;
    cocos2d::Sprite* _tick = nullptr;
};

// Clickable 5-star picker (rating 1..5), hover preview.
class StarPicker : public cocos2d::Node {
public:
    static StarPicker* create(float starSize);
    int rating() const { return _rating; }
    void setRating(int rating);
    std::function<void(int)> onPick;

protected:
    bool init(float starSize);
    void show(int stars);
    std::vector<cocos2d::Sprite*> _stars;
    float _starSize = 0.0f;
    int _rating = 0;
};

// HWWindow alerts (the game's popup) with callbacks: confirm -> done(true) on the blue button.
void alert(const std::string& title, const std::string& message);
void confirm(const std::string& title, const std::string& message, const std::string& yes, const std::string& no,
             std::function<void(bool)> done);

// Wraps text into a label of fixed width.
cocos2d::Label* textBlock(const std::string& text, const std::string& font, float size, const cocos2d::Color3B& color,
                          float width, cocos2d::TextHAlignment align = cocos2d::TextHAlignment::LEFT);
cocos2d::Label* label(const std::string& text, const std::string& font, float size, const cocos2d::Color3B& color,
                      const cocos2d::Vec2& anchor = cocos2d::Vec2(0.0f, 0.5f));

}  // namespace tjfui
}  // namespace online
