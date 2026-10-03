#pragma once
// UIKit stand-ins for the iOS level editor's screens (EDITOR_PORT.md rule 3: UIKit -> cocos2d-x UI).
// Not an iOS class: the minimum of UIView / UIScrollView / NSNotificationCenter / root-view
// semantics the ported screens rely on, so their code can keep the iOS geometry and flow.
//
// Geometry (all public frame values are iOS POINTS, like the original code):
//   * a frame is relative to the superview, origin TOP-LEFT, y pointing DOWN (UIKit);
//   * design units = points * EditorAssets::pointsToDesign() (iPad points, scaled by height);
//   * windowSize() is [[CCDirector sharedDirector] winSize] in points: the visible design size
//     divided by pointsToDesign() (768 points tall; ~1382 wide at 3600x2000).
// A View converts frames to cocos2d-x (bottom-left, y up) positions when a subview is added and
// again in layoutSubviews(). Plain nodes added with a frame are centre-anchored at the frame's
// centre; ui::Widget nodes additionally get the frame size as content size.
//
// UIKit -> cocos2d-x mapping used by the editor UI (E4):
//   UIView -> uikit::View (ui::Layout)          UIScrollView -> uikit::ScrollView
//   UILabel -> ui::Text                          UIButton -> ui::Button (scale-9 iOS bundle PNG)
//   UISlider -> uikit::Slider                    UISwitch -> uikit::Switch
//   UITextField -> ui::EditBox (+EditBoxDelegate) UITableView/UIPickerView -> ui::ListView
//   UIViewController / popover -> EditorViewController (a full-window View; see its header)
//   UIAlertView -> HWWindow (the game's own window)
//   NSNotificationCenter -> uikit::NotificationCenter (Director EventDispatcher, EventCustom)

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/CCRefPtr.h"
#include "cocos2d.h"
#include "ui/CocosGUI.h"

namespace uikit {

// ---- geometry -------------------------------------------------------------------------------
float pointsToDesign();            // EditorAssets::pointsToDesign()
cocos2d::Size windowSize();        // [[CCDirector sharedDirector] winSize], in points
float notchOffset();               // [Screen notchOffset] (iOS safe-area inset); 0 on PC

// ---- UIColor --------------------------------------------------------------------------------
// colorWithRed:green:blue:alpha: (components 0..1; rounded to 8 bit).
cocos2d::Color4B color(double r, double g, double b, double a);
cocos2d::Color4B whiteColor();     // 1,1,1,1
cocos2d::Color4B blackColor();     // 0,0,0,1
cocos2d::Color4B grayColor();      // 0.5,0.5,0.5,1
cocos2d::Color4B clearColor();     // 0,0,0,0

// ---- UIFont ---------------------------------------------------------------------------------
// The iOS editor uses "Helvetica-Bold", boldSystemFontOfSize: and the system font. The port maps
// bold faces to a bold TTF (Arial Bold when present) and the rest to a regular one; falls back
// to the "Arial" system font. Sizes are in points (converted with pointsToDesign()).
std::string fontFile(bool bold);
// UILabel stand-in: a ui::Text with the given font, size in points, text area = frame (set by
// View::addSubview), vertically centred; alignment 0 left, 1 centre, 2 right (NSTextAlignment).
cocos2d::ui::Text* makeLabel(const std::string& text, bool bold, float pointSize, int alignment = 0);
// UILabel shadowColor / shadowOffset (points, y down).
void setLabelShadow(cocos2d::ui::Text* label, const cocos2d::Color4B& color, const cocos2d::Size& offset);
// adjustsFontSizeToFitWidth: shrinks the font until the text fits the label's width.
void fitLabelWidth(cocos2d::ui::Text* label, float pointSize);

// ---- UIImage ----------------------------------------------------------------------------------
// [UIImage imageNamed:] / imageWithContentsOfFile: from the player's iOS bundle. Registers the
// image as a SpriteFrame (key "uikit/<name>") and returns the key, or "" when missing. `scale`
// receives frame units per iOS point: the image scale (2 for an "@2x" file, else 1) over the
// content scale factor, as frame sizes are pixels / content scale factor. Apple "CgBI" PNGs
// (most of the bundle's loose PNGs) are decoded too (platform/common/AppleImage).
std::string imageNamed(const std::string& name, float* scale = nullptr);
// A generated image (control art for UISlider / UISwitch, which are system-drawn on iOS):
// `pixel(x, y)` gives straight-alpha RGBA for each pixel (y down). Cached by key.
std::string generatedImage(const std::string& key, int width, int height,
                           const std::function<cocos2d::Color4B(int x, int y)>& pixel);

// ---- UIButton skins ----------------------------------------------------------------------------
// setBackgroundImage:"<prefix>Button.png" (normal) / "<prefix>ButtonHighlight.png" (highlighted),
// resizableImageWithCapInsets:(4, 4, 4, 4) - prefixes yellow, blue, pink, grey, darkGrey.
// The button is scale-9 (caps in image pixels); its size comes from the frame it is placed with.
void skinButton(cocos2d::ui::Button* button, const std::string& prefix);
// A UIButton title: text, system font of `pointSize` points, colour (normal state).
void setButtonTitle(cocos2d::ui::Button* button, const std::string& title, float pointSize,
                    const cocos2d::Color4B& color);

// ---- NSString ---------------------------------------------------------------------------------
std::string capitalizedString(const std::string& s);   // -capitalizedString (ASCII letters)
std::string lowercaseString(const std::string& s);     // -lowercaseString (ASCII letters)
std::string trimmedString(const std::string& s);       // whitespaceAndNewlineCharacterSet trim

// ---- arm64 conversions (keep iOS results for odd inputs without C++ UB) ------------------------
int fcvtzs(double value);            // (int)value, saturating, NaN -> 0 (arm64 fcvtzs)
unsigned int fcvtzu(double value);   // (unsigned)value, saturating, negative/NaN -> 0

// ---- UIView ---------------------------------------------------------------------------------
class View : public cocos2d::ui::Layout
{
public:
    static View* create(const cocos2d::Rect& frame);
    // -[UIView initWithFrame:]. Sets content size, clears the background, swallows touches.
    virtual bool initWithFrame(const cocos2d::Rect& frame);

