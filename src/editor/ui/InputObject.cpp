#include "InputObject.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Special.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace {
// [NSString stringWithFormat:@"%.2f", value] / @"%i".
std::string formatValue(bool showDecimal, float value)
{
    char buf[64];
    if (showDecimal)
        std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(value));
    else
        std::snprintf(buf, sizeof(buf), "%i", uikit::fcvtzs(value));
    return buf;
}
}  // namespace

InputObject::InputObject()
    : _special(nullptr), _displayValueTextField(nullptr), _label(nullptr), _showDecimal(false), _editable(false),
      _inputEnabled(false), _limitsEnabled(false), _minValue(0.0f), _maxValue(0.0f), _currentValue(0.0f),
      _delegate(nullptr), _isAngleValue(false)
{
}

// @ios 1000c9cb0 (dealloc)
InputObject::~InputObject()
{
    _delegate = nullptr;
    if (_displayValueTextField != nullptr) _displayValueTextField->setDelegate(nullptr);
}

InputObject* InputObject::create(const Rect& frame, const std::string& label, const std::string& property,
                                 float initialValue, bool addDisplayValueTextField)
{
    InputObject* io = new (std::nothrow) InputObject();
    if (io && io->initWithFrame(frame, label, property, initialValue, addDisplayValueTextField))
    {
        io->autorelease();
        return io;
    }
    delete io;
    return nullptr;
}

// @ios 1000c9538
bool InputObject::initWithFrame(const Rect& frame, const std::string& label, const std::string& property,
                                float initialValue, bool addDisplayValueTextField)
{
    if (!uikit::View::initWithFrame(frame)) return false;
    _limitsEnabled = false;
    _editable = true;
    _currentValue = initialValue;
    _property = property;
    _labelKey = label;  // EDITOR (browser features, PC addition)
    if (addDisplayValueTextField) this->addDisplayValueTextField();
    if (!label.empty())
    {
        const float w = this->frame().size.width;
        // [[NSString stringWithFormat:@"%@:", label] lowercaseString]; labels arrive as
        // Localizable keys or already-localized text (Localization::get passes those through).
        _label = uikit::makeLabel(uikit::lowercaseString(Localization::get(label) + ":"), true, 16.0f, 0);
        _label->setTextColor(uikit::whiteColor());
        uikit::setLabelShadow(_label, uikit::grayColor(), Size(0.5f, 0.5f));
        _label->setTouchEnabled(true);  // UITapGestureRecognizer -> handleLabelTouch
        _label->addClickEventListener([this](Ref*) { handleLabelTouch(); });
        addSubview(_label, Rect(0, 0, static_cast<float>(w * 0.75), 27.0f));
    }
    return true;
}

// @ios 1000c9710
void InputObject::handleLabelTouch()
{
    if (_delegate != nullptr) _delegate->inputObjectParameterLabelTapped(_property);
}

// @ios 1000c9730
float InputObject::propertyValue() const { return _currentValue; }

// @ios 1000c9740
bool InputObject::setPropertyValue(float value, bool updateUndo, const Value& previousValue)
{
    if (_limitsEnabled)
    {
        if (value < _minValue) return false;
        if (_maxValue < value) return false;
    }
    _currentValue = value;
    updateUI();
    if (_delegate != nullptr) _delegate->inputObjectParameter(_property, _currentValue, updateUndo, previousValue);
    return true;
}

// @ios 1000c980c
void InputObject::setMinValue(float minValue, float maxValue)
{
    _limitsEnabled = true;
    _minValue = minValue;
    _maxValue = maxValue;
}

// @ios 1000c9838
void InputObject::updateUI() { updateDisplayValue(); }

// @ios 1000c983c
void InputObject::updateDisplayValue()
{
    if (_displayValueTextField == nullptr) return;
    _displayValueTextField->setText(formatValue(_showDecimal, _currentValue).c_str());
}

// @ios 1000c98d4
void InputObject::setEnabled(bool enabled)
{
    _inputEnabled = enabled;
    if (_displayValueTextField != nullptr) _displayValueTextField->setEnabled(enabled);
}

// @ios 1000c98f0
void InputObject::textFieldDidBeginEditing(ui::EditBox* /*textField*/) {}

// @ios 1000c98f4
void InputObject::textFieldDidEndEditing(ui::EditBox* /*textField*/) {}

// @ios 1000c98f8
bool InputObject::textFieldShouldBeginEditing(ui::EditBox* /*textField*/)
{
    uikit::NotificationCenter::postNotification(uikit::notification::kIOGainsFocus, this, nullptr);
    return true;
}

