#include "AlignRefsView.h"

#include <algorithm>
#include <cmath>

#include "EditorSpriteBatchNode.h"
#include "Special.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace {
// [UIButton buttonWithType:UIButtonTypeRoundedRect]: a borderless button with tinted text.
ui::Button* systemButton(const std::string& title)
{
    ui::Button* button = ui::Button::create();
    button->setTitleFontName(uikit::fontFile(false));
    button->setTitleFontSize(15.0f * uikit::pointsToDesign());
    button->setTitleColor(Color3B(0, 122, 255));
    button->setTitleText(title);
    button->setPressedActionEnabled(false);
    return button;
}
}  // namespace

AlignRefsView::AlignRefsView()
    : _sbn(nullptr), _upBtn(nullptr), _downBtn(nullptr), _leftBtn(nullptr), _rightBtn(nullptr)
{
}

AlignRefsView* AlignRefsView::create(const Rect& frame, const Vector<Special*>& specials, EditorSpriteBatchNode* editorSBN)
{
    AlignRefsView* view = new (std::nothrow) AlignRefsView();
    if (view && view->initWithFrame(frame, specials, editorSBN))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

// @ios 1000dced8
bool AlignRefsView::initWithFrame(const Rect& frame, const Vector<Special*>& specials, EditorSpriteBatchNode* editorSBN)
{
    const Size winSize = uikit::windowSize();
    if (!EditorUIView::initWithFrame(frame)) return false;
    _refs = specials;
    _sbn = editorSBN;
    _menuLabel->setString(uikit::capitalizedString(Localization::get("ALIGN OBJECTS")));

    // OK sits at the window's bottom inside the scroll view (beyond its bounds on iOS too).
    const float quarter = static_cast<float>(winSize.width * 0.5) * 0.5f;
    ui::Button* ok = systemButton(Localization::get("OK"));
    ok->addClickEventListener([this](Ref* sender) { confirmChanges(sender); });
    _scrollView->addSubview(ok, Rect(quarter, static_cast<float>(winSize.height + -44.0), quarter, 44.0f));

    struct Spec
    {
        ui::Button** button;
        const char* title;
        Rect frame;
    };
    const Spec specs[4] = {{&_upBtn, "up", Rect(22, 0, 44, 44)},
                           {&_downBtn, "down", Rect(88, 0, 44, 44)},
                           {&_leftBtn, "left", Rect(22, 44, 44, 44)},
                           {&_rightBtn, "right", Rect(88, 44, 44, 44)}};
    for (const Spec& spec : specs)
    {
        *spec.button = systemButton(Localization::get(spec.title));
        (*spec.button)->addClickEventListener([this](Ref* sender) { handleAlignBtnPress(sender); });
        _scrollView->addSubview(*spec.button, spec.frame);
    }
    return true;
}

// @ios 1000dd204
void AlignRefsView::handleAlignBtnPress(Ref* sender)
{
    if (_refs.empty()) return;
    // Union of the refs' bounding boxes (CGRect doubles), kept in float like the original.
    const cg::Rect first = _refs.at(0)->refBoundingBox();
    float minX = static_cast<float>(first.origin.x);
    float maxX = static_cast<float>(first.origin.x + first.size.width);
    float minY = static_cast<float>(first.origin.y);
    float maxY = static_cast<float>(first.origin.y + first.size.height);
    for (ssize_t i = 1; i < _refs.size(); ++i)
    {
        const cg::Rect bb = _refs.at(i)->refBoundingBox();
        maxX = static_cast<float>(std::fmax(bb.origin.x + bb.size.width, static_cast<double>(maxX)));
        maxY = static_cast<float>(std::fmax(bb.origin.y + bb.size.height, static_cast<double>(maxY)));
        if (!(bb.origin.x > static_cast<double>(minX))) minX = static_cast<float>(bb.origin.x);
        if (!(bb.origin.y > static_cast<double>(minY))) minY = static_cast<float>(bb.origin.y);
    }

    for (Special* ref : _refs)
    {
        const cg::Rect bb = ref->refBoundingBox();
        const Vec2 pos = ref->getPosition();
        if (sender == _upBtn)
        {
            const float d = static_cast<float>(static_cast<double>(pos.y) - (bb.origin.y + bb.size.height));
            ref->setPosition(Vec2(pos.x, maxY + d));
        }
        else if (sender == _downBtn)
        {
            const float d = static_cast<float>(static_cast<double>(pos.y) - bb.origin.y);
            ref->setPosition(Vec2(pos.x, minY + d));
        }
        else if (sender == _leftBtn)
        {
            const float d = static_cast<float>(static_cast<double>(pos.x) - bb.origin.x);
            ref->setPosition(Vec2(minX + d, pos.y));
        }
        else if (sender == _rightBtn)
        {
            const float d = static_cast<float>(static_cast<double>(pos.x) - (bb.origin.x + bb.size.width));
            ref->setPosition(Vec2(maxX + d, pos.y));
        }
    }
    if (_sbn != nullptr) _sbn->updateSelectionRect();
}

// @ios 1000dd698
void AlignRefsView::confirmChanges(Ref* /*sender*/) { removeFromSuperview(); }
