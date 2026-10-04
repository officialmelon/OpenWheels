#pragma once
// iOS: ColorInputObject : InputObject (instanceStart 0x40, instanceSize 0x50).
// One colour channel row: optional caption label (first line), a short channel label
// (`colorLabel`, e.g. "r"/"g"/"b"/"a": 25 x 27 at x 0, not tappable), a slider and the value
// text field (moved to the right of the slider). The slider row starts at y = 0, or at
// (int)height + 4 when a caption label is present; slider frame (25, y, rowWidth - 60, height).
// Used by RefShape (innerRed/Green/Blue: 0..255, 254 segments; shapeOpacity: 99 segments, E1)
// and SelectBackgroundUIView (rColor/gColor/bColor: 0..255, 254 segments, no caption).
//
// Same flow as SliderInputObject except calculateSliderValueWithUpdateUndo first rounds the
// slider value to 1/100 (roundf(s * 100) * 0.01f) and NSLogs it. Unlike SliderInputObject,
// showDecimal is cleared when fmodf(max - min, segments) != 0 (inverted test, confirmed in the
// assembly: b.eq skips the store).

#include "InputObject.h"

class ColorInputObject : public InputObject
{
public:
    static ColorInputObject* create(const cocos2d::Rect& frame, const std::string& label,
                                    const std::string& colorLabel, const std::string& property,
                                    float initialValue, float minValue, float maxValue,
                                    unsigned int segments);
    virtual bool initWithFrame(const cocos2d::Rect& frame, const std::string& label,
                               const std::string& colorLabel, const std::string& property,
                               float initialValue, float minValue, float maxValue,
                               unsigned int segments);                               // @ios 1000de60c

    void sliderBegan(cocos2d::Ref* sender);                                         // @ios 1000de998
    void sliderChange(cocos2d::Ref* sender);                                        // @ios 1000dea18
    void sliderEnd(cocos2d::Ref* sender);                                           // @ios 1000dea20
    void calculateSliderValueWithUpdateUndo(bool updateUndo);                       // @ios 1000dea28
    void updateUI() override;                                                       // @ios 1000deb6c
    void setEnabled(bool enabled) override;                                         // @ios 1000debf8
    void removeFromSuperview() override;                                            // @ios 1000dec50
    unsigned int segments() const { return _segments; }  // EDITOR (browser features, PC addition)

protected:
    ColorInputObject();
    using InputObject::initWithFrame;

    uikit::Slider* _slider;    // +0x40  UISlider
    unsigned int _segments;          // +0x48
    float _previousValue;            // +0x4c
};