    const cocos2d::Rect& frame() const { return _frame; }
    // Moves/resizes this view inside its superview (re-flips its position) and lays out children.
    virtual void setFrame(const cocos2d::Rect& frame);
    cocos2d::Rect bounds() const;  // (0, 0, frame.size)

    void setBackgroundColor(const cocos2d::Color4B& color);  // alpha 0 == clearColor
    void setUserInteractionEnabled(bool enabled);            // touch + swallow on/off

    // -[UIView addSubview:]. A View child is placed by its own frame(); any other node by the
    // frame passed here (remembered so layoutSubviews() can re-place it).
    void addSubview(View* view);
    virtual void addSubview(cocos2d::Node* node, const cocos2d::Rect& frame);
    void setSubviewFrame(cocos2d::Node* node, const cocos2d::Rect& frame);  // -[UIView setFrame:] on a plain node
    cocos2d::Rect subviewFrame(cocos2d::Node* node) const;
    View* superview() const;

    // -[UIView removeFromSuperview]. The iOS subclasses override this to tear down observers;
    // the base removes the node from its parent (with cleanup).
    virtual void removeFromSuperview();

    // Re-applies every stored subview frame (call after this view's height changed).
    virtual void layoutSubviews();

    // Keeps the frame list in sync when a child is removed by other means.
    void removeChild(cocos2d::Node* child, bool cleanup = true) override;

protected:
    View();
    ~View() override;

    // Node that receives subviews (ScrollView: the scroller's inner container).
    virtual cocos2d::Node* contentNode();
    // Height (points) used to flip subview frames: bounds height, or the scroll content height.
    virtual float contentHeightForLayout() const;
    void placeSubview(cocos2d::Node* node, const cocos2d::Rect& frame);

    cocos2d::Rect _frame;                                         // UIView frame (points)
    // Subviews and their frames (retained; entries whose node left contentNode() are dropped).
    std::vector<std::pair<cocos2d::RefPtr<cocos2d::Node>, cocos2d::Rect>> _subviewFrames;
};

// ---- UIScrollView ---------------------------------------------------------------------------
// A View holding a vertical ui::ScrollView; subviews go into the scroll content, flipped
// against the content height (which may exceed the frame).
class ScrollView : public View
{
public:
    static ScrollView* create(const cocos2d::Rect& frame);
    bool initWithFrame(const cocos2d::Rect& frame) override;

    cocos2d::Size contentSize() const { return _scrollContentSize; }   // points
    void setScrollContentSize(const cocos2d::Size& size);              // -[UIScrollView setContentSize:]
    cocos2d::Vec2 contentOffset() const;                               // points, y down
    void setContentOffset(const cocos2d::Vec2& offset, bool animated = false);
    // UIEdgeInsets (top, left, bottom, right), points. Bottom inset extends the scrollable area.
    void setContentInset(float top, float left, float bottom, float right);
    cocos2d::Rect contentInset() const;  // packed as (top, left, bottom, right)
    void setScrollIndicatorInsets(float top, float left, float bottom, float right);  // no-op on PC
    void setScrollEnabled(bool enabled);

    cocos2d::ui::ScrollView* scroller() const { return _scroller; }

protected:
    ScrollView();
    cocos2d::Node* contentNode() override;
    float contentHeightForLayout() const override;
    void applyScrollContentSize();

    cocos2d::ui::ScrollView* _scroller;
    cocos2d::Size _scrollContentSize;
    float _insetTop, _insetLeft, _insetBottom, _insetRight;
};

// ---- UISlider -------------------------------------------------------------------------------------
// Value 0..1 (setValue clamps, NaN -> 0). Like UISlider, only a touch that starts on the thumb
// (28 pt, +10 pt slop) is tracked; dragging changes the value continuously relative to the start
// (ValueChanged on every change), touch-down / touch-up(inside, outside, cancel) are reported.
// Drawn in code: 2 pt track (minimum side tinted), white thumb with a soft shadow.
class Slider : public cocos2d::ui::Widget
{
public:
    enum class Event { TouchDown, ValueChanged, TouchUp };  // UIControlEvents 0x1 / 0x1000 / 0x40|0x80|0x100
    using Callback = std::function<void(Slider* slider, Event event)>;

