#include "EditorViewController.h"

#include <algorithm>

USING_NS_CC;

namespace {
RefPtr<EditorViewController>& rootPresented()
{
    static RefPtr<EditorViewController> s_rootPresented;
    return s_rootPresented;
}
const int kModalPresentationCurrentContext = 3;  // UIModalPresentationCurrentContext
}  // namespace

// ================================================================================================
// EditorViewController
// ================================================================================================

EditorViewController::EditorViewController()
    : _viewLoaded(false), _presentedByRoot(false), _modalPresentationStyle(0), _contentSizeForViewInPopover(Size::ZERO),
      _presentingViewController(nullptr), _presentedViewController(nullptr)
{
}

EditorViewController::~EditorViewController() { CC_SAFE_RELEASE_NULL(_presentedViewController); }

// @ios 10010e2b8
bool EditorViewController::initWithNibName(const std::string& nibName)
{
    const Size window = uikit::windowSize();
    if (!uikit::View::initWithFrame(Rect(0, 0, window.width, window.height))) return false;
    _nibName = nibName;
    return true;
}

// @ios 10010e2ec
void EditorViewController::viewDidLoad() {}

// @ios 10010e320
void EditorViewController::didReceiveMemoryWarning() {}

// @ios 10010e354
void EditorViewController::skinButton(ui::Button* button, int style)
{
    Color4B titleColor = uikit::color(1.0, 1.0, 1.0, 1.0);
    const char* prefix;
    switch (style)
    {
    case 1: prefix = "blue"; break;
    case 2:
        titleColor = uikit::color(0.0, 0.0, 0.0, 1.0);
        prefix = "yellow";
        break;
    case 3: prefix = "grey"; break;
    case 4: prefix = "darkGrey"; break;
    default: prefix = "pink"; break;
    }
    uikit::skinButton(button, prefix);
    button->setTitleColor(Color3B(titleColor.r, titleColor.g, titleColor.b));
    if (button->getTitleRenderer()) button->getTitleRenderer()->setOpacity(titleColor.a);
}

void EditorViewController::viewWillAppear(bool /*animated*/) {}
void EditorViewController::viewDidAppear(bool /*animated*/) {}
void EditorViewController::viewWillDisappear(bool /*animated*/) {}

void EditorViewController::show(bool animated, const std::function<void()>& completion)
{
    RefPtr<EditorViewController> keep(this);
    if (!_viewLoaded)
    {
        _viewLoaded = true;
        viewDidLoad();
    }
    viewWillAppear(animated);
    viewDidAppear(animated);
    if (completion) completion();
}

void EditorViewController::hide(bool animated, const std::function<void()>& completion)
{
    RefPtr<EditorViewController> keep(this);
    viewWillDisappear(animated);
    removeFromSuperview();
    if (completion) completion();
}

void EditorViewController::presentViewController(EditorViewController* viewController, bool animated,
                                                 const std::function<void()>& completion)
{
    if (viewController == nullptr) return;
    if (_presentedViewController != nullptr)
    {
        log("EditorViewController: %s is already presenting a view controller", _nibName.c_str());
        return;
    }
    _presentedViewController = viewController;
    _presentedViewController->retain();
    viewController->_presentingViewController = this;
    viewController->_presentedByRoot = false;
    if (viewController->_modalPresentationStyle == kModalPresentationCurrentContext && superview() != nullptr)
    {
        // Covers the presenting controller's view only (e.g. inside the editor-menu popover).
        viewController->setFrame(frame());
        superview()->addSubview(viewController);
    }
    else
    {
        const Size window = uikit::windowSize();
        viewController->setFrame(Rect(0, 0, window.width, window.height));
        uikit::window()->addSubview(viewController);
    }
    viewController->show(animated, completion);
}

void EditorViewController::dismissViewControllerAnimated(bool animated, const std::function<void()>& completion)
{
    RefPtr<EditorViewController> keep(this);
    if (_presentedViewController != nullptr)
    {
        EditorViewController* presented = _presentedViewController;
        if (presented->_presentedViewController != nullptr) presented->dismissViewControllerAnimated(animated, nullptr);
        presented->hide(animated, nullptr);
        presented->_presentingViewController = nullptr;
        _presentedViewController = nullptr;
        presented->release();
        // The presenting controller reappears.
        viewWillAppear(animated);
        viewDidAppear(animated);
        if (completion) completion();
        return;
    }
    if (_presentingViewController != nullptr)
    {
        _presentingViewController->dismissViewControllerAnimated(animated, completion);
        return;
    }
    if (_presentedByRoot)
    {
        dismissRootPresented(animated, completion);
        return;
    }
    if (completion) completion();
}

void EditorViewController::presentOnRoot(EditorViewController* viewController, bool animated,
                                         const std::function<void()>& completion)
{
    if (viewController == nullptr) return;
    if (rootPresented() && rootPresented()->getParent() != nullptr)
    {
        log("EditorViewController: the root is already presenting a view controller");
        return;
    }
    rootPresented() = viewController;
    viewController->_presentedByRoot = true;
    viewController->_presentingViewController = nullptr;
    const Size window = uikit::windowSize();
    viewController->setFrame(Rect(0, 0, window.width, window.height));
    uikit::window()->addSubview(viewController);
    viewController->show(animated, completion);
}

