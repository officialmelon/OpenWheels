#pragma once
// EDITOR (browser features, PC addition): controls of the editor's property inspector, in the
// style of the online level browser (online::ui: the game's alert-window panel, Clarendon / Arial
// type, blue / pink window buttons). Sizes are design units (the game's 2000-unit-high design
// resolution). Every control works with touch and mouse; text fields take keyboard input.

#include <functional>
#include <string>
#include <vector>

#include "cocos2d.h"

namespace flashed {
namespace ui {

// The editor's text styles on the light window panel.
cocos2d::Label* makeLabel(const std::string& text, float size, bool bold, const cocos2d::Color3B& color);
cocos2d::Label* makeHeading(const std::string& text, float size);

// True while a ValueField has keyboard focus (editor shortcuts stay off).
bool textEditing();

// Optional extra hit test of a control (the inspector restricts touches to its visible area).
using HitFilter = std::function<bool(const cocos2d::Vec2& worldPoint)>;

// A one-line text / number field (white rounded box). Click to edit; Enter or clicking elsewhere
// commits, Escape cancels. Shift+Enter inserts a line break when multiline.
class ValueField : public cocos2d::Node, public cocos2d::IMEDelegate
{
public:
    static ValueField* create(const cocos2d::Size& size, bool numeric, bool multiline = false);
    void setText(const std::string& text);
    const std::string& text() const { return _text; }
    void focus();
    void unfocus(bool commit);
    bool focused() const { return _focused; }
    std::function<void(const std::string&)> onCommit;
    static ValueField* current();
    HitFilter hitFilter;

    bool canAttachWithIME() override { return true; }
    bool canDetachWithIME() override { return true; }
    void didAttachWithIME() override;
    void didDetachWithIME() override;
    void insertText(const char* text, size_t len) override;
    void deleteBackward() override;
    void controlKey(cocos2d::EventKeyboard::KeyCode keyCode) override;
    const std::string& getContentText() override { return _text; }

protected:
    bool init(const cocos2d::Size& size, bool numeric, bool multiline);
    void onExit() override;
    void refresh();
    std::string _text;
    std::string _original;
    bool _numeric = false;
    bool _multiline = false;
    bool _focused = false;
    bool _shift = false;
    bool _replaceAll = false;   // just focused: the first key replaces the whole text
    cocos2d::Label* _label = nullptr;
    cocos2d::DrawNode* _selection = nullptr;
    cocos2d::DrawNode* _caret = nullptr;
    cocos2d::Sprite* _bg = nullptr;
};

// A horizontal slider (blue fill, white knob). value in [min, max], snapped to `segments` steps.
class SliderBar : public cocos2d::Node
{
public:
    static SliderBar* create(float width);
    void setRange(float min, float max, int segments);
    void setValue(float value);
    float value() const { return _value; }
    // (value, final, valueAtDragStart): final on release.
    std::function<void(float, bool, float)> onChange;
    HitFilter hitFilter;

protected:
    bool init(float width);
    void redraw();
    float valueAt(float x) const;
    cocos2d::DrawNode* _draw = nullptr;
    float _min = 0, _max = 1, _value = 0, _start = 0;
    int _segments = 0;
    bool _dragging = false;
};

// An on/off pill.
class Toggle : public cocos2d::Node
{
public:
    static Toggle* create();
    void setOn(bool on);
    bool isOn() const { return _on; }
    std::function<void(bool)> onChange;
    HitFilter hitFilter;

protected:
    bool init() override;
    void redraw();
    cocos2d::DrawNode* _draw = nullptr;
    bool _on = false;
};

// A tappable colour swatch with its hex value.
class Swatch : public cocos2d::Node
{
public:
    static Swatch* create(const cocos2d::Size& size);
    void setColor(int rgb);   // -1 = none
    std::function<void()> onTap;
    HitFilter hitFilter;

protected:
    bool init(const cocos2d::Size& size);
    cocos2d::DrawNode* _draw = nullptr;
    cocos2d::Label* _label = nullptr;
    int _rgb = 0;
};

// Popups (modal, on the running scene; a tap outside closes them).
// A scrollable, filterable list; `onPick(index)`.
void openListPicker(const std::string& title, const std::vector<std::string>& items, int selected,
                    std::function<void(int)> onPick, bool searchable);
// Colour palette + hex field; allowNone adds a "none" choice (-1).
void openColorPicker(const std::string& title, int rgb, bool allowNone, std::function<void(int)> onPick);
bool popupOpen();
void closePopups();

}  // namespace ui
}  // namespace flashed
