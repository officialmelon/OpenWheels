#include "ColorInputObject.h"

#include <cmath>

#include "platform/common/Localization.h"

USING_NS_CC;

ColorInputObject::ColorInputObject() : _slider(nullptr), _segments(0), _previousValue(0.0f) {}

ColorInputObject* ColorInputObject::create(const Rect& frame, const std::string& label, const std::string& colorLabel,
                                           const std::string& property, float initialValue, float minValue,
                                           float maxValue, unsigned int segments)
{
    ColorInputObject* io = new (std::nothrow) ColorInputObject();
    if (io && io->initWithFrame(frame, label, colorLabel, property, initialValue, minValue, maxValue, segments))
    {
        io->autorelease();
        return io;
    }
    delete io;
    return nullptr;
}

// @ios 1000de60c
bool ColorInputObject::initWithFrame(const Rect& frame, const std::string& label, const std::string& colorLabel,
                                     const std::string& property, float initialValue, float minValue, float maxValue,
                                     unsigned int segments)
{
    // One line (slider + value), plus a caption line above when a label is given.
    const int rowHeight = uikit::fcvtzs(frame.size.height);
    const bool hasLabel = !label.empty();
    Rect rowFrame = frame;
    rowFrame.size.height = static_cast<float>(static_cast<double>((hasLabel ? rowHeight * 2 : rowHeight) + 4));
    if (!InputObject::initWithFrame(rowFrame, label, property, initialValue, true)) return false;
    setMinValue(minValue, maxValue);
    _showDecimal = true;
    if (segments != 0)
    {
        if (std::fmod(maxValue - minValue, static_cast<float>(segments)) != 0.0f) _showDecimal = false;
    }
    const float y = hasLabel ? static_cast<float>(rowHeight + 4) : 0.0f;

    // Channel label ("r:", "g:", ...): not tappable.
    float sliderX = 0.0f;  // [nil frame].size.width == 0 without a channel label
    if (!colorLabel.empty())
    {
        ui::Text* channel = uikit::makeLabel(uikit::lowercaseString(Localization::get(colorLabel) + ":"), true, 16.0f, 0);
        channel->setTextColor(uikit::whiteColor());
        uikit::setLabelShadow(channel, uikit::grayColor(), Size(0.5f, 0.5f));
        channel->setTouchEnabled(false);
        addSubview(channel, Rect(0, y, 25.0f, 27.0f));
        sliderX = 25.0f;
    }
    _segments = segments;

    _slider = uikit::Slider::create();
    const Rect sliderFrame(sliderX, y, static_cast<float>(this->frame().size.width + -60.0),
                           static_cast<float>(static_cast<unsigned int>(rowHeight)));
    // The value field moves to the slider's line (origin.x stays 0, its size is kept).
    if (_displayValueTextField != nullptr)
    {
        const Rect fieldFrame = subviewFrame(_displayValueTextField);
        setSubviewFrame(_displayValueTextField, Rect(0, sliderFrame.origin.y, fieldFrame.size.width, fieldFrame.size.height));
    }
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
    addSubview(_slider, sliderFrame);
    updateDisplayValue();
    return true;
}

// @ios 1000de998
void ColorInputObject::sliderBegan(Ref* /*sender*/)
{
    float v = _slider->value();
    if (_segments != 0)
    {
        const float s = static_cast<float>(_segments);
        v = std::round(v * s) / s;
    }
    _previousValue = _minValue + v * (_maxValue - _minValue);
}

// @ios 1000dea18
void ColorInputObject::sliderChange(Ref* /*sender*/) { calculateSliderValueWithUpdateUndo(false); }

// @ios 1000dea20
void ColorInputObject::sliderEnd(Ref* /*sender*/) { calculateSliderValueWithUpdateUndo(true); }

// @ios 1000dea28
void ColorInputObject::calculateSliderValueWithUpdateUndo(bool updateUndo)
{
    float v = std::round(_slider->value() * 100.0f) * 0.01f;
    if (_segments != 0)
    {
        const float s = static_cast<float>(_segments);
        v = std::round(v * s) / s;
        _slider->setValue(v);
    }
    const float value = _minValue + v * (_maxValue - _minValue);
    log("-- newValue %f", static_cast<double>(value));
    if (!updateUndo)
    {
        setPropertyValue(value, false, Value::Null);
        return;
    }
    if (value != _previousValue) setPropertyValue(value, true, Value(_previousValue));
}

// @ios 1000deb6c
void ColorInputObject::updateUI()
{
    InputObject::updateUI();
    _slider->setValue((_currentValue - _minValue) / (_maxValue - _minValue));
}

// @ios 1000debf8
void ColorInputObject::setEnabled(bool enabled)
{
    InputObject::setEnabled(enabled);
    _slider->setSliderEnabled(enabled);
}

// @ios 1000dec50
void ColorInputObject::removeFromSuperview()
{
    _slider->setCallback(nullptr);
    _slider->removeFromParent();
    InputObject::removeFromSuperview();
}
