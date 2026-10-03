#include "SwitchInputObject.h"

USING_NS_CC;

SwitchInputObject::SwitchInputObject() : _switchView(nullptr) {}

SwitchInputObject* SwitchInputObject::create(const Rect& frame, const std::string& label, const std::string& property,
                                             float initialValue)
{
    SwitchInputObject* io = new (std::nothrow) SwitchInputObject();
    if (io && io->initWithFrame(frame, label, property, initialValue))
    {
        io->autorelease();
        return io;
    }
    delete io;
    return nullptr;
}

// @ios 1000ca65c
bool SwitchInputObject::initWithFrame(const Rect& frame, const std::string& label, const std::string& property,
                                      float initialValue)
{
    if (!InputObject::initWithFrame(frame, label, property, initialValue, false)) return false;
    // [[UISwitch alloc] initWithFrame:CGRectZero] takes its intrinsic size; right-aligned in the row.
    _switchView = uikit::Switch::create();
    const float w = uikit::Switch::kWidth, h = uikit::Switch::kHeight;
    _switchView->setOnTintColor(uikit::color(0.239215686917305, 0.5333333611488342, 0.7803921699523926, 1.0));
    _switchView->setCallback([this](uikit::Switch* sender) { switchChange(sender); });
    _switchView->setOn(initialValue != 0.0f, false);
    addSubview(_switchView, Rect(this->frame().size.width - w, 0, w, h));
    return true;
}

// @ios 1000ca7a8
void SwitchInputObject::switchChange(Ref* sender)
{
    const bool on = static_cast<uikit::Switch*>(sender)->isOn();
    setPropertyValue(on ? 1.0f : 0.0f, true, Value::Null);
}

// @ios 1000ca7dc
void SwitchInputObject::setEnabled(bool enabled)
{
    InputObject::setEnabled(enabled);
    _inputEnabled = enabled;
    _switchView->setSwitchEnabled(enabled);
}

// @ios 1000ca844
void SwitchInputObject::updateUI()
{
    InputObject::updateUI();
    _switchView->setOn(_currentValue != 0.0f, true);
}
