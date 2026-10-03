#pragma once
// iOS: EditParametersView : EditorUIView (instanceStart 0x30, instanceSize 0x68).
// The property inspector. Opened by EditorLayer::editParamsBtnPressed: with the selected refs
// (frame x = winW - min(winW/2, 284) - notch, full height) and the batch node's undo manager.
// Backgrounds: self rgba(0.706, 0.706, 0.706, 0.5), scroll view rgba(0.8, 0.8, 0.8, 0.25).
//
// addInputs: 0 refs -> title "NOTHING SELECTED"; 2+ refs -> title "MULTIPLE OBJECTS" and NO
// inputs; exactly 1 ref -> special = refs[0], title = capitalized special->name(), and for each
// key of special->propertyKeysForUI(): io = special->inputObjectForPropertyWithRect(key,
// (8, y, scrollW - 16, 27)); if io: io->setDelegate(this), add to the scroll view, inputDict[key]
// = io, y += io->frame().size.height + 8 (y starts at 8); finally the scroll content height
// follows y.
//
// Model -> UI (iOS KVO): every key of propertyKeysForUI of every ref is observed;
// observeValueForKeyPath pushes the new value into inputDict[key] with
// setPropertyValue(v, false, Null) when it differs.
// UI -> model (inputObjectParameter):
//   updateUndo == false: for each ref whose valueForKey(key) != value: setValueForKey(value, key)
//   updateUndo == true : undo group; for each ref register undo "setValueForKey(old, key)" where
//                        old = previousValue, or the ref's current value when previousValue is
//                        Null; then setValueForKey(value, key); end group; post
//                        "undo_stack_updated" (object = undo manager)
//   always: post "sel_rect_changed".
// Label tap "angle": undo-registers the current rotation, then sets angle to
// fmodf(rotation, 45) == 0 ? fmodf(rotation + 45, 360) : ceilf(rotation * (1/45.f)) * 45.
// Notifications observed: "sel_ref_change" (object = selected refs) -> rebuild,
// "ref_ui_keys_will_change" / "ref_ui_keys_changed" -> drop / rebuild inputs,
// "io_gains_focus" -> remember focused input (keyboard scroll; inert on PC),
// keyboard did show / will hide (UIKeyboard notifications; no equivalent on PC - kept, unused).
// Localization keys: "NOTHING SELECTED", "MULTIPLE OBJECTS", "CLOSE".

#include <string>
#include <unordered_map>

#include "EditorUIView.h"
#include "InputObject.h"

class Special;
class EditorUndoManager;

class EditParametersView : public EditorUIView, public InputObjectDelegate
{
public:
    static EditParametersView* create(const cocos2d::Rect& frame, const cocos2d::Vector<Special*>& specials);
    // initWithFrame:specials:
    virtual bool initWithFrame(const cocos2d::Rect& frame, const cocos2d::Vector<Special*>& specials); // @ios 1000c6048

    void addInputs();                                                               // @ios 1000c6248
    void removeFromSuperview() override;                                            // @ios 1000c647c
    void removeInputObjects();                                                      // @ios 1000c64e8
    void uiKeysWillChange(void* notificationObject);                                // @ios 1000c6670
    void uiKeysDidChange(void* notificationObject);                                 // @ios 1000c6674
    // object = cocos2d::Vector<Special*>* (the new selection).
    void handleSelectionChange(void* notificationObject);                           // @ios 1000c6698
    void addParameterObservers();                                                   // @ios 1000c6724
    void removeParameterObservers();                                                // @ios 1000c68c8
    // KVO callback (Special property observers, see E4.md "Needs from E1").
    void observeValueForKeyPath(const std::string& keyPath, Special* object,
                                const cocos2d::Value& newValue);                    // @ios 1000c6a64
    void keyboardDidShow(float keyboardHeight);                                     // @ios 1000c6ae8
    void scrollToKeyboardFocus();                                                   // @ios 1000c6bc4
    void keyboardWillHide();                                                        // @ios 1000c6c98
    void keyboardFocusChange(void* notificationObject);                             // @ios 1000c6d18  (object = InputObject*)
    void textViewDidEndEditing(cocos2d::ui::EditBox* textView);                     // @ios 1000c6d68
    bool textFieldShouldReturn(cocos2d::ui::EditBox* textField);                    // @ios 1000c6d70
    void textViewDidChange(cocos2d::ui::EditBox* textView);                         // @ios 1000c6d8c  (empty)

    // InputObjectDelegate
    void inputObjectParameter(const std::string& parameter, float value, bool updateUndo,
                              const cocos2d::Value& previousValue) override;        // @ios 1000c6d90
    void inputObjectParameterLabelTapped(const std::string& parameter) override;    // @ios 1000c70a8

    void confirmChanges(cocos2d::Ref* sender);                                      // @ios 1000c7214  (removeFromSuperview)
    void cancelChanges(cocos2d::Ref* sender);                                       // @ios 1000c7218  (removeFromSuperview)

    const cocos2d::Vector<InputObject*>& inputObjects() const;                      // @ios 1000c721c
    EditorUndoManager* undoManager() const;                                         // @ios 1000c722c
    void setUndoManager(EditorUndoManager* undoManager);                            // @ios 1000c723c

protected:
    EditParametersView();
    ~EditParametersView() override;                                                 // @ios 1000c65fc  dealloc
    using EditorUIView::initWithFrame;

    Special* _special;                                          // +0x30  Special (refs[0] when 1 ref)
    cocos2d::Vector<Special*> _refs;                            // +0x38  NSMutableArray
    std::unordered_map<std::string, InputObject*> _inputDict;   // +0x40  NSMutableDictionary (inputs are children of the scroll view)
    InputObject* _keyboardFocusView;                            // +0x48  UIView (assign)
    bool _keyboardPresent;                                      // +0x50
    EditorUndoManager* _undoManager;                            // +0x58  NSUndoManager (owned by EditorSpriteBatchNode)
    cocos2d::Vector<InputObject*> _inputObjects;                // +0x60  NSMutableArray (never filled in iOS 1.2.7)
};