void EditorViewController::dismissRootPresented(bool animated, const std::function<void()>& completion)
{
    RefPtr<EditorViewController> presented = rootPresented();
    rootPresented() = nullptr;
    if (presented)
    {
        if (presented->_presentedViewController != nullptr) presented->dismissViewControllerAnimated(animated, nullptr);
        presented->_presentedByRoot = false;
        presented->hide(animated, nullptr);
    }
    if (completion) completion();
}

EditorViewController* EditorViewController::rootPresentedViewController() { return rootPresented().get(); }

// ================================================================================================
// EditorPopoverController
// ================================================================================================

EditorPopoverController::EditorPopoverController() : _popoverContentSize(Size::ZERO), _container(nullptr), _visible(false) {}

EditorPopoverController::~EditorPopoverController() {}

EditorPopoverController* EditorPopoverController::create(EditorViewController* contentViewController)
{
    EditorPopoverController* popover = new (std::nothrow) EditorPopoverController();
    if (popover && popover->initWithContentViewController(contentViewController))
    {
        popover->autorelease();
        return popover;
    }
    delete popover;
    return nullptr;
}

bool EditorPopoverController::initWithContentViewController(EditorViewController* contentViewController)
{
    const Size window = uikit::windowSize();
    if (!uikit::View::initWithFrame(Rect(0, 0, window.width, window.height))) return false;
    _contentViewController = contentViewController;
    setUserInteractionEnabled(true);  // swallow touches outside the content
    addClickEventListener([this](Ref*) {
        // A tap outside the content (the content view swallows its own touches).
        RefPtr<EditorPopoverController> keep(this);
        if (!_visible) return;
        if (shouldDismissPopover && !shouldDismissPopover(this)) return;
        dismissPopoverAnimated(true);
        if (didDismissPopover) didDismissPopover(this);
    });
    return true;
}

void EditorPopoverController::setPopoverContentSize(const Size& size) { _popoverContentSize = size; }

void EditorPopoverController::presentPopoverFromRect(const Rect& rect, unsigned int /*permittedArrowDirections*/,
                                                     bool animated)
{
    if (_visible || !_contentViewController) return;
    EditorViewController* content = _contentViewController.get();
    Size size = _popoverContentSize;
    if (size.width <= 0.0f || size.height <= 0.0f) size = content->contentSizeForViewInPopover();
    if (size.width <= 0.0f || size.height <= 0.0f) size = Size(320.0f, 480.0f);  // UIPopover default

    // Placement next to the anchor (arrow side chosen like UIKit's "any": below, above, right, left).
    const Size window = uikit::windowSize();
    const float margin = 10.0f, arrow = 13.0f, border = 1.0f;
    const float w = size.width + border * 2, h = size.height + border * 2;
    auto clampX = [&](float x) { return std::max(margin, std::min(window.width - margin - w, x)); };
    auto clampY = [&](float y) { return std::max(margin, std::min(window.height - margin - h, y)); };
    float x, y;
    if (rect.getMaxY() + arrow + h <= window.height - margin)
    {
        x = clampX(rect.getMidX() - w * 0.5f);
        y = rect.getMaxY() + arrow;
    }
    else if (rect.getMinY() - arrow - h >= margin)
    {
        x = clampX(rect.getMidX() - w * 0.5f);
        y = rect.getMinY() - arrow - h;
    }
    else if (rect.getMaxX() + arrow + w <= window.width - margin)
    {
        x = rect.getMaxX() + arrow;
        y = clampY(rect.getMidY() - h * 0.5f);
    }
    else
    {
        x = std::max(margin, rect.getMinX() - arrow - w);
        y = clampY(rect.getMidY() - h * 0.5f);
    }

    if (_container == nullptr)
    {
        _container = uikit::View::create(Rect(x, y, w, h));
        _container->setBackgroundColor(uikit::color(0.1, 0.1, 0.1, 0.9));
        addSubview(_container);
    }
    else
    {
        setSubviewFrame(_container, Rect(x, y, w, h));
    }
    content->setFrame(Rect(border, border, size.width, size.height));
    _container->addSubview(content);
    uikit::window()->addSubview(this);
    _visible = true;
    content->show(animated, nullptr);
}

void EditorPopoverController::dismissPopoverAnimated(bool animated)
{
    if (!_visible) return;
    RefPtr<EditorPopoverController> keep(this);
    _visible = false;
    if (_contentViewController)
    {
        EditorViewController* content = _contentViewController.get();
        if (content->presentedViewController() != nullptr) content->dismissViewControllerAnimated(animated, nullptr);
        content->viewWillDisappear(animated);
        content->removeFromSuperview();
    }
    removeFromSuperview();
}
