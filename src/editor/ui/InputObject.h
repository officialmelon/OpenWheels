#pragma once
// iOS: InputObject : UIView (instanceStart 0x8, instanceSize 0x41).
// One property row in EditParametersView: an optional label plus an optional numeric text
// field. Subclasses add a slider (SliderInputObject, ColorInputObject) or a switch
// (SwitchInputObject). Created by the model from -[Special inputObjectForProperty:withRect:]
// (Special::inputObjectForPropertyWithRect and the RefShape / *Ref overrides, E1/E2), see
// "Contract" below.
//
// ===== Contract (E1/E2 create these; EditParametersView/SelectBackgroundUIView consume them) =====
//
// Creation - every factory returns an autoreleased object (cocos2d create idiom), nullptr on
// failure. `frame` is in iOS points, relative to the parent's content, origin top-left, y down:
// EditParametersView passes (8, y, scrollWidth - 16, 27). The object keeps that frame
// (frame() below) and EditParametersView stacks rows by frame().size.height + 8.
// `label` (and ColorInputObject's `colorLabel`) is a Localizable.strings KEY (e.g. "ANGLE");
// InputObject displays Localization::get(label) (which returns the string itself when it is not
// a key, so literal captions like "r" also work). An EMPTY string means nil (no label).
// `property` is the Special KVC key the row edits ("x", "angle", ...).
// Robustness: iOS passes some odd values faithfully (e.g. a negative max - min converted to an
// unsigned segment count); the factories accept any value without crashing (segment math is
// done in 64-bit / guarded, never divides by zero).
//
//   InputObject::create(frame, label, property, initialValue, addDisplayValueTextField)
//   SliderInputObject::create(frame, label, property, initialValue, min, max, segments)
//   SwitchInputObject::create(frame, label, property, initialValue)          // value 0 / 1
//   ColorInputObject::create(frame, label, colorLabel, property, initialValue, min, max, segments)
//   then optionally: io->setMinValue(min, max);  io->setIsAngleValue(true);
//
// Value: always a float (bools are 0/1, ints are converted). propertyValue() returns it.
// setPropertyValue(v, updateUndo, previous):
//   * if limits are enabled and v is outside [min, max] -> returns false, nothing changes;
//   * else stores v, refreshes the UI (updateUI) and calls
//     delegate->inputObjectParameter(property, v, updateUndo, previous); returns true.
//   previous is a float Value or Value::Null (iOS nil = "use the model's current value").
// User edits call setPropertyValue themselves:
//   * text field return: trimmed text, ignored when empty; parsed with floatValue semantics;
//     if limits are enabled: angle rows wrap with fmodf(v, 360) (+360 if negative), other rows
//     clamp to [min, max]; if different from the current value ->
//     setPropertyValue(v, true, Null); the text is always re-formatted afterwards;
//   * slider (uikit::Slider; only drags that start on the thumb): began remembers previousValue;
//     every change -> setPropertyValue(v, false, Null);
//     touch end -> setPropertyValue(v, true, previousValue) when v != previousValue;
//   * switch: every toggle -> setPropertyValue(isOn ? 1 : 0, true, Null).
// Model -> UI: the owner calls setPropertyValue(newValue, false, Null) when the model changes
//   (EditParametersView does this from its KVO callback when the value differs).
// Display: text field frame (0, 0, rowWidth, 27), text "%.2f" when showDecimal, else "%i" of
//   (int)value; colour rgba(0.29, 0.557, 0.784, 1), right aligned, Helvetica-Bold 16.
//   On PC (ui::EditBox) the value is committed only when editing ends with Return, like
//   textFieldShouldReturn:. Label: "<label>:" lower-cased,
//   Helvetica-Bold 16, white, gray shadow (0.5,0.5), frame (0, 0, w * 0.75, 27), tappable ->
//   delegate->inputObjectParameterLabelTapped(property) (EditParametersView: "angle" label
//   steps the rotation by 45 degrees).
// Focus: when the text field starts editing, posts "io_gains_focus" with this object.

#include <string>

#include "UIKitCompat.h"

class InputObject;
class Special;

// iOS protocol <InputObjectDelegate>, implemented by EditParametersView and
// SelectBackgroundUIView.
class InputObjectDelegate
{
public:
    virtual ~InputObjectDelegate() {}
    // -inputObjectParameter:didChange:updateUndo:previousValue:
    // previousValue: float Value, or Value::Null for nil.
    virtual void inputObjectParameter(const std::string& parameter, float value, bool updateUndo,
                                      const cocos2d::Value& previousValue) = 0;
    // -inputObjectParameterLabelTapped: (only EditParametersView implements it; a label is only
    // tappable when one was given, which SelectBackgroundUIView never does).
    virtual void inputObjectParameterLabelTapped(const std::string& parameter) {}
};

