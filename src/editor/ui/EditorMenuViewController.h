#pragma once
// iOS: EditorMenuViewController : EditorViewController (instanceStart 0x8, instanceSize 0xa8).
// The editor's main menu, opened by EditorLayer::showMainMenuBtnPressed: (E3) from
// EditorMenuViewController.nib with the layer's LevelMO; EditorLayer observes
// "main_menu_closed" (-> removeMainMenu: dismisses it) and "save_level_done".
// 13 of its methods (viewDidLoad .. confirmLoadLevel, 10010bbe0-10010c80c) are missing from the
// Ghidra export; they were recovered from the arm64 code (docs/editor/E4.md).
//
// Presentation (port decision: iPad): EditorLayer wraps the menu in an EditorPopoverController
// (480 x 320 points, contentSizeForViewInPopover set in viewDidLoad) presented from the menu
// button and calls setPopoverController(popover); every "if (popoverController)" branch takes
// the iPad side. Without a popover (presentOnRoot) the iPhone branches run.
//
// Layout (nib "objects-11.0+", points; root view = the 480 x 320 popover content, autolayout resolved):
//   container view: top 0, leading = safe area, trailing = safe area - 5, bottom = root - 60,
//                   background rgba(0.8, 0.8, 0.8, 1); root background rgba(0.706, 0.706, 0.706, 1)
//   left column (fixed, x 9, 160 x 40): saveBtn y 8, createNewLevelBtn y 56, loadBtn y 104,
//                   shareBtn y 152, mainMenuBtn y 201 (h 38)
//   right column (trailing - 12, 160 x 40): lockBtn y 8, unlockBtn y 56;
//                   rotateSwitchLabel (75 x 31) + rotateSwitch, 12 below unlockBtn, switch at
//                   trailing - 20; pasteInPlaceLabel + pasteInPlaceSwitch 12-13 below;
//                   centerToCharBtn 18 below pasteInPlaceLabel
//   bottom bar: menuNameLabel (leading + 10, bottom - 19, 242 x 21, bold 17 white);
//               closeBtn (trailing, centred on the label, 160 x 39)
//   snapRotLabel / snapRotSwitch exist in the nib but are not in the view hierarchy (invisible).
// viewWillAppear: tracker category "editor_level_menu" (dropped on PC); title "EDITOR MENU";
// buttons skinned style 1 (blue) except mainMenuBtn / closeBtn (style 0, pink); titles
// (capitalized) "SAVE LEVEL", "LOAD LEVEL", "SHARE LEVEL", "NEW LEVEL", "MAIN MENU",
// "CLOSE MENU", "LOCK SELECTION", "CENTER TO CHARACTER", unlock = "UNLOCK ALL" or
// "%@ %i %@" (UNLOCK, lockedRefs count, ITEMS) when items are locked; switch labels
// "ROTATE ITEMS\nSEPARATELY", "SNAP TO\nANGLE", "PASTE IN\nPLACE" (formatLabel:); switches
// show rotateItemsIndependently / snapToAngle / pasteInPlace (customizeSwitch:);
// lockBtn enabled iff exactly 1 selected ref that is not a CharacterRef, or 2+ selected refs;
// unlockBtn enabled iff lockedRefs count != 0.
// Alert ("lose changes"): HWWindow, title "Hey", message "Lose changes?", confirm "Yes"
// (iOS button index 0, HWWindow tag 1), cancel "No"; window tag = alert type
// (0 new level, 1 exit editor, 2 load level).

#include <string>

#include "EditorViewController.h"
#include "HWWindowDelegate.h"

class EditorLayer;
class LevelMO;
class LoadLevelViewController;
class HWWindow;
class EditorPopoverController;

