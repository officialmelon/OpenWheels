#include "EditorUIView.h"

#include <algorithm>
#include <cmath>

#include "platform/common/Localization.h"

USING_NS_CC;


EditorUIView::EditorUIView()
    : _btnWidth(0), _btnHeight(0), _space(0), _scrollView(nullptr), _menuLabel(nullptr), _closeBtn(nullptr), _delegate(nullptr)
{
}

EditorUIView::~EditorUIView() { uikit::NotificationCenter::removeObserver(this); }

EditorUIView* EditorUIView::create(const Rect& frame)
{
    EditorUIView* view = new (std::nothrow) EditorUIView();
    if (view && view->initWithFrame(frame))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

// @ios 1000ef2bc
bool EditorUIView::initWithFrame(const Rect& frame)
{
    if (!uikit::View::initWithFrame(frame)) return false;
    _btnWidth = 66;
    _btnHeight = 30;
    _space = 8;
    const double w = frame.size.width;
    const double scrollH = frame.size.height - (static_cast<double>(_btnHeight) + _space * 3);
    const int scrollHInt = uikit::fcvtzs(scrollH);

    _scrollView = uikit::ScrollView::create(Rect(_space, _space, static_cast<float>(w - _space * 2),
                                                 static_cast<float>(std::trunc(scrollH))));
    _scrollView->setBackgroundColor(uikit::color(0.800000011920929, 0.800000011920929, 0.800000011920929, 1.0));
    addSubview(_scrollView);

    _menuLabel = uikit::makeLabel("", true, 16.0f, 0);
    uikit::setLabelShadow(_menuLabel, uikit::grayColor(), Size(0.5f, 0.5f));
    _menuLabel->setTextColor(uikit::whiteColor());
    addSubview(_menuLabel, Rect(_space * 2, static_cast<float>(_space + scrollHInt), static_cast<float>(w * 0.5 - _space * 2),
                                static_cast<float>(_space * 2 + _btnHeight)));
    addStandardCloseButton();
    setBackgroundColor(uikit::color(0.7058823704719543, 0.7058823704719543, 0.7058823704719543, 1.0));
    return true;
}

// @ios 1000ef50c
ui::Button* EditorUIView::buttonWithTitle(const std::string& title, const Rect& frame)
{
    return pinkButtonWithTitle(title, frame);
}

// @ios 1000ef518
ui::Button* EditorUIView::yellowButtonWithTitle(const std::string& title, const Rect& frame)
{
    return buttonWithTitle(title, "yellow", frame);
}

// @ios 1000ef52c
ui::Button* EditorUIView::blueButtonWithTitle(const std::string& title, const Rect& frame)
{
    return buttonWithTitle(title, "blue", frame);
}

// @ios 1000ef540
ui::Button* EditorUIView::pinkButtonWithTitle(const std::string& title, const Rect& frame)
{
    return buttonWithTitle(title, "pink", frame);
}

// @ios 1000ef554
ui::Button* EditorUIView::buttonWithTitle(const std::string& title, const std::string& prefix, const Rect& /*frame*/)
{
    // [UIButton buttonWithType:UIButtonTypeSystem]; the frame is applied when the caller adds the
    // button (uikit::View::addSubview).
    ui::Button* button = ui::Button::create();
    uikit::skinButton(button, prefix);
    // setTitleColor:[UIColor colorWithRed:1 green:1 blue:1 alpha:0.9]; title shadow black (UIButton's
    // default title shadow offset is zero, so it is not visible). System font 15 pt.
    uikit::setButtonTitle(button, title, 15.0f, uikit::color(1.0, 1.0, 1.0, 0.8999999761581421));
    return button;
}

// @ios 1000ef6d4
uikit::View* EditorUIView::lineAt(const Vec2& point, float width)
{
    uikit::View* line = uikit::View::create(Rect(point.x, point.y, width, 1.0f));
    line->setBackgroundColor(uikit::color(0, 0, 0, 0.15000000596046448));
    line->setUserInteractionEnabled(false);
    return line;
}

// @ios 1000ef760
void EditorUIView::addStandardCloseButton()
{
    const std::string title = uikit::capitalizedString(Localization::get("CLOSE"));
    const Rect f = frame();
    const Rect btnFrame(static_cast<float>(f.size.width - (static_cast<double>(_btnWidth) + _space)),
                        static_cast<float>(f.size.height - (static_cast<double>(_btnHeight) + _space)), _btnWidth,
                        _btnHeight);
    _closeBtn = buttonWithTitle(title, btnFrame);
    _closeBtn->addClickEventListener([this](Ref* sender) { closeView(sender); });
    addSubview(_closeBtn, btnFrame);
}

// @ios 1000ef864
void EditorUIView::closeView(Ref* /*sender*/)
{
    uikit::NotificationCenter::postNotification(uikit::notification::kEditorViewClosed, this);
}

// @ios 1000ef898
void EditorUIView::removeFromSuperview()
{
    if (_closeBtn != nullptr) _closeBtn->addClickEventListener(nullptr);
    uikit::NotificationCenter::removeObserver(this);
    RefPtr<EditorUIView> keep(this);
    uikit::View::removeFromSuperview();
}

// @ios 1000ef910
EditorUIViewLayerDelegate* EditorUIView::delegate() const { return _delegate; }

// @ios 1000ef920
void EditorUIView::setDelegate(EditorUIViewLayerDelegate* delegate) { _delegate = delegate; }