    static Slider* create();
    float value() const { return _value; }
    void setValue(float value);
    void setMinimumTrackTintColor(const cocos2d::Color4B& color);
    void setCallback(const Callback& callback) { _callback = callback; }
    void setSliderEnabled(bool enabled);   // -[UIControl setEnabled:] (dims to 0.5)
    bool isSliderEnabled() const { return _sliderEnabled; }

    bool onTouchBegan(cocos2d::Touch* touch, cocos2d::Event* event) override;
    void onTouchMoved(cocos2d::Touch* touch, cocos2d::Event* event) override;
    void onTouchEnded(cocos2d::Touch* touch, cocos2d::Event* event) override;
    void onTouchCancelled(cocos2d::Touch* touch, cocos2d::Event* event) override;

protected:
    Slider();
    bool init() override;
    void onSizeChanged() override;
    void redraw();
    float thumbCenterX() const;   // design units, local

    cocos2d::DrawNode* _draw;
    cocos2d::Color4B _minimumTrackTintColor;
    float _value;
    float _touchStartValue;
    float _touchStartX;
    bool _tracking;
    bool _sliderEnabled;
    Callback _callback;
};

// ---- UISwitch -------------------------------------------------------------------------------------
// 51 x 31 pt pill drawn in code; a tap toggles it and reports ValueChanged (UIControlEventValueChanged).
class Switch : public cocos2d::ui::Widget
{
public:
    using Callback = std::function<void(Switch* sender)>;
    static constexpr float kWidth = 51.0f;    // points (UISwitch intrinsic size)
    static constexpr float kHeight = 31.0f;

    static Switch* create();
    bool isOn() const { return _on; }
    void setOn(bool on, bool animated = false);
    void setOnTintColor(const cocos2d::Color4B& color);
    void setCallback(const Callback& callback) { _callback = callback; }
    void setSwitchEnabled(bool enabled);
    bool isSwitchEnabled() const { return _switchEnabled; }

protected:
    Switch();
    bool init() override;
    void onSizeChanged() override;
    void releaseUpEvent() override;
    void redraw();

    cocos2d::DrawNode* _draw;
    cocos2d::Color4B _onTintColor;
    bool _on;
    bool _switchEnabled;
    Callback _callback;
};

// ---- the window ([CCDirector sharedDirector].view) -------------------------------------------
// Full-screen, transparent View added to the running scene above everything else
// (z = kWindowZOrder); editor panels and view controllers are added here. Recreated when the
// running scene changes. Touches pass through where no panel is.
constexpr int kWindowZOrder = 100000;
View* window();

// ---- NSNotificationCenter ----------------------------------------------------------------------
// Notifications are EventCustom events on the Director's dispatcher: event name = notification
// name, userData = the notification's object. Observers are grouped by owner so
// removeObserver(owner) drops all of them (-[NSNotificationCenter removeObserver:]).
// `objectFilter` != nullptr only delivers notifications posted with that object
// (addObserver:selector:name:object:).
class NotificationCenter
{
public:
    using Callback = std::function<void(void* object, void* userInfo)>;

    // EventCustom userData = object; userInfo is available to observers during the dispatch.
    static void postNotification(const std::string& name, void* object, void* userInfo = nullptr);
    static void addObserver(const void* owner, const std::string& name, void* objectFilter, Callback callback);
    static void removeObserver(const void* owner);                                             // all names
    static void removeObserver(const void* owner, const std::string& name, void* objectFilter); // one name
};

// iOS notification names used by the editor UI (exact iOS strings).
namespace notification {
constexpr const char* kEditorViewClosed = "editor_view_closed";        // object: the EditorUIView
constexpr const char* kEditorViewItemAdded = "editor_view_item_added"; // object: int* level-item id
constexpr const char* kIOGainsFocus = "io_gains_focus";                // object: the InputObject
constexpr const char* kSelRefChange = "sel_ref_change";                // object: cocos2d::Vector<Special*>* (selected refs)
constexpr const char* kRefUIKeysWillChange = "ref_ui_keys_will_change";// object: the Special
constexpr const char* kRefUIKeysChanged = "ref_ui_keys_changed";       // object: the Special
constexpr const char* kUndoStackUpdated = "undo_stack_updated";        // object: the EditorUndoManager
constexpr const char* kSelRectChanged = "sel_rect_changed";            // object: nullptr
constexpr const char* kMainMenuClosed = "main_menu_closed";            // object: nullptr
constexpr const char* kSaveLevelCancel = "save_level_cancel";          // posted by SaveLevelViewController (E5)
constexpr const char* kSaveLevelDone = "save_level_done";              // posted by SaveLevelViewController (E5)
}  // namespace notification

}  // namespace uikit
