#pragma once
// ONLINE (PC addition): small widget kit for the online level browser (OnlineLevelBrowser).
// Not part of the 1:1 reconstruction. Buttons and panels reuse the game's own menu art
// (menus/main/menu_main.plist buttons, menus/alert/menu_alert.plist window frame and buttons,
// stretched as 9-slices); icons, stars and plain rounded shapes are generated white textures
// tinted per use. Fonts are the game's: Clarendon Bold for headings/buttons, Arial for text.

#include <functional>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "ui/UIScale9Sprite.h"

namespace online {
namespace ui {

// Fonts (the game's menu fonts).
extern const char* const kFontHeading;      // fonts/ClarendonLTStd-Bold.ttf
extern const char* const kFontBody;         // fonts/Arial.ttf
extern const char* const kFontBodyBold;     // fonts/Arial Bold.ttf

// Palette (globals::colors plus text tones).
extern const cocos2d::Color3B kPink;        // (253,129,129)
extern const cocos2d::Color3B kBlue;        // (61,139,199)
extern const cocos2d::Color3B kTextDim;     // secondary text on the dark background
extern const cocos2d::Color3B kInk;         // text on light (HWWindow) panels
extern const cocos2d::Color3B kInkDim;      // secondary text on light panels
extern const cocos2d::Color3B kStarGold;

// Loads the menu atlases the widgets use (main menu + alert window).
void loadAtlases();

// ---- art --------------------------------------------------------------------------------------
// A white rounded rectangle as a scale-9 sprite (corner radius in design units), tinted/faded.
cocos2d::ui::Scale9Sprite* roundedRect(const cocos2d::Size& size, float radius, const cocos2d::Color3B& color,
                                       GLubyte opacity);
// An atlas frame stretched as a 9-slice to `size` (centre = normalized stretchable rect).
cocos2d::Sprite* frameSprite(const std::string& frame, const cocos2d::Size& size,
                             const cocos2d::Rect& centre = cocos2d::Rect(0.3f, 0.3f, 0.4f, 0.4f));
// The HWWindow panel (window_frame.png: light grey body under a darker header strip).
cocos2d::Sprite* windowPanel(const cocos2d::Size& size);
float windowPanelStrip();   // height of that header strip (design units)
// White icons (design-unit size): star, search, spinner, globe, clear, chevron.
cocos2d::Sprite* iconSprite(const std::string& name, float size);
// Replaces the icon of a MainMenu::btnWithIcon button with a generated one (e.g. "globe"),
// centred on the button like the atlas icons.
void setMenuButtonIcon(cocos2d::MenuItemSprite* button, const std::string& icon);

// 5 stars, `height` tall, filled to rating (0..5, partial fill of the last star).
class StarBar : public cocos2d::Node {
public:
    static StarBar* create(float height, const cocos2d::Color3B& emptyColor = cocos2d::Color3B::WHITE,
                           GLubyte emptyOpacity = 55);
    void setRating(float rating);
private:
    std::vector<cocos2d::Sprite*> _fills;
};

// ---- text helpers ---------------------------------------------------------------------------------
std::string formatCount(int n);                  // 999, 1.2K, 45K, 1.2M, 1.2B
std::string formatThousands(int n);              // 1,234,567
std::string formatDate(const std::string& ymd);  // "2013-05-21" -> "May 21, 2013"
std::string formatRating(float rating);          // "4.27"
// Sets label text, cutting it with an ellipsis so it fits maxWidth (UTF-8 aware).
void setEllipsized(cocos2d::Label* label, const std::string& text, float maxWidth);
// Browser character names (1..11; 0 = "Any character").
std::string characterName(int character);
// Short tag for the list thumbnail ("ANY", "SANTA"...); "" when the portrait says it all.
std::string characterTag(int character);
bool characterOnMobile(int character);           // 1-5, 9
std::string characterPortrait(int character);    // 25% portrait path (generic for any/missing)

// Shows a non-blocking note on top of whatever scene is running `delay` seconds from now (the
// scene that a level start pushes); it fades out by itself.
void showToast(const std::string& title, const std::vector<std::string>& lines, float delay);

// True when `node` and all its ancestors are visible (and it is running).
bool isShown(cocos2d::Node* node);
// True when an HWWindow (the game's modal popup) or a Dropdown menu is open.
bool modalOpen();

// ---- widgets --------------------------------------------------------------------------------------
// A button: either an atlas frame stretched as a 9-slice (the game's chunky buttons) or a
// generated rounded rectangle; Clarendon label, optional icon, hover/pressed/disabled looks.
class Button : public cocos2d::Node {
public:
    struct Style {
        std::string frame;               // atlas frame ("" = generated rounded rectangle)
        std::string frameDown;           // pressed frame ("" = darken)
        cocos2d::Rect centre = cocos2d::Rect(0.3f, 0.3f, 0.4f, 0.4f);
        cocos2d::Color3B color = cocos2d::Color3B::WHITE;  // tint / fill
        GLubyte opacity = 255;
        float radius = 30.0f;            // generated shape only
        cocos2d::Color3B textColor = cocos2d::Color3B::WHITE;
    };
    static Style chunky(const std::string& colour);   // menu_main_btn_<blue|pink|grey> (white border)
    static Style playButton();                        // menu_main_playbtn_blue
    static Style window(const std::string& colour);   // window_btn_<blue|pink|yellow> (alert buttons)
    static Style ghost();                             // translucent dark, for secondary controls

