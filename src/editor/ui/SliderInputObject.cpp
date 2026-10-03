#include "SliderInputObject.h"

#include <cmath>

USING_NS_CC;

SliderInputObject::SliderInputObject() : _slider(nullptr), _segments(0), _previousValue(0.0f) {}

SliderInputObject* SliderInputObject::create(const Rect& frame, const std::string& label, const std::string& property,
                                             float initialValue, float minValue, float maxValue, unsigned int segments)
{
    SliderInputObject* io = new (std::nothrow) SliderInputObject();
    if (io && io->initWithFrame(frame, label, property, initialValue, minValue, maxValue, segments))
    {
        io->autorelease();
        return io;
    }
    delete io;
    return nullptr;
}

// @ios 1000ca11c
bool SliderInputObject::initWithFrame(const Rect& frame, const std::string& label, const std::string& property,
                                      float initialValue, float minValue, float maxValue, unsigned int segments)
{
    // The row is two lines: the label / text field line, then the slider line.
    const int rowHeight = uikit::fcvtzs(frame.size.height);
    Rect rowFrame = frame;
    rowFrame.size.height = static_cast<float>(static_cast<double>(rowHeight * 2 + 4));
    if (!InputObject::initWithFrame(rowFrame, label, property, initialValue, true))
        return false;
    setMinValue(minValue, maxValue);
    _showDecimal = true;
    if (segments != 0)
    {
        if (std::fmod(maxValue - minValue, static_cast<float>(segments)) == 0.0f) _showDecimal = false;
    }
    _segments = segments;

    _slider = uikit::Slider::create();
    _slider->setValue((_currentValue - _minValue) / (_maxValue - _minValue));
    _slider->setMinimumTrackTintColor(uikit::color(0.239215686917305, 0.5333333611488342, 0.7803921699523926, 1.0));
    _slider->setCallback([this](uikit::Slider* slider, uikit::Slider::Event event) {
        switch (event)
        {
        case uikit::Slider::Event::TouchDown: sliderBegan(slider); break;
        case uikit::Slider::Event::ValueChanged: sliderChange(slider); break;
        case uikit::Slider::Event::TouchUp: sliderEnd(slider); break;
        }
    });
    addSubview(_slider, Rect(0, static_cast<float>(rowHeight + 4), this->frame().size.width,
                             static_cast<float>(static_cast<unsigned int>(rowHeight))));
    updateDisplayValue();
    return true;
}

// Slider value -> snapped fraction (UISlider value with the segment snapping).
static float snappedSliderValue(float value, unsigned int segments)
{
    if (segments == 0) return value;
    const float s = static_cast<float>(segments);
    return std::round(value * s) / s;  // frinta (round half away from zero)
}

// @ios 1000ca330
void SliderInputObject::sliderBegan(Ref* /*sender*/)
{
    const float v = snappedSliderValue(_slider->value(), _segments);
    _previousValue = _minValue + v * (_maxValue - _minValue);
}

// @ios 1000ca3b0
void SliderInputObject::sliderChange(Ref* /*sender*/) { calculateSliderValueWithUpdateUndo(false); }

// @ios 1000ca3b8
void SliderInputObject::sliderEnd(Ref* /*sender*/) { calculateSliderValueWithUpdateUndo(true); }

// @ios 1000ca3c0
void SliderInputObject::calculateSliderValueWithUpdateUndo(bool updateUndo)
{
    float v = _slider->value();
    if (_segments != 0)
    {
        v = snappedSliderValue(v, _segments);
        _slider->setValue(v);
    }
    const float value = _minValue + v * (_maxValue - _minValue);
    if (!updateUndo)
    {
        setPropertyValue(value, false, Value::Null);
        return;
    }
    if (value != _previousValue) setPropertyValue(value, true, Value(_previousValue));
}

// @ios 1000ca4c4
void SliderInputObject::updateUI()
{
    InputObject::updateUI();
    _slider->setValue((_currentValue - _minValue) / (_maxValue - _minValue));
}

// @ios 1000ca550
void SliderInputObject::setEnabled(bool enabled)
{
    InputObject::setEnabled(enabled);
    _slider->setSliderEnabled(enabled);
}

// @ios 1000ca5a8
void SliderInputObject::removeFromSuperview()
{
    _slider->setCallback(nullptr);
    _slider->removeFromParent();
    InputObject::removeFromSuperview();
}