class EditorMenuViewController : public EditorViewController, public HWWindowDelegate
{
public:
    static EditorMenuViewController* create(const std::string& nibName, LevelMO* levelMO);
    static EditorMenuViewController* create(LevelMO* levelMO);  // nib "EditorMenuViewController"
    // initWithNibName:bundle:levelMO:
    virtual bool initWithNibName(const std::string& nibName, LevelMO* levelMO);      // @ios 10010bbe0
    // contentSizeForViewInPopover 480 x 320 (+ port: builds the nib layout).
    void viewDidLoad() override;                                                    // @ios 10010bc40
    void viewWillAppear(bool animated) override;                                    // @ios 10010bc98
    void viewWillDisappear(bool animated) override;                                 // @ios 10010c2a8
    void didReceiveMemoryWarning() override;                                        // @ios 10010c2d4
    void saveLevelCancel(void* notificationObject);                                 // @ios 10010c474
    void saveLevelDone(void* notificationObject);                                   // @ios 10010c4c8
    void formatLabel(cocos2d::ui::Text* label);                                     // @ios 10010c51c
    void customizeSwitch(uikit::Switch* aSwitch);                           // @ios 10010c5cc
    void saveBtnPressed(cocos2d::Ref* sender);                                      // @ios 10010c61c
    void loadBtnPressed(cocos2d::Ref* sender);                                      // @ios 10010c708
    void confirmLoadLevel();                                                        // @ios 10010c748  (+ completion block @ios 10010c80c)

    // Port: builds the nib's view hierarchy (frames from the 480 x 320 popover layout).
    void loadNibLayout();
    void viewDidAppear(bool animated) override;                                     // @ios 10010c814
    void showLoadLevelVCComplete();                                                 // @ios 10010c82c
    void loadLevelVCClosed(void* notificationObject);                               // @ios 10010c830
    void createNewLevelBtnPressed(cocos2d::Ref* sender);                            // @ios 10010c89c
    void confirmNewLevel();                                                         // @ios 10010c8dc
    void exitEditorBtnPressed(cocos2d::Ref* sender);                                // @ios 10010c940
    void confirmExitEditor();                                                       // @ios 10010c980
    void closeMenuBtnPressed(cocos2d::Ref* sender);                                 // @ios 10010c9cc
    void lockBtnPressed(cocos2d::Ref* sender);                                      // @ios 10010ca28
    void unlockBtnPressed(cocos2d::Ref* sender);                                    // @ios 10010ca5c
    void rotateSwitchValueChanged(cocos2d::Ref* sender);                            // @ios 10010ca90
    void pasteInPlaceBtnPressed(cocos2d::Ref* sender);                              // @ios 10010cac4
    void shareBtnPressed(cocos2d::Ref* sender);                                     // @ios 10010caf8
    void centerToCharBtnPressed(cocos2d::Ref* sender);                              // @ios 10010cbc4
    void showLoseChangesAlertViewWithAlertType(int alertType);                      // @ios 10010cbf0
    // alertView:clickedButtonAtIndex: (buttonIndex 0 = "Yes")
    void alertView(HWWindow* alertView, long buttonIndex);                          // @ios 10010cc68

    // HWWindowDelegate -> alertView(window, tag == 1 ? 0 : 1)
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;

