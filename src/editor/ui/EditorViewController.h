#pragma once
// iOS: EditorViewController : UIViewController (instanceStart 0x8, instanceSize 0x8 - no ivars).
// Base of the editor's nib-backed screens: EditorMenuViewController (E4),
// SaveLevelViewController and LoadLevelViewController (E5).
//
// Port: a UIViewController is a full-window uikit::View; its nib layout is rebuilt in code by
// the subclass in viewDidLoad (frames recovered from the compiled .nib, see docs/editor/E4.md).
// Presentation follows UIKit modal semantics:
//   * presentOnRoot(vc)  == [[CCDirector sharedDirector] presentViewController:vc ...]
//     (CCDirector is the root view controller on iOS): vc is added to uikit::window();
//   * a->presentViewController(b) presents b above a (b->presentingViewController() == a);
//   * vc->dismissViewControllerAnimated(): if vc presented something, dismisses that; otherwise
//     asks its presenter to dismiss vc (UIKit rule); dismissRootPresented() == the CCDirector
//     dismissing whatever it presented.
// Life cycle: the first presentation calls viewDidLoad (once), then viewWillAppear(animated);
// after it is shown viewDidAppear(animated); dismissal calls viewWillDisappear(animated) before
// removing the view. modalPresentationStyle is stored only (all styles cover the window).
//
// skinButton:style: - background "<prefix>Button.png" / "<prefix>ButtonHighlight.png" (loose
// @2x PNGs from the iOS bundle, cap insets 4pt), title color white (style 2: black):
//   style 1 blue, 2 yellow, 3 grey, 4 darkGrey, anything else pink.

#include <functional>
#include <string>

#include "UIKitCompat.h"

class EditorViewController : public uikit::View
{
public:
    // initWithNibName:bundle: (the bundle argument is always the main bundle). The nib name is
    // kept for reference only; the subclass builds the layout in viewDidLoad.
    virtual bool initWithNibName(const std::string& nibName);                       // @ios 10010e2b8
    virtual void viewDidLoad();                                                     // @ios 10010e2ec
    virtual void didReceiveMemoryWarning();                                         // @ios 10010e320
    void skinButton(cocos2d::ui::Button* button, int style);                        // @ios 10010e354

    // UIViewController behaviour inherited by every subclass (no iOS code of its own).
    virtual void viewWillAppear(bool animated);
    virtual void viewDidAppear(bool animated);
    virtual void viewWillDisappear(bool animated);

    void presentViewController(EditorViewController* viewController, bool animated,
                               const std::function<void()>& completion);
    void dismissViewControllerAnimated(bool animated, const std::function<void()>& completion);
    EditorViewController* presentingViewController() const { return _presentingViewController; }
    EditorViewController* presentedViewController() const { return _presentedViewController; }
    bool isPresentedByRoot() const { return _presentedByRoot; }
    void setModalPresentationStyle(int style) { _modalPresentationStyle = style; }
    int modalPresentationStyle() const { return _modalPresentationStyle; }
    const std::string& nibName() const { return _nibName; }

    // The CCDirector (root view controller) side.
    static void presentOnRoot(EditorViewController* viewController, bool animated,
                              const std::function<void()>& completion);
    static void dismissRootPresented(bool animated, const std::function<void()>& completion);
    static EditorViewController* rootPresentedViewController();

protected:
    EditorViewController();
    ~EditorViewController() override;

    void show(bool animated, const std::function<void()>& completion);  // shared by both present paths
    void hide(bool animated, const std::function<void()>& completion);

    friend class EditorPopoverController;

    // Port state (UIViewController internals, not iOS ivars).
    std::string _nibName;
    bool _viewLoaded;
    bool _presentedByRoot;
    int _modalPresentationStyle;
    cocos2d::Size _contentSizeForViewInPopover;       // points (setContentSizeForViewInPopover:)
    EditorViewController* _presentingViewController;  // assign
    EditorViewController* _presentedViewController;   // retained while presented

public:
    void setContentSizeForViewInPopover(const cocos2d::Size& size) { _contentSizeForViewInPopover = size; }
    const cocos2d::Size& contentSizeForViewInPopover() const { return _contentSizeForViewInPopover; }
};

// UIPopoverController stand-in (iPad editor menu, EditorLayer::showMainMenuBtnPressed:). Not an
// iOS-binary class. The popover is a full-window, transparent, touch-swallowing View: a touch
// outside the content asks shouldDismissPopover (default true) and then dismisses and calls
// didDismissPopover (UIKit only reports user dismissals; dismissPopoverAnimated: does not).
// The content view controller is shown in a frame of popoverContentSize (default: its
// contentSizeForViewInPopover) next to the anchor rect, inside the window with a 10 pt margin,
// with a dark rounded border (the iOS popover chrome).
class EditorPopoverController : public uikit::View
{
public:
    // initWithContentViewController:
    static EditorPopoverController* create(EditorViewController* contentViewController);
    EditorViewController* contentViewController() const { return _contentViewController; }
    void setPopoverContentSize(const cocos2d::Size& size);   // points
    const cocos2d::Size& popoverContentSize() const { return _popoverContentSize; }
    // presentPopoverFromRect:inView:permittedArrowDirections:animated: - `rect` in window points
    // (top-left origin). Adds the popover to uikit::window(); content viewDidLoad (once) /
    // viewWillAppear / viewDidAppear.
    void presentPopoverFromRect(const cocos2d::Rect& rect, unsigned int permittedArrowDirections, bool animated);
    // dismissPopoverAnimated: - content viewWillDisappear, then removed from the window.
    void dismissPopoverAnimated(bool animated);
    bool isPopoverVisible() const { return _visible; }

    // <UIPopoverControllerDelegate> (EditorLayer binds its popoverController* methods).
    std::function<bool(EditorPopoverController*)> shouldDismissPopover;
    std::function<void(EditorPopoverController*)> didDismissPopover;

protected:
    EditorPopoverController();
    ~EditorPopoverController() override;
    bool initWithContentViewController(EditorViewController* contentViewController);

    cocos2d::RefPtr<EditorViewController> _contentViewController;
    cocos2d::Size _popoverContentSize;
    uikit::View* _container;   // chrome + content
    bool _visible;
};
