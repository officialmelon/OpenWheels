#pragma once
// iOS: SwitchInputObject : InputObject (instanceStart 0x40, instanceSize 0x48).
// A boolean row: label + UISwitch (ui::CheckBox), no text field. The switch is right-aligned in
// the row (x = rowWidth - switchWidth, y = 0), onTintColor rgba(0.239, 0.533, 0.780, 1), and starts
// on when initialValue != 0. Used by Special for "fixed", "sleeping", "interactive" (E1).
// Toggling calls setPropertyValue(isOn ? 1 : 0, updateUndo = true, Null).

#include "InputObject.h"

class SwitchInputObject : public InputObject
{
public:
    static SwitchInputObject* create(const cocos2d::Rect& frame, const std::string& label,
                                     const std::string& property, float initialValue);
    // Calls InputObject's init with addDisplayValueTextField = false.
    virtual bool initWithFrame(const cocos2d::Rect& frame, const std::string& label,
                               const std::string& property, float initialValue);     // @ios 1000ca65c

    void switchChange(cocos2d::Ref* sender);                                        // @ios 1000ca7a8
    void setEnabled(bool enabled) override;                                         // @ios 1000ca7dc
    void updateUI() override;                                                       // @ios 1000ca844

protected:
    SwitchInputObject();
    using InputObject::initWithFrame;

    uikit::Switch* _switchView;  // +0x40  UISwitch
};
