#pragma once
// iOS: EditorUIView : UIView (instanceStart 0x8, instanceSize 0x30).
// Base of the editor's side panels (AddSpecialItemUIView, SelectBackgroundUIView,
// EditParametersView, AlignRefsView; UserLevelSelectUIView in E5). EditorLayer creates one with a
// frame in window points (right-hand column: x = winW - notch - min(winW/2, 284), full height),
// adds it to uikit::window() and observes "editor_view_closed" with the panel as object.
//
// Layout built by initWithFrame: (w, h = frame size; btnWidth 66, btnHeight 30, space 8):
//   self background  rgba(0.706, 0.706, 0.706, 1)
//   scrollView       (8, 8, w - 16, h - 54)          background rgba(0.8, 0.8, 0.8, 1)
//   menuLabel        (16, (int)(h - 46), w/2 - 16, 46)  Helvetica-Bold 16, white, gray shadow
//                    (0.5, 0.5), left aligned, adjustsFontSizeToFitWidth, text ""
//   closeBtn         (w - 74, h - 38, 66, 30)  pink button, title capitalized
//                    Localization::get("CLOSE"), target closeView:
//
// Buttons: buttonWithTitle:prefix:frame: makes a UIButtonTypeSystem button with
// "<prefix>Button.png" / "<prefix>ButtonHighlight.png" (loose @2x PNGs in the iOS bundle,
// resizable, cap insets 4pt), title color rgba(1,1,1,0.9), title shadow black.

#include <string>

#include "UIKitCompat.h"

class EditorUIView;

// iOS protocol <EditorUIViewLayerDelegate>, implemented by EditorLayer (E3).
// Only SelectBackgroundUIView sends it: key "bg" (value int: background index, see
// SelectBackgroundUIView.h) and "bgColor" (value int 0xRRGGBB). userInfo is nil in every iOS call
// (an empty map here); EditorLayer reads userInfo["color"] when value == -1 for "bg".
class EditorUIViewLayerDelegate
{
public:
    virtual ~EditorUIViewLayerDelegate() {}
    // -editorUIView:changedValue:forKey:userInfo:   (EditorLayer @ios 100007e74)
    virtual void editorUIView(EditorUIView* view, const cocos2d::Value& changedValue,
                              const std::string& key, const cocos2d::ValueMap& userInfo) = 0;
};

class EditorUIView : public uikit::View
{
public:
    static EditorUIView* create(const cocos2d::Rect& frame);
    bool initWithFrame(const cocos2d::Rect& frame) override;                          // @ios 1000ef2bc

    // Class methods (+buttonWithTitle:frame: etc.). frame in points of the future superview.
    static cocos2d::ui::Button* buttonWithTitle(const std::string& title, const cocos2d::Rect& frame);       // @ios 1000ef50c  (= pink)
    static cocos2d::ui::Button* yellowButtonWithTitle(const std::string& title, const cocos2d::Rect& frame); // @ios 1000ef518
    static cocos2d::ui::Button* blueButtonWithTitle(const std::string& title, const cocos2d::Rect& frame);   // @ios 1000ef52c
    static cocos2d::ui::Button* pinkButtonWithTitle(const std::string& title, const cocos2d::Rect& frame);   // @ios 1000ef540
    static cocos2d::ui::Button* buttonWithTitle(const std::string& title, const std::string& prefix,
                                                const cocos2d::Rect& frame);                                 // @ios 1000ef554
    // +lineAt:width: - a 1-line separator view at `point`, black with alpha 0.15.
    static uikit::View* lineAt(const cocos2d::Vec2& point, float width);                                    // @ios 1000ef6d4

    void addStandardCloseButton();                                                    // @ios 1000ef760
    // Posts "editor_view_closed" with this view as object (EditorLayer::hideMenu: removes us).
    virtual void closeView(cocos2d::Ref* sender);                                     // @ios 1000ef864
    // Detaches the close button target, removes every NotificationCenter observer owned by
    // this view, then removes it from the window.
    void removeFromSuperview() override;                                              // @ios 1000ef898

    EditorUIViewLayerDelegate* delegate() const;                                      // @ios 1000ef910
    void setDelegate(EditorUIViewLayerDelegate* delegate);                            // @ios 1000ef920

    uikit::ScrollView* scrollView() const { return _scrollView; }
    cocos2d::ui::Text* menuLabel() const { return _menuLabel; }
    cocos2d::ui::Button* closeBtn() const { return _closeBtn; }

protected:
    EditorUIView();
    ~EditorUIView() override;

    unsigned short _btnWidth;                     // +0x08  66
    unsigned short _btnHeight;                    // +0x0a  30
    unsigned short _space;                        // +0x0c  8
    uikit::ScrollView* _scrollView;               // +0x10  UIScrollView
    cocos2d::ui::Text* _menuLabel;                // +0x18  UILabel
    cocos2d::ui::Button* _closeBtn;               // +0x20  UIButton
    EditorUIViewLayerDelegate* _delegate;         // +0x28  id<EditorUIViewLayerDelegate> (assign)
};
