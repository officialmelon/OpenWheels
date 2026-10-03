// LevelListView -- UITableView stand-in for the E5 level lists (see LevelListView.h).
#include "LevelListView.h"

#include "UIKitCompat.h"

#include <algorithm>
#include <cstdlib>

USING_NS_CC;

namespace {

// Row widgets carry their index path: tag = row, name = "s<section>".
const int kHeaderTag = -1;

}  // namespace

LevelListView* LevelListView::create(const Size& size, LevelListViewDataSource* dataSource)
{
    LevelListView* view = new (std::nothrow) LevelListView();
    if (view && view->init(size, dataSource))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

LevelListView::LevelListView()
    : _dataSource(nullptr),
      _rowHeight(44.0f),
      _fontSize(17.0f),
      _rowColor(Color3B::WHITE),
      _selectedRowColor(Color3B(217, 217, 217)),   // UITableViewCellSelectionStyleGray
      _textColor(Color3B::BLACK)
{
}

bool LevelListView::init(const Size& size, LevelListViewDataSource* dataSource)
{
    if (!ui::ListView::init())
    {
        return false;
    }
    _dataSource = dataSource;
    float scale = uikit::pointsToDesign();
    setContentSize(Size(size.width * scale, size.height * scale));
    setDirection(ui::ScrollView::Direction::VERTICAL);
    setGravity(ui::ListView::Gravity::LEFT);
    setItemsMargin(0.0f);
    setBounceEnabled(true);
    setScrollBarEnabled(true);
    setClippingEnabled(true);
    setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    setBackGroundColor(_rowColor);
    return true;
}

void LevelListView::setRowHeight(float points) { _rowHeight = points; }
void LevelListView::setFontSize(float points) { _fontSize = points; }

void LevelListView::setColors(const Color3B& row, const Color3B& selectedRow, const Color3B& text)
{
    _rowColor = row;
    _selectedRowColor = selectedRow;
    _textColor = text;
    setBackGroundColor(_rowColor);
}

void LevelListView::reloadData()
{
    removeAllItems();
    _selected = LevelIndexPath();
    if (!_dataSource)
    {
        return;
    }
    float scale = uikit::pointsToDesign();
    float width = getContentSize().width;
    long sections = _dataSource->numberOfSectionsInTableView(this);
    for (long section = 0; section < sections; section++)
    {
        if (ui::Widget* header = _dataSource->tableViewViewForHeaderInSection(this, section))
        {
            header->setTag(kHeaderTag);
            header->setTouchEnabled(false);
            pushBackCustomItem(header);
        }
        long rows = _dataSource->tableViewNumberOfRowsInSection(this, section);
        for (long row = 0; row < rows; row++)
        {
            ui::Layout* cell = ui::Layout::create();
            cell->setContentSize(Size(width, _rowHeight * scale));
            cell->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
            cell->setBackGroundColor(_rowColor);
            cell->setTouchEnabled(true);
            cell->setSwallowTouches(false);
            cell->setTag(static_cast<int>(row));
            cell->setName(StringUtils::format("s%ld", section));
            cell->addTouchEventListener(CC_CALLBACK_2(LevelListView::onRowTouched, this));

            // UITableViewCell textLabel: 15 pt left inset, system font, single line.
            std::string title = _dataSource->tableViewTitleForRow(this, section, row);
            ui::Text* label = uikit::makeLabel(title, "Helvetica", _fontSize);
            label->setTextColor(Color4B(_textColor));
            label->setAnchorPoint(Vec2(0.0f, 0.5f));
            label->setPosition(Vec2(15.0f * scale, _rowHeight * scale * 0.5f));
            cell->addChild(label);

            // separator line
            LayerColor* separator = LayerColor::create(Color4B(200, 199, 204, 255), width, 1.0f);
            separator->setPosition(Vec2(15.0f * scale, 0.0f));
            cell->addChild(separator);

            pushBackCustomItem(cell);
        }
    }
    forceDoLayout();
    jumpToTop();
}

ui::Widget* LevelListView::rowWidget(const LevelIndexPath& indexPath) const
{
    std::string name = StringUtils::format("s%ld", indexPath.section);
    for (ui::Widget* item : const_cast<LevelListView*>(this)->getItems())
    {
        if (item->getTag() == indexPath.row && item->getName() == name)
        {
            return item;
        }
    }
    return nullptr;
}

LevelIndexPath LevelListView::indexPathForSelectedRow() const
{
    return _selected;
}

void LevelListView::selectRow(const LevelIndexPath& indexPath)
{
    _selected = indexPath;
    updateHighlight();
}

void LevelListView::deselect()
{
    _selected = LevelIndexPath();
    updateHighlight();
}

void LevelListView::deleteRow(const LevelIndexPath& indexPath)
{
    // Rebuild from the data source (the row is already gone there); keeps tags consistent.
    float offset = getInnerContainerPosition().y;
    reloadData();
    Vec2 position = getInnerContainerPosition();
    position.y = std::min(0.0f, std::max(offset, getContentSize().height - getInnerContainerSize().height));
    setInnerContainerPosition(position);
}

void LevelListView::updateHighlight()
{
    for (ui::Widget* item : getItems())
    {
        if (item->getTag() == kHeaderTag)
        {
            continue;
        }
        ui::Layout* cell = dynamic_cast<ui::Layout*>(item);
        if (!cell)
        {
            continue;
        }
        bool selected = _selected.isValid() && item->getTag() == _selected.row &&
                        item->getName() == StringUtils::format("s%ld", _selected.section);
        cell->setBackGroundColor(selected ? _selectedRowColor : _rowColor);
    }
}

void LevelListView::onRowTouched(Ref* sender, ui::Widget::TouchEventType type)
{
    if (type != ui::Widget::TouchEventType::ENDED)
    {
        return;
    }
    ui::Widget* cell = static_cast<ui::Widget*>(sender);
    // A drag that scrolled the list is not a tap.
    if (cell->getTouchBeganPosition().distance(cell->getTouchEndPosition()) > 20.0f)
    {
        return;
    }
    long section = std::atol(cell->getName().c_str() + 1);
    LevelIndexPath indexPath(section, cell->getTag());
    selectRow(indexPath);
    if (_dataSource)
    {
        _dataSource->tableViewDidSelectRow(this, indexPath.section, indexPath.row);
    }
}
