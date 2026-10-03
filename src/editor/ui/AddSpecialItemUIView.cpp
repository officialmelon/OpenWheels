#include "AddSpecialItemUIView.h"

#include <algorithm>

#include "EditorSettings.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace {
// UITableView metrics used by the port. RE-TODO(@1000efc7c): the iOS cell keeps the icon's
// natural size (85-170 px images, scale 1); the port fits it into a 50 pt box in a 60 pt row.
const float kRowHeight = 60.0f;
const float kIconBox = 50.0f;
const float kHeaderHeight = 18.0f;
}  // namespace

AddSpecialItemUIView::AddSpecialItemUIView() : _tv(nullptr), _session(nullptr) {}

AddSpecialItemUIView* AddSpecialItemUIView::create(const Rect& frame)
{
    AddSpecialItemUIView* view = new (std::nothrow) AddSpecialItemUIView();
    if (view && view->initWithFrame(frame))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

// @ios 1000ef930
bool AddSpecialItemUIView::initWithFrame(const Rect& frame)
{
    if (!EditorUIView::initWithFrame(frame)) return false;
    _session = nullptr;  // [Session sharedSession]; the catalog lives in EditorSettings (E1)
    _menuLabel->setString(uikit::capitalizedString(Localization::get("ADD ITEMS")));
    addTableView();
    return true;
}

// @ios 1000ef9d8
void AddSpecialItemUIView::addTableView()
{
    EditorSettings* settings = EditorSettings::getInstance();
    // (iOS copies settings.levelItems into an unused NSMutableArray here.)
    const Rect tableFrame = _scrollView->frame();
    const float k = uikit::pointsToDesign();
    _tv = ui::ListView::create();
    _tv->setDirection(ui::ScrollView::Direction::VERTICAL);
    _tv->setBounceEnabled(true);
    _tv->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    _tv->setBackGroundColor(Color3B::WHITE);
    _tv->setItemsMargin(0.0f);
    _tv->setScrollBarEnabled(true);
    _scrollView->removeFromSuperview();
    _scrollView = nullptr;
    addSubview(_tv, tableFrame);

    const long sections = numberOfSectionsInTableView(_tv);
    for (long section = 0; section < sections; ++section)
    {
        _tv->pushBackCustomItem(tableViewViewForHeaderInSection(_tv, section));
        const long rows = tableViewNumberOfRowsInSection(_tv, section);
        for (long row = 0; row < rows; ++row)
        {
            ui::Widget* cell = tableViewCellForRow(_tv, section, row);
            cell->setTouchEnabled(true);
            cell->setSwallowTouches(false);
            cell->addClickEventListener([this, section, row](Ref*) { tableViewDidSelectRow(_tv, section, row); });
            _tv->pushBackCustomItem(cell);
        }
    }
    _tv->forceDoLayout();
    // [tv setContentOffset:CGPointMake(0, settings.scrollViewYOffset)]
    const float innerH = _tv->getInnerContainerSize().height;
    const float viewH = _tv->getContentSize().height;
    const float y = std::min(0.0f, std::max(viewH - innerH, (viewH - innerH) + settings->scrollViewYOffset() * k));
    _tv->setInnerContainerPosition(Vec2(0.0f, y));
}

// @ios 1000efba4
void AddSpecialItemUIView::closeView(Ref* sender)
{
    const float k = uikit::pointsToDesign();
    const float innerH = _tv->getInnerContainerSize().height;
    const float viewH = _tv->getContentSize().height;
    const float offsetY = (_tv->getInnerContainerPosition().y - (viewH - innerH)) / k;
    EditorSettings::getInstance()->setScrollViewYOffset(offsetY);
    EditorUIView::closeView(sender);
}

// @ios 1000efc20
long AddSpecialItemUIView::numberOfSectionsInTableView(ui::ListView* /*tableView*/)
{
    return static_cast<long>(EditorSettings::getInstance()->sectionedLevelItemIDs().size());
}

// @ios 1000efc44
long AddSpecialItemUIView::tableViewNumberOfRowsInSection(ui::ListView* /*tableView*/, long section)
{
    return static_cast<long>(EditorSettings::getInstance()->sectionedLevelItemIDs().at(section).size());
}

// @ios 1000efc7c
ui::Widget* AddSpecialItemUIView::tableViewCellForRow(ui::ListView* tableView, long section, long row)
{
    EditorSettings* settings = EditorSettings::getInstance();
    const int itemID = settings->sectionedLevelItemIDs().at(section).at(row);
    const float k = uikit::pointsToDesign();
    const float width = tableView->getContentSize().width;

    ui::Layout* cell = ui::Layout::create();
    cell->setContentSize(Size(width, kRowHeight * k));
    cell->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    cell->setBackGroundColor(Color3B::WHITE);

    // [[keyForLevelItem componentsSeparatedByString:@"Ref"] objectAtIndex:0] + "_cellIcon.png"
    const std::string key = settings->keyForLevelItem(static_cast<unsigned int>(itemID));
    const std::string base = key.substr(0, key.find("Ref"));
    float scale = 1.0f;
    const std::string frameName = uikit::imageNamed(base + "_cellIcon.png", &scale);
    float textX = 15.0f;
    if (!frameName.empty())
    {
        Sprite* icon = Sprite::createWithSpriteFrameName(frameName);
        const Size px = icon->getContentSize();
        const float points = std::max(px.width, px.height) / scale;
        const float fit = points > kIconBox ? kIconBox / points : 1.0f;
        icon->setScale(k / scale * fit);
        icon->setPosition(Vec2((15.0f + kIconBox * 0.5f) * k, kRowHeight * 0.5f * k));
        cell->addChild(icon);
        textX = 15.0f + kIconBox + 15.0f;
    }
    ui::Text* label = uikit::makeLabel(uikit::capitalizedString(settings->nameForLevelItem(static_cast<unsigned int>(itemID))),
                                       false, 17.0f, 0);
    label->setContentSize(Size(width - textX * k, kRowHeight * k));
    label->setAnchorPoint(Vec2::ZERO);
    label->setPosition(Vec2(textX * k, 0.0f));
    cell->addChild(label);
    // separator
    DrawNode* line = DrawNode::create();
    line->drawSolidRect(Vec2(textX * k, 0.0f), Vec2(width, 0.5f * k), Color4F(0.78f, 0.78f, 0.8f, 1.0f));
    cell->addChild(line);
    return cell;
}

// @ios 1000efde0
bool AddSpecialItemUIView::tableViewCanEditRow(ui::ListView* /*tableView*/, long /*section*/, long /*row*/) { return false; }

// @ios 1000efde8
void AddSpecialItemUIView::tableViewAccessoryButtonTapped(ui::ListView* /*tableView*/, long /*section*/, long /*row*/) {}

// @ios 1000efdec
std::string AddSpecialItemUIView::tableViewTitleForHeaderInSection(ui::ListView* /*tableView*/, long section)
{
    return uikit::capitalizedString(EditorSettings::getInstance()->sectionNames().at(section));
}

// @ios 1000efe24
void AddSpecialItemUIView::tableViewDidSelectRow(ui::ListView* /*tableView*/, long section, long row)
{
    int itemID = EditorSettings::getInstance()->sectionedLevelItemIDs().at(section).at(row);
    RefPtr<AddSpecialItemUIView> keep(this);
    uikit::NotificationCenter::postNotification(uikit::notification::kEditorViewItemAdded, &itemID);
    closeView(nullptr);
}

// @ios 1000efeb8
ui::Widget* AddSpecialItemUIView::tableViewViewForHeaderInSection(ui::ListView* tableView, long section)
{
    const float k = uikit::pointsToDesign();
    const float width = tableView->getContentSize().width;
    ui::Layout* header = ui::Layout::create();
    header->setContentSize(Size(width, kHeaderHeight * k));
    header->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    const Color4B bg = uikit::color(0.23921568627450981, 0.5333333333333333, 0.7372549019607844, 0.75);
    header->setBackGroundColor(Color3B(bg.r, bg.g, bg.b));
    header->setBackGroundColorOpacity(bg.a);
    ui::Text* label = uikit::makeLabel(tableViewTitleForHeaderInSection(tableView, section), true, 12.0f, 0);
    label->setTextColor(uikit::whiteColor());
    label->setContentSize(Size(width - 5.0f * k, kHeaderHeight * k));
    label->setAnchorPoint(Vec2::ZERO);
    label->setPosition(Vec2(5.0f * k, -2.0f * k));  // frame (5, 2, w, 18), y down
    header->addChild(label);
    return header;
}