    // Properties (iOS outlets / assign properties).
    EditorLayer* editorLayer() const;                                               // @ios 10010cce0
    void setEditorLayer(EditorLayer* editorLayer);                                  // @ios 10010ccf0
    EditorPopoverController* popoverController() const;                             // @ios 10010cd00
    void setPopoverController(EditorPopoverController* popoverController);          // @ios 10010cd10
    cocos2d::ui::Button* saveBtn() const;                                           // @ios 10010cd20
    void setSaveBtn(cocos2d::ui::Button* saveBtn);                                  // @ios 10010cd30
    cocos2d::ui::Button* loadBtn() const;                                           // @ios 10010cd3c
    void setLoadBtn(cocos2d::ui::Button* loadBtn);                                  // @ios 10010cd4c
    cocos2d::ui::Button* createNewLevelBtn() const;                                 // @ios 10010cd58
    void setCreateNewLevelBtn(cocos2d::ui::Button* createNewLevelBtn);              // @ios 10010cd68
    cocos2d::ui::Button* closeBtn() const;                                          // @ios 10010cd74
    void setCloseBtn(cocos2d::ui::Button* closeBtn);                                // @ios 10010cd84
    cocos2d::ui::Button* lockBtn() const;                                           // @ios 10010cd90
    void setLockBtn(cocos2d::ui::Button* lockBtn);                                  // @ios 10010cda0
    cocos2d::ui::Button* unlockBtn() const;                                         // @ios 10010cdac
    void setUnlockBtn(cocos2d::ui::Button* unlockBtn);                              // @ios 10010cdbc
    cocos2d::ui::Button* mainMenuBtn() const;                                       // @ios 10010cdc8
    void setMainMenuBtn(cocos2d::ui::Button* mainMenuBtn);                          // @ios 10010cdd8
    cocos2d::ui::Button* shareBtn() const;                                          // @ios 10010cde4
    void setShareBtn(cocos2d::ui::Button* shareBtn);                                // @ios 10010cdf4
    uikit::Switch* rotateSwitch() const;                                    // @ios 10010ce00
    void setRotateSwitch(uikit::Switch* rotateSwitch);                      // @ios 10010ce10
    cocos2d::ui::Text* rotateSwitchLabel() const;                                   // @ios 10010ce1c
    void setRotateSwitchLabel(cocos2d::ui::Text* rotateSwitchLabel);                // @ios 10010ce2c
    uikit::Switch* pasteInPlaceSwitch() const;                              // @ios 10010ce38
    void setPasteInPlaceSwitch(uikit::Switch* pasteInPlaceSwitch);          // @ios 10010ce48
    cocos2d::ui::Text* pasteInPlaceLabel() const;                                   // @ios 10010ce54
    void setPasteInPlaceLabel(cocos2d::ui::Text* pasteInPlaceLabel);                // @ios 10010ce64
    cocos2d::ui::Text* snapRotLabel() const;                                        // @ios 10010ce70
    void setSnapRotLabel(cocos2d::ui::Text* snapRotLabel);                          // @ios 10010ce80
    uikit::Switch* snapRotSwitch() const;                                   // @ios 10010ce8c
    void setSnapRotSwitch(uikit::Switch* snapRotSwitch);                    // @ios 10010ce9c
    cocos2d::ui::Button* centerToCharBtn() const;                                   // @ios 10010cea8
    void setCenterToCharBtn(cocos2d::ui::Button* centerToCharBtn);                  // @ios 10010ceb8
    cocos2d::ui::Text* menuNameLabel() const;                                       // @ios 10010cec4
    void setMenuNameLabel(cocos2d::ui::Text* menuNameLabel);                        // @ios 10010ced4

protected:
    EditorMenuViewController();
    ~EditorMenuViewController() override;                                           // @ios 10010c308  dealloc
    using EditorViewController::initWithNibName;

    LevelMO* _levelMO;                                        // +0x08  LevelMO (retained)
    LoadLevelViewController* _loadLevelViewController;        // +0x10
    EditorLayer* _editorLayer;                                // +0x18  iOS `editorLayer` (assign)
    EditorPopoverController* _popoverController;              // +0x20  UIPopoverController (assign)
    cocos2d::ui::Button* _saveBtn;                            // +0x28
    cocos2d::ui::Button* _loadBtn;                            // +0x30
    cocos2d::ui::Button* _createNewLevelBtn;                  // +0x38
    cocos2d::ui::Button* _closeBtn;                           // +0x40
    cocos2d::ui::Button* _lockBtn;                            // +0x48
    cocos2d::ui::Button* _unlockBtn;                          // +0x50
    cocos2d::ui::Button* _mainMenuBtn;                        // +0x58
    cocos2d::ui::Button* _shareBtn;                           // +0x60
    uikit::Switch* _rotateSwitch;                     // +0x68  UISwitch
    cocos2d::ui::Text* _rotateSwitchLabel;                    // +0x70  UILabel
    uikit::Switch* _pasteInPlaceSwitch;               // +0x78
    cocos2d::ui::Text* _pasteInPlaceLabel;                    // +0x80
    cocos2d::ui::Text* _snapRotLabel;                         // +0x88
    uikit::Switch* _snapRotSwitch;                    // +0x90
    cocos2d::ui::Button* _centerToCharBtn;                    // +0x98
    cocos2d::ui::Text* _menuNameLabel;                        // +0xa0

    // NET (PC addition): "Send Nearby" beside SHARE LEVEL (src/net/NearbySendUIView.h).
    cocos2d::ui::Button* _sendNearbyBtn = nullptr;
    void sendNearbyBtnPressed(cocos2d::Ref* sender);
};
