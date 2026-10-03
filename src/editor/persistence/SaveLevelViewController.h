#pragma once
// SaveLevelViewController (iOS 1.2.7, SaveLevelViewController.nib): the save dialog opened by
// -[EditorMenuViewController saveBtnPressed:] with the menu's _levelMO (the EditorLayer's levelMO
// when the menu opened) and editorLayer set; the menu also observes "save_level_cancel" /
// "save_level_done" (both handlers just remove the observers). EditorLayer observes
// "save_level_done" (-[EditorLayer saveLevelComplete:]: hasSaved = YES, dismiss the menu).
//
// Flow:
//   viewWillAppear: name/description fields empty, "<n> chars left" counters (max 20 / 300).
//     If neither _levelMO nor editorLayer.levelMO.name exists: one button SAVE (saveBtnPressed:)
//     and the SAVE OVER button is removed. Otherwise SAVE NEW + SAVE OVER, and the fields are
//     pre-filled from editorLayer.levelMO (name, comments) -- not from _levelMO.
//   SAVE / SAVE NEW (saveBtnPressed:): name trimmed of whitespace/newlines must be non-empty
//     ("HEY" / "ENTER A NAME" / "OK", no delegate) and the untrimmed name must differ from every
//     level name in chapter 5000 ("HEY" / "ENTER UNIQUE NAME" / "OK"). Then insert a LevelMO:
//     data = editorLayer.levelData, id_x = last id_x + 1, name, comments, force_character,
//     playable_character = editorLayer.characterIndex, add to chapter 5000,
//     editorLayer.levelMO = it, Session setLevelDataWithManagedObject:, tracker
//     submitAction "save_level" label "count_<levels in chapter>", save. Failure: "Error" / error
//     text / "Ok"; success: showSaveConfirmAlert.
//   SAVE OVER (saveOverBtn:): editorLayer.levelMO (port: nothing happens if it is null -- a nil
//     message on iOS -- but the save and alerts still run) gets comments, name (untrimmed, no checks),
//     data, force_character, playable_character; all its levelTimes are deleted; save; then the
//     same error / success alerts.
//   showSaveConfirmAlert: "SUCCESS" / "LEVEL SAVED" / "OK", tag 2.
//   CANCEL: both fields blank after trimming -> dismiss silently; otherwise alert tag 1
//     "Do Not Save?" / "Cancel the save?" / "NO" / "YES".
//   alertView:clickedButtonAtIndex: tag 2 (any button) or tag 1 button 1 -> post
//     "save_level_done" (uikit::NotificationCenter, object nil) and
//     [self.presentingViewController dismissViewControllerAnimated:]. (So confirming the cancel
//     also marks the editor as saved -- iOS behaviour, kept.)
//   Text limits (textView:shouldChangeTextInRange:replacementText:): "\n" in the name moves focus
//     to the description, "\n" in the description ends editing; both are rejected. Otherwise the
//     change is allowed iff (length - range.length + replacement.length) <= max.
//
// Nib layout (points): root 667x375 grey 0.706; panel (0,0,667,315) grey 0.8 with name label
// (20,10,101,21), name chars-left (539,10,108,21, right-aligned), name text view (20,39,627,32,
// white, 14pt), description label (20,103,144,21), description chars-left (503,103,144,21,
// right-aligned), description text view (20,132,627,77, white, 14pt); on the root SAVE
// (10,325,120,39), SAVE OVER (138,325,120,39), CANCEL (538,325,120,40) (bold 15pt; skin styles:
// CANCEL 0, SAVE 1, SAVE OVER 1). Labels 17pt white; captions are capitalized localization keys
// (LEVEL NAME, LEVEL DESCRIPTION, CANCEL, SAVE / SAVE NEW, SAVE OVER).
// The nib's "Test" button (testBtnPressed:) is not in the view hierarchy.

#include "EditorViewController.h"
#include "HWWindowDelegate.h"
#include "LevelTextView.h"

#include <string>

class ChapterMO;
class EditorLayer;
class LevelMO;