// @ios 1000c9938
bool InputObject::textFieldShouldReturn(ui::EditBox* textField)
{
    // [textField resignFirstResponder] - the EditBox has already ended editing.
    const std::string text = uikit::trimmedString(textField->getText());
    if (text != "")
    {
        float value = std::strtof(text.c_str(), nullptr);  // -[NSString floatValue]
        if (_limitsEnabled)
        {
            if (_isAngleValue)
            {
                value = std::fmod(value, 360.0f);
                if (!(value >= 0.0f)) value = value + 360.0f;
            }
            else
            {
                value = std::fmax(std::fmin(value, _maxValue), _minValue);  // fminnm, fmaxnm
            }
        }
        if (_currentValue != value)
        {
            setPropertyValue(value, true, Value::Null);
            return false;
        }
    }
    updateDisplayValue();
    return false;
}

// @ios 1000c9a54
bool InputObject::textFieldShouldEndEditing(ui::EditBox* /*textField*/) { return true; }

// @ios 1000c9a5c
void InputObject::update()
{
    // A nil special answers nil -> 0 (messaging nil); `special` is never set in iOS 1.2.7.
    const Value value = _special != nullptr ? _special->valueForKey(_property) : Value(0.0f);
    _currentValue = value.asFloat();
    if (_displayValueTextField == nullptr) return;
    if (_showDecimal)
        _displayValueTextField->setText(formatValue(true, _currentValue).c_str());
    else
        _displayValueTextField->setText(formatValue(false, value.asFloat()).c_str());
}

// @ios 1000c9b14
void InputObject::addDisplayValueTextField()
{
    const float k = uikit::pointsToDesign();
    const Rect fieldFrame(0, 0, frame().size.width, 27.0f);
    ui::Scale9Sprite* background = ui::Scale9Sprite::createWithSpriteFrameName(
        uikit::generatedImage("clear4", 4, 4, [](int, int) { return Color4B(0, 0, 0, 0); }));
    _displayValueTextField = ui::EditBox::create(fieldFrame.size * k, background);
    _displayValueTextField->setFont(uikit::fontFile(true).c_str(), static_cast<int>(16.0f * k));
    _displayValueTextField->setFontColor(uikit::color(0.29019609093666077, 0.5568627715110779, 0.7843137383460999, 1.0));
    _displayValueTextField->setTextHorizontalAlignment(TextHAlignment::RIGHT);
    _displayValueTextField->setDelegate(this);
    _displayValueTextField->setInputMode(ui::EditBox::InputMode::SINGLE_LINE);   // UIKeyboardTypeNumbersAndPunctuation
    _displayValueTextField->setReturnType(ui::EditBox::KeyboardReturnType::DONE);  // UIReturnKeyDone
    _displayValueTextField->setText(formatValue(true, _currentValue).c_str());
    addSubview(_displayValueTextField, fieldFrame);
    updateDisplayValue();
}

// @ios 1000c9c7c
void InputObject::removeFromSuperview() { uikit::View::removeFromSuperview(); }

// @ios 1000c9d0c
bool InputObject::enabled() const { return _inputEnabled; }

// @ios 1000c9d1c
InputObjectDelegate* InputObject::delegate() const { return _delegate; }

// @ios 1000c9d2c
void InputObject::setDelegate(InputObjectDelegate* delegate) { _delegate = delegate; }

// @ios 1000c9d3c
bool InputObject::isAngleValue() const { return _isAngleValue; }

// @ios 1000c9d4c
void InputObject::setIsAngleValue(bool isAngleValue) { _isAngleValue = isAngleValue; }

// ---- ui::EditBoxDelegate -> UITextFieldDelegate ------------------------------------------------

void InputObject::editBoxEditingDidBegin(ui::EditBox* editBox)
{
    textFieldShouldBeginEditing(editBox);
    textFieldDidBeginEditing(editBox);
}

void InputObject::editBoxReturn(ui::EditBox* /*editBox*/) {}

void InputObject::editBoxEditingDidEndWithAction(ui::EditBox* editBox, ui::EditBoxDelegate::EditBoxEndAction action)
{
    // UIKit only calls textFieldShouldReturn: for the return key; leaving the field otherwise
    // keeps the typed text uncommitted.
    if (action == ui::EditBoxDelegate::EditBoxEndAction::RETURN) textFieldShouldReturn(editBox);
    textFieldShouldEndEditing(editBox);
    textFieldDidEndEditing(editBox);
}
