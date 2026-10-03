#pragma once
// iOS: LoadLevelViewController : EditorViewController (instanceSize 0x68), nib
// "LoadLevelViewController". The saved-level browser: presented by -[EditorMenuViewController
// confirmLoadLevel] (after the "lose changes" check) with UIModalPresentationOverFullScreen.
// Two sections, both sorted by id_x: 0 = chapter 5000 "YOUR LEVELS", 1 = chapter 5001
// "IMPORTED LEVELS". Selecting a row shows its name and description; PLAY / EDIT / DELETE / SHARE
// act on the selected row; NEW LEVEL starts an empty editor; CANCEL closes.
//
// Layout (compiled nib, points, top-left origin; root fills the window):
//   root background grey 0.706; panel (44, 0, 808, 354) grey 0.8 containing:
//     table           (0, 0, 550, 354)    white
//     levelNameLabel  (318, 8, 242, 21)   17pt, white
//     description     (558, 131, 242, 119) background 0.697, 14pt, text white
//     PLAY            (558, 258, 242, 40)  bold 19pt     skin style 0
//     DELETE          (558, 306, 75, 40)   bold 15pt     skin style 1
//     EDIT            (641, 306, 75, 40)                 skin style 1
//     SHARE           (724, 306, 76, 40)                 skin style 1
//   root: menuNameLabel (20, 279, 242, 21) 17pt white; NEW LEVEL (600, 364, 117, 40) style 2;
//         CANCEL (725, 364, 117, 40) style 0.
//   Section header (tableView:viewForHeaderInSection:): view of the table's width, label at (5, 2)
//   bold system 12pt white on clear; view background rgba(0.239, 0.533, 0.737, 0.75).
//   The nib frames are for an 896x414 root; autolayout pins the panel/buttons to the edges
//   (resolved like E4 does for EditorMenuViewController.nib).
//
// Alerts: HWWindow; window tag = UIAlertView tag; UIAlertView button index 0 (cancel button) =
// HWWindow cancel (tag 0), index 1 (first other button) = HWWindow confirm (tag 1).
//
// iOS quirks kept: confirmEditLevel and shareBtnPressed: read _levels (section 0) whatever the
// selected section (both buttons are disabled for section 1); play/edit push their scene onto the
// Director stack over the editor and then dismiss the root-presented menu.

#include "EditorViewController.h"
#include "HWWindowDelegate.h"
#include "LevelListView.h"

#include <string>

class LevelMO;
class LevelTextView;

