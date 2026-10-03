#pragma once
// iOS: AddSpecialItemUIView : EditorUIView (instanceStart 0x30, instanceSize 0x40).
// "ADD ITEMS" panel: a sectioned table of every level item the editor can place. Opened by
// EditorLayer::addItemsBtnPressed: (frame x = winW - notch - w, w = min(winW/2, 284), full
// height). The table replaces the scroll view (same frame) and restores the last scroll
// position from the item catalog (iOS [Settings scrollViewYOffset]); closing stores it back.
//
// Sections / rows come from the iOS Settings item catalog (sectionNames: SHAPES,
// BUILDING BLOCKS, HAZARDS, MOVEMENT, OBJECTIVES, MISCELLANEOUS; sectionedLevelItemIDs; per id
// keyForLevelItem = ref class name, e.g. "IBeamRef", and nameForLevelItem = localized name).
//   header view: label at (5, 2) bold system 12, white, on rgba(0.239, 0.533, 0.737, 0.75),
//                text = section name capitalized
//   cell: image "<key before the first "Ref">_cellIcon.png" (loose PNG in the iOS bundle) and the
//         capitalized item name
// Selecting a row posts "editor_view_item_added" with the item id (int*) - EditorLayer
// (handleMenuTouch:) creates the ref at the screen centre - then closeView(nullptr).
// Localization keys: "ADD ITEMS" (title), "CLOSE", section and item name keys (via catalog).

#include <string>

#include "EditorUIView.h"

class Session;

class AddSpecialItemUIView : public EditorUIView
{
public:
    static AddSpecialItemUIView* create(const cocos2d::Rect& frame);
    bool initWithFrame(const cocos2d::Rect& frame) override;                        // @ios 1000ef930
    void addTableView();                                                            // @ios 1000ef9d8
    void closeView(cocos2d::Ref* sender) override;                                  // @ios 1000efba4

    // UITableViewDataSource / UITableViewDelegate (the ListView is filled from these).
    long numberOfSectionsInTableView(cocos2d::ui::ListView* tableView);                                  // @ios 1000efc20
    long tableViewNumberOfRowsInSection(cocos2d::ui::ListView* tableView, long section);                 // @ios 1000efc44  tableView:numberOfRowsInSection:
    cocos2d::ui::Widget* tableViewCellForRow(cocos2d::ui::ListView* tableView, long section, long row);  // @ios 1000efc7c  tableView:cellForRowAtIndexPath:
    bool tableViewCanEditRow(cocos2d::ui::ListView* tableView, long section, long row);                  // @ios 1000efde0  tableView:canEditRowAtIndexPath: (false)
    void tableViewAccessoryButtonTapped(cocos2d::ui::ListView* tableView, long section, long row);       // @ios 1000efde8  (empty)
    std::string tableViewTitleForHeaderInSection(cocos2d::ui::ListView* tableView, long section);        // @ios 1000efdec
    void tableViewDidSelectRow(cocos2d::ui::ListView* tableView, long section, long row);                // @ios 1000efe24  tableView:didSelectRowAtIndexPath:
    cocos2d::ui::Widget* tableViewViewForHeaderInSection(cocos2d::ui::ListView* tableView, long section); // @ios 1000efeb8

protected:
    AddSpecialItemUIView();

    cocos2d::ui::ListView* _tv;   // +0x30  UITableView
    Session* _session;            // +0x38  Session ([Session sharedSession], used for .settings)
};