    static Button* create(const std::string& text, const cocos2d::Size& size, const Style& style,
                          float fontSize = 56.0f, const std::string& font = kFontHeading);
    void setCallback(std::function<void()> callback) { _callback = std::move(callback); }
    void setText(const std::string& text);
    void setButtonSize(const cocos2d::Size& size);
    void setStyle(const Style& style);
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }
    // Places an icon left of the text (or centred when the text is empty); iconRight puts it
    // after the text (dropdown chevrons).
    void setIcon(cocos2d::Sprite* icon, bool iconRight = false);
    cocos2d::Label* label() const { return _label; }
    bool hitTest(const cocos2d::Vec2& worldPoint) const;

protected:
    bool init(const std::string& text, const cocos2d::Size& size, const Style& style, float fontSize,
              const std::string& font);
    void rebuildBackground();
    void refresh();
    void layoutContent();

    Style _style;
    cocos2d::Sprite* _bg = nullptr;
    cocos2d::Label* _label = nullptr;
    cocos2d::Sprite* _icon = nullptr;
    bool _iconRight = false;
    std::function<void()> _callback;
    bool _enabled = true;
    bool _hover = false;
    bool _pressed = false;
};

// A button showing the current choice and a chevron; clicking opens a small menu of options
// (an HWWindow-looking panel) below it.
class Dropdown : public Button {
public:
    static Dropdown* create(const std::vector<std::string>& options, const cocos2d::Size& size, const Style& style,
                            float fontSize = 52.0f);
    void setSelectedIndex(int index);
    int selectedIndex() const { return _selected; }
    std::function<void(int)> onSelect;
    static bool anyOpen();
    static void closeAll();

protected:
    void open();
    std::vector<std::string> _options;
    int _selected = 0;
};

// Single-line text input drawn in the game's style. Keyboard text arrives through the IME
// dispatcher (GLFW char events), Backspace deletes, Ctrl+V pastes; Enter is handled by the
// owner since GLFW does not deliver it as a character. A clear (x) button appears with text.
class SearchField : public cocos2d::Node, public cocos2d::IMEDelegate {
public:
    static SearchField* create(const cocos2d::Size& size, const std::string& placeholder);
    const std::string& text() const { return _text; }
    void setText(const std::string& text);
    void setPlaceholder(const std::string& placeholder);
    void focus();
    void paste();
    std::function<void()> onChange;

    // IMEDelegate
    bool canAttachWithIME() override { return true; }
    bool canDetachWithIME() override { return true; }
    void didAttachWithIME() override;
    void didDetachWithIME() override;
    void insertText(const char* text, size_t len) override;
    void deleteBackward() override;
    const std::string& getContentText() override { return _text; }

protected:
    bool init(const cocos2d::Size& size, const std::string& placeholder);
    void refresh();

    std::string _text;
    std::string _placeholder;
    cocos2d::Label* _label = nullptr;
    cocos2d::Sprite* _caret = nullptr;
    cocos2d::Sprite* _icon = nullptr;
    cocos2d::Sprite* _clear = nullptr;
    float _textLeft = 0.0f;
    bool _focused = false;
};

// Small animated "busy" indicator.
cocos2d::Sprite* createSpinner(float size, const cocos2d::Color3B& color);

}  // namespace ui
}  // namespace online