class LoadLevelViewController : public EditorViewController,
                                public LevelListViewDataSource,
                                public HWWindowDelegate
{
public:
    // [[LoadLevelViewController alloc] initWithNibName:@"LoadLevelViewController" bundle:main]
    static LoadLevelViewController* create();
    bool initWithNibName(const std::string& nibName) override;              // @ios 100057964

    // ---- actions ----
    // Session clearLevelData, close, replaceScene(EditorLayer::createScene()).
    void createNewLevelBtnPressed(cocos2d::Ref* sender);                    // @ios 100057998
    void cancelBtnPressed(cocos2d::Ref* sender);                            // @ios 1000579f0  close
    // Session: chapterIndex 5000 (section 0) / 5001 (section 1), levelIndex = id_x,
    // setLevelDataWithManagedObject:, characterIndex = playable_character, vehicleIndex 0;
    // pushScene(force_character ? gameplay : character select) (LevelSession::playLevel);
    // dismissRootPresented.
    void playBtnPressed(cocos2d::Ref* sender);                              // @ios 1000579f4
    // levelTimes.count != 0 -> alert tag 1: "Hey" / "Editing this level will delete the best
    // times. Continue?" / cancel "No" / other "Yes" (literal English on iOS); else confirmEditLevel.
    void editBtnPressed(cocos2d::Ref* sender);                              // @ios 100057b2c
    // pushScene(EditorLayer::createSceneWithLevelMO(_levels[selected row])), dismissRootPresented.
    void confirmEditLevel();                                                // @ios 100057c10
    // Alert tag 0: title "%@ \"%@\"" (DELETE THE LEVEL, name), message THIS CANT BE UNDONE,
    // cancel CANCEL / other OK (both capitalized).
    void deleteBtnPressed(cocos2d::Ref* sender);                            // @ios 100057c98
    // ShareAction::shareLevelDataFile(data, playable_character, force_character != 0, name,
    // creator "", comments, this).
    void shareBtnPressed(cocos2d::Ref* sender);                             // @ios 100057e1c

    // ---- UIViewController ----
    // menuNameLabel = YOUR LEVELS (capitalized); clears name/description; skins and titles the
    // buttons (CANCEL 0, NEW LEVEL 2, PLAY 0, EDIT 1, DELETE 1, SHARE 1; titles capitalized);
    // fetches chapter 5000 -> _levels and 5001 -> _importedLevels sorted by id_x;
    // displayLevelInfo(nil).
    void viewWillAppear(bool animated) override;                            // @ios 100057ef0
    // super, contentSizeForViewInPopover 480x320 (unused), port: builds the nib layout.
    void viewDidLoad() override;                                            // @ios 1000583c0
    void didReceiveMemoryWarning() override;                                // @ios 100058418
    // [self.presentingViewController dismissViewControllerAnimated:YES completion:nil]
    void close();                                                           // @ios 10005844c

    // ---- UIAlertViewDelegate: alertView:clickedButtonAtIndex: ----
    // tag 1, index 1 -> confirmEditLevel. tag 0, index 1 -> remove the selected level from its
    // array, LevelStore deleteObject + save, deleteRow, displayLevelInfo(nil).
    void alertViewClickedButtonAtIndex(int alertTag, long buttonIndex);     // @ios 100058568
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;
    // Port: releases the reference taken while one of our alerts is on screen.
    void hwWindowWasDismissed(HWWindow* window) override;

    // ---- UITableViewDataSource / UITableViewDelegate ----
    long numberOfSectionsInTableView(cocos2d::ui::ListView* tableView) override;                     // @ios 100058708  2
    long tableViewNumberOfRowsInSection(cocos2d::ui::ListView* tableView, long section) override;   // @ios 100058710
    std::string tableViewTitleForRow(cocos2d::ui::ListView* tableView, long section, long row) override;  // @ios 100058734  name
    bool tableViewCanEditRow(cocos2d::ui::ListView* tableView, long section, long row);             // @ios 1000587d4  false
    void tableViewDidSelectRow(cocos2d::ui::ListView* tableView, long section, long row) override;  // @ios 1000587dc
    // nil: name "--", description SELECT A LEVEL, SHARE/PLAY/EDIT/DELETE hidden. Otherwise the
    // name, comments (NO LEVEL DESCRIPTION when nil or ""), the four buttons shown, EDIT and SHARE
    // enabled only for section 0. Description text colour white in both cases.
    void displayLevelInfo(const LevelIndexPath& indexPath);                 // @ios 1000587e4
    cocos2d::ui::Widget* tableViewViewForHeaderInSection(cocos2d::ui::ListView* tableView,
                                                         long section) override;  // @ios 100058a04

    // ---- properties (IBOutlets) ----
    cocos2d::ui::Text* levelNameLabel() const;                              // @ios 100058b68
    void setLevelNameLabel(cocos2d::ui::Text* label);                       // @ios 100058b78
    LevelTextView* descriptionTextView() const;                             // @ios 100058b84
    void setDescriptionTextView(LevelTextView* textView);                   // @ios 100058b94
    cocos2d::ui::Text* menuNameLabel() const;                               // @ios 100058ba0
    void setMenuNameLabel(cocos2d::ui::Text* label);                        // @ios 100058bb0
    LevelListView* tableView() const;                                       // @ios 100058bbc
    void setTableView(LevelListView* tableView);                            // @ios 100058bcc
    cocos2d::ui::Button* cancelBtn() const;                                 // @ios 100058bd8
    void setCancelBtn(cocos2d::ui::Button* button);                         // @ios 100058be8
    cocos2d::ui::Button* playBtn() const;                                   // @ios 100058bf4
    void setPlayBtn(cocos2d::ui::Button* button);                           // @ios 100058c04
    cocos2d::ui::Button* editBtn() const;                                   // @ios 100058c10
    void setEditBtn(cocos2d::ui::Button* button);                           // @ios 100058c20
    cocos2d::ui::Button* deleteBtn() const;                                 // @ios 100058c2c
    void setDeleteBtn(cocos2d::ui::Button* button);                         // @ios 100058c3c
    cocos2d::ui::Button* shareLevelBtn() const;                             // @ios 100058c48
    void setShareLevelBtn(cocos2d::ui::Button* button);                     // @ios 100058c58
    cocos2d::ui::Button* createNewLevelBtn() const;                         // @ios 100058c64
    void setCreateNewLevelBtn(cocos2d::ui::Button* button);                 // @ios 100058c74

protected:
    LoadLevelViewController();
    ~LoadLevelViewController() override;                                    // @ios 100058468  dealloc

    LevelMO* levelAtIndexPath(const LevelIndexPath& indexPath);
    void showAlert(int tag, const std::string& title, const std::string& message,
                   const std::string& cancelTitle, const std::string& otherTitle);

    cocos2d::Vector<LevelMO*> _levels;                  // +0x08  chapter 5000, id_x ascending
    cocos2d::Vector<LevelMO*> _importedLevels;          // +0x10  chapter 5001, id_x ascending
    cocos2d::ui::Text* _levelNameLabel;                 // +0x18  UILabel
    LevelTextView* _descriptionTextView;                // +0x20  UITextView
    cocos2d::ui::Text* _menuNameLabel;                  // +0x28  UILabel
    LevelListView* _tableView;                          // +0x30  UITableView
    cocos2d::ui::Button* _cancelBtn;                    // +0x38
    cocos2d::ui::Button* _playBtn;                      // +0x40
    cocos2d::ui::Button* _editBtn;                      // +0x48
    cocos2d::ui::Button* _deleteBtn;                    // +0x50
    cocos2d::ui::Button* _shareLevelBtn;                // +0x58
    cocos2d::ui::Button* _createNewLevelBtn;            // +0x60
};