class SaveLevelViewController : public EditorViewController,
                                public LevelTextViewDelegate,
                                public HWWindowDelegate
{
public:
    // alloc + initWithNibName:@"SaveLevelViewController" bundle:[NSBundle mainBundle] levelMO:
    static SaveLevelViewController* create(LevelMO* levelMO);
    // maxNameChars = 20, maxDescChars = 300, charLeftString = capitalized CHARS LEFT,
    // setMO = chapter 5000, _levelMO = levelMO.
    bool initWithNibName(const std::string& nibName, LevelMO* levelMO);     // @ios 10010cee0

    void viewWillAppear(bool animated) override;                            // @ios 10010d024
    // super, contentSizeForViewInPopover 480x320 (unused), port: builds the nib layout.
    void viewDidLoad() override;                                            // @ios 10010d36c
    void didReceiveMemoryWarning() override;                                // @ios 10010d3c4
    void viewWillLayoutSubviews();                                          // @ios 10010d3f8  super only

    // ---- actions ----
    void saveOverBtn(cocos2d::Ref* sender);                                 // @ios 10010d52c
    void saveBtnPressed(cocos2d::Ref* sender);                              // @ios 10010d768
    // Debug leftover: logs the name of every level in setMO.
    void testBtnPressed(cocos2d::Ref* sender);                              // @ios 10010dc64
    void showSaveConfirmAlert();                                            // @ios 10010dd84
    void cancelBtnPressed(cocos2d::Ref* sender);                            // @ios 10010de58
    void cancelSave();                                                      // @ios 10010df70  presenting VC dismiss (no callers)

    // ---- UIAlertViewDelegate ----
    void alertViewClickedButtonAtIndex(int alertTag, long buttonIndex);     // @ios 10010df8c
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;
    // Port: releases the reference taken while one of our alerts is on screen.
    void hwWindowWasDismissed(HWWindow* window) override;

    // ---- UITextViewDelegate ----
    void textViewDidBeginEditing(LevelTextView* textView) override;         // @ios 10010e008  empty
    // Sets the matching counter to "%i %@" (max - length, charLeftString).
    void textViewDidChange(LevelTextView* textView) override;               // @ios 10010e00c
    void textViewDidEndEditing(LevelTextView* textView) override;           // @ios 10010e0b8  empty
    bool textViewShouldChangeTextInRange(LevelTextView* textView, size_t location, size_t length,
                                         const std::string& replacement) override;  // @ios 10010e0bc

    // ---- properties ----
    EditorLayer* editorLayer() const;                                       // @ios 10010e19c
    void setEditorLayer(EditorLayer* editorLayer);                          // @ios 10010e1ac
    LevelTextView* nameTextView() const;                                    // @ios 10010e1bc
    void setNameTextView(LevelTextView* textView);                          // @ios 10010e1cc
    LevelTextView* descriptionTextView() const;                             // @ios 10010e1d8
    void setDescriptionTextView(LevelTextView* textView);                   // @ios 10010e1e8
    cocos2d::ui::Text* nameLabel() const;                                      // @ios 10010e1f4
    void setNameLabel(cocos2d::ui::Text* label);                               // @ios 10010e204
    cocos2d::ui::Text* nameCharsLeftLabel() const;                             // @ios 10010e210
    void setNameCharsLeftLabel(cocos2d::ui::Text* label);                      // @ios 10010e220
    cocos2d::ui::Text* descriptionLabel() const;                               // @ios 10010e22c
    void setDescriptionLabel(cocos2d::ui::Text* label);                        // @ios 10010e23c
    cocos2d::ui::Text* descriptionCharsLeftLabel() const;                      // @ios 10010e248
    void setDescriptionCharsLeftLabel(cocos2d::ui::Text* label);               // @ios 10010e258
    cocos2d::ui::Button* cancelBtn() const;                                 // @ios 10010e264
    void setCancelBtn(cocos2d::ui::Button* button);                         // @ios 10010e274
    cocos2d::ui::Button* saveBtn() const;                                   // @ios 10010e280
    void setSaveBtn(cocos2d::ui::Button* button);                           // @ios 10010e290
    cocos2d::ui::Button* saveOverBtn() const;                               // @ios 10010e29c
    void setSaveOverBtn(cocos2d::ui::Button* button);                       // @ios 10010e2ac

protected:
    SaveLevelViewController();
    ~SaveLevelViewController() override;                                    // @ios 10010d42c  dealloc
    void showAlert(int tag, const std::string& title, const std::string& message,
                   const std::string& cancelTitle, const std::string& otherTitle, bool withDelegate);
    // -[NSString stringByTrimmingCharactersInSet:whitespaceAndNewlineCharacterSet]
    static std::string trimmed(const std::string& text);

    std::string charLeftString;                         // +0x08
    unsigned int maxNameChars;                          // +0x10  20
    unsigned int maxDescChars;                          // +0x14  300
    ChapterMO* setMO;                                   // +0x18  chapter 5000 (weak; owned by LevelStore)
    LevelMO* _levelMO;                                  // +0x20  retained
    EditorLayer* editorLayer_;                          // +0x28  "editorLayer" ivar (assign)
    LevelTextView* _nameTextView;                       // +0x30
    LevelTextView* _descriptionTextView;                // +0x38
    cocos2d::ui::Text* _nameLabel;                         // +0x40
    cocos2d::ui::Text* _nameCharsLeftLabel;                // +0x48
    cocos2d::ui::Text* _descriptionLabel;                  // +0x50
    cocos2d::ui::Text* _descriptionCharsLeftLabel;         // +0x58
    cocos2d::ui::Button* _cancelBtn;                    // +0x60
    cocos2d::ui::Button* _saveBtn;                      // +0x68
    cocos2d::ui::Button* _saveOverBtn;                  // +0x70
};
