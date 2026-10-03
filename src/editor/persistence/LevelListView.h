#pragma once
// LevelListView: the UITableView of LoadLevelViewController and UserLevelSelectUIView (port class,
// no iOS counterpart). A ui::ListView (E4 maps UITableView -> ui::ListView) that rebuilds its rows
// from a data source with the UITableView protocol shape used elsewhere in the editor UI
// (AddSpecialItemUIView, EditorMenuTableViewController), plus the bits of UITableView state the
// level screens need: single selection (indexPathForSelectedRow), row deletion and section
// header views.
//
//   numberOfSectionsInTableView: / tableView:numberOfRowsInSection:  -> data source
//   tableView:cellForRowAtIndexPath: (cell.textLabel.text = name,
//       selectionStyle 2 = gray highlight)                             -> tableViewTitleForRow
//   tableView:viewForHeaderInSection:                                  -> tableViewViewForHeaderInSection
//   tableView:didSelectRowAtIndexPath:                                 -> tableViewDidSelectRow
//   deleteRowsAtIndexPaths:withRowAnimation:                           -> deleteRow
// Row/section indices are long like the other editor tables.

#include "cocos2d.h"
#include "ui/CocosGUI.h"

#include <string>

// NSIndexPath (section, row); row < 0 stands for nil.
struct LevelIndexPath
{
    long section;
    long row;
    LevelIndexPath() : section(0), row(-1) {}
    LevelIndexPath(long s, long r) : section(s), row(r) {}
    bool isValid() const { return row >= 0; }
};

class LevelListViewDataSource
{
public:
    virtual ~LevelListViewDataSource() {}
    virtual long numberOfSectionsInTableView(cocos2d::ui::ListView* tableView) { return 1; }
    virtual long tableViewNumberOfRowsInSection(cocos2d::ui::ListView* tableView, long section) = 0;
    virtual std::string tableViewTitleForRow(cocos2d::ui::ListView* tableView, long section, long row) = 0;
    // nullptr = no header.
    virtual cocos2d::ui::Widget* tableViewViewForHeaderInSection(cocos2d::ui::ListView* tableView,
                                                                 long section) { return nullptr; }
    virtual void tableViewDidSelectRow(cocos2d::ui::ListView* tableView, long section, long row) {}
};

class LevelListView : public cocos2d::ui::ListView
{
public:
    // size in points (uikit frame size); fontSize in points.
    static LevelListView* create(const cocos2d::Size& size, LevelListViewDataSource* dataSource);
    bool init(const cocos2d::Size& size, LevelListViewDataSource* dataSource);

    void reloadData();                                    // rebuild every row; clears the selection
    LevelIndexPath indexPathForSelectedRow() const;
    void selectRow(const LevelIndexPath& indexPath);      // highlight only (no delegate call)
    void deselect();
    // The data source must already have dropped the row. Clears the selection.
    void deleteRow(const LevelIndexPath& indexPath);

    void setRowHeight(float points);                      // default 44 (UITableView default)
    void setFontSize(float points);                       // default 17 (UITableViewCell textLabel)
    void setColors(const cocos2d::Color3B& row, const cocos2d::Color3B& selectedRow,
                   const cocos2d::Color3B& text);         // white / gray / black

protected:
    LevelListView();
    void onRowTouched(cocos2d::Ref* sender, cocos2d::ui::Widget::TouchEventType type);
    void updateHighlight();
    cocos2d::ui::Widget* rowWidget(const LevelIndexPath& indexPath) const;

    LevelListViewDataSource* _dataSource;   // weak (the owning screen)
    LevelIndexPath _selected;
    float _rowHeight;
    float _fontSize;
    cocos2d::Color3B _rowColor;
    cocos2d::Color3B _selectedRowColor;
    cocos2d::Color3B _textColor;
};
