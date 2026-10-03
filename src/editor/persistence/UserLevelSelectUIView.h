#pragma once
// iOS: UserLevelSelectUIView : EditorUIView (instanceStart 0x30, instanceSize 0x68). A side panel
// listing chapter 5000 ("your levels") with NEW LEVEL, and PLAY LEVEL / DELETE LEVEL / EDIT LEVEL
// for the selected row.
//
// On iOS nothing instantiates this class (no reference outside its own methods); the live route
// to saved levels is editor menu -> LoadLevelViewController. The port revives it as the user-level
// picker reachable OUTSIDE the editor (docs/editor/E5.md "Entry points"): hosted in uikit::window()
// like any EditorUIView, either over the editor or on its own scene (scene()).
//
// initWithFrame: (w, h = frame; EditorUIView metrics btnWidth 66, btnHeight 30, space 8):
//   the scrollView is replaced by a plain view with the same frame and background colour;
//   table (scroll.x, scroll.y, scroll.w * 0.5, scroll.h), white cells, gray selection;
//   NEW LEVEL blue button, width 2 * btnWidth, left of the close button (x = close.x - (space +
//   2 * btnWidth)), target newLevelBtnPressed:;
//   nameLabel at (winW/2 + space, 2*space, winW/2 - 3*space, btnHeight), clear, text "";
//   descriptionText (read-only) at the same x/width, y = btnHeight + 2*space, height btnHeight.
// displayLevelInfo: creates the three row buttons on the first selection (PLAY LEVEL pink, DELETE
// LEVEL blue, EDIT LEVEL blue; frames from winSize/space/btnHeight, float maths kept) and removes
// them again for nil.
//
// iOS bugs in this dead code and what the port does (each marked "iOS bug" in the .cpp):
//   * displayLevelInfo: valueForKey:@"descriptionText" (no such LevelMO key ->
//     NSUnknownKeyException) -> shows "comments".
//   * editBtnPressed: Session setLevelDataWithManagedObject: then pushScene(EditorLayer.scene),
//     which opens an EMPTY editor (levelMO nil) -> opens EditorLayer::createSceneWithLevelMO(level).
//   * the delete handler never saves -> the port calls LevelStore::save().
//   Kept: playBtnPressed: sets chapterIndex 5000 but no levelIndex beyond what
//   setLevelDataWithManagedObject: sets; canMoveRowAtIndexPath: YES without any reorder UI.

#include "EditorUIView.h"
#include "HWWindowDelegate.h"
#include "LevelListView.h"

#include <string>

class LevelMO;
class LevelTextView;

class UserLevelSelectUIView : public EditorUIView,
                              public LevelListViewDataSource,
                              public HWWindowDelegate
{
public:
    static UserLevelSelectUIView* create(const cocos2d::Rect& frame);
    // Port: a Scene with the main-menu background holding this panel full-window, for the
    // out-of-editor entry point. Closing it (closeView) returns to MainMenu.
    static cocos2d::Scene* scene();

    bool initWithFrame(const cocos2d::Rect& frame) override;                // @ios 100106bb8

    // pushScene(EditorLayer::createScene()), removeFromSuperview.
    void newLevelBtnPressed(cocos2d::Ref* sender);                          // @ios 100107078
    // Alert (tag 0): title "%@ \"%@\"" (DELETE THE LEVEL, name), THIS CANT BE UNDONE,
    // cancel CANCEL / other OK (capitalized).
    void deleteBtnPressed(cocos2d::Ref* sender);                            // @ios 1001070c0
    void editBtnPressed(cocos2d::Ref* sender);                              // @ios 100107220
    // chapterIndex 5000, setLevelDataWithManagedObject:, characterIndex = playable_character,
    // vehicleIndex 0, pushScene(force_character ? gameplay : character select), removeFromSuperview.
    void playBtnPressed(cocos2d::Ref* sender);                              // @ios 1001072ac
    // index 1: remove from levels, LevelStore deleteObject:, deleteRow, displayLevelInfo(nil).
    void alertViewClickedButtonAtIndex(int alertTag, long buttonIndex);     // @ios 100107398
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;
    // Port: releases the reference taken while one of our alerts is on screen.
    void hwWindowWasDismissed(HWWindow* window) override;

    long tableViewNumberOfRowsInSection(cocos2d::ui::ListView* tableView, long section) override;   // @ios 1001074ac
    std::string tableViewTitleForRow(cocos2d::ui::ListView* tableView, long section, long row) override;  // @ios 1001074bc
    bool tableViewCanEditRow(cocos2d::ui::ListView* tableView, long section, long row);             // @ios 100107548  false
    bool tableViewCanMoveRow(cocos2d::ui::ListView* tableView, long section, long row);             // @ios 100107550  true
    void tableViewDidSelectRow(cocos2d::ui::ListView* tableView, long section, long row) override;  // @ios 100107558
    void displayLevelInfo(const LevelIndexPath& indexPath);                 // @ios 100107560

protected:
    UserLevelSelectUIView();
    ~UserLevelSelectUIView() override;                                      // @ios 100107028  dealloc
    LevelMO* selectedLevel();

    cocos2d::Vector<LevelMO*> levels;                   // +0x30  chapter 5000, id_x ascending
    LevelListView* tableView;                           // +0x38  UITableView
    cocos2d::ui::Text* nameLabel;                       // +0x40  UILabel
    LevelTextView* descriptionText;                     // +0x48  UITextView (not editable)
    cocos2d::ui::Button* editLevelBtn;                  // +0x50
    cocos2d::ui::Button* playLevelBtn;                  // +0x58
    cocos2d::ui::Button* deleteBtn;                     // +0x60
};
