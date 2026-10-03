#pragma once
// iOS: SliderInputObject : InputObject (instanceStart 0x40, instanceSize 0x50).
// A numeric row with a text field (always added) and a UISlider below it
// (ui::Slider; minimumTrackTintColor rgba(0.239, 0.533, 0.780, 1)).
// Used by RefShape ("collision") and the *Ref classes (E2, e.g. ArrowGunRef).
//
// init: limits = [minValue, maxValue]; showDecimal = true, but false when segments != 0 and
// fmodf(max - min, segments) == 0. The slider shows (value - min) / (max - min).
// Slider value s (0..1) -> property: if segments: s = roundf(s * segments) / segments (and the
// slider snaps to it); value = min + s * (max - min).
// Events (UIControlEvents): TouchDown -> sliderBegan, ValueChanged -> sliderChange,
// TouchUpInside / TouchUpOutside / TouchCancel -> sliderEnd. See InputObject.h for the
// setPropertyValue/undo contract.

#include "InputObject.h"

class SliderInputObject : public InputObject
{
public:
    static SliderInputObject* create(const cocos2d::Rect& frame, const std::string& label,
                                     const std::string& property, float initialValue,
                                     float minValue, float maxValue, unsigned int segments);
    virtual bool initWithFrame(const cocos2d::Rect& frame, const std::string& label,
                               const std::string& property, float initialValue, float minValue,
                               float maxValue, unsigned int segments);               // @ios 1000ca11c

    void sliderBegan(cocos2d::Ref* sender);                                         // @ios 1000ca330
    void sliderChange(cocos2d::Ref* sender);                                        // @ios 1000ca3b0
    void sliderEnd(cocos2d::Ref* sender);                                           // @ios 1000ca3b8
    void calculateSliderValueWithUpdateUndo(bool updateUndo);                       // @ios 1000ca3c0
    void updateUI() override;                                                       // @ios 1000ca4c4
    void setEnabled(bool enabled) override;                                         // @ios 1000ca550
    void removeFromSuperview() override;                                            // @ios 1000ca5a8

protected:
    SliderInputObject();
    using InputObject::initWithFrame;

    uikit::Slider* _slider;    // +0x40  UISlider
    unsigned int _segments;          // +0x48
    float _previousValue;            // +0x4c  property value at sliderBegan
};