class InputObject : public uikit::View, public cocos2d::ui::EditBoxDelegate
{
public:
    using cocos2d::ui::Layout::update;  // keep Node::update(float) visible next to -update

    static InputObject* create(const cocos2d::Rect& frame, const std::string& label,
                               const std::string& property, float initialValue,
                               bool addDisplayValueTextField);
    // Sets limitsEnabled = false, editable = true, currentValue = initialValue; adds the text
    // field first (if requested), then the label (if non-empty).
    virtual bool initWithFrame(const cocos2d::Rect& frame, const std::string& label,
                               const std::string& property, float initialValue,
                               bool addDisplayValueTextField);                        // @ios 1000c9538

    void handleLabelTouch();                                                          // @ios 1000c9710
    float propertyValue() const;                                                      // @ios 1000c9730
    bool setPropertyValue(float value, bool updateUndo, const cocos2d::Value& previousValue); // @ios 1000c9740
    // Enables limits and sets the range (iOS -setMinValue:maxValue:).
    void setMinValue(float minValue, float maxValue);                                 // @ios 1000c980c
    // Refreshes every control from currentValue. Base: updateDisplayValue.
    virtual void updateUI();                                                          // @ios 1000c9838
    void updateDisplayValue();                                                        // @ios 1000c983c
    // iOS -setEnabled: (stores `enabled`, enables the text field). Overrides
    // ui::Widget::setEnabled on purpose; the Widget's own enabled state is left untouched.
    void setEnabled(bool enabled) override;                                           // @ios 1000c98d4

    // UITextFieldDelegate (driven from the EditBoxDelegate overrides below).
    void textFieldDidBeginEditing(cocos2d::ui::EditBox* textField);                   // @ios 1000c98f0
    void textFieldDidEndEditing(cocos2d::ui::EditBox* textField);                     // @ios 1000c98f4
    bool textFieldShouldBeginEditing(cocos2d::ui::EditBox* textField);                // @ios 1000c98f8
    bool textFieldShouldReturn(cocos2d::ui::EditBox* textField);                      // @ios 1000c9938
    bool textFieldShouldEndEditing(cocos2d::ui::EditBox* textField);                  // @ios 1000c9a54

    // Re-reads [special valueForKey:property] into currentValue and the text field
    // (unreferenced in iOS 1.2.7; `special` is never assigned there).
    void update();                                                                    // @ios 1000c9a5c
    void addDisplayValueTextField();                                                  // @ios 1000c9b14
    void removeFromSuperview() override;                                              // @ios 1000c9c7c

    bool enabled() const;                                                             // @ios 1000c9d0c
    InputObjectDelegate* delegate() const;                                            // @ios 1000c9d1c
    void setDelegate(InputObjectDelegate* delegate);                                  // @ios 1000c9d2c
    bool isAngleValue() const;                                                        // @ios 1000c9d3c
    void setIsAngleValue(bool isAngleValue);                                          // @ios 1000c9d4c

    const std::string& property() const { return _property; }

    // ui::EditBoxDelegate -> UITextFieldDelegate mapping:
    //   editBoxEditingDidBegin -> textFieldShouldBeginEditing + textFieldDidBeginEditing
    //   editBoxReturn          -> textFieldShouldReturn
    //   editBoxEditingDidEndWithAction -> textFieldShouldEndEditing + textFieldDidEndEditing
    void editBoxEditingDidBegin(cocos2d::ui::EditBox* editBox) override;
    void editBoxReturn(cocos2d::ui::EditBox* editBox) override;
    void editBoxEditingDidEndWithAction(cocos2d::ui::EditBox* editBox,
                                        cocos2d::ui::EditBoxDelegate::EditBoxEndAction action) override;

protected:
    InputObject();
    ~InputObject() override;                                                          // @ios 1000c9cb0 (dealloc: delegate = nil)

    Special* _special;                                // +0x08  Special (never set in iOS 1.2.7)
    std::string _property;                            // +0x10  NSString
    cocos2d::ui::EditBox* _displayValueTextField;     // +0x18  UITextField (nullptr if not added)
    cocos2d::ui::Text* _label;                        // +0x20  UILabel (nullptr if no label)
    bool _showDecimal;                                // +0x28
    bool _editable;                                   // +0x29
    bool _inputEnabled;                               // +0x2a  iOS `enabled` (renamed: ui::Widget has _enabled)
    bool _limitsEnabled;                              // +0x2b
    float _minValue;                                  // +0x2c
    float _maxValue;                                  // +0x30
    float _currentValue;                              // +0x34  iOS `currentValue`
    InputObjectDelegate* _delegate;                   // +0x38  id<InputObjectDelegate> (assign)
    bool _isAngleValue;                               // +0x40
};
