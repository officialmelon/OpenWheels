#include "BladeWeaponRef.h"

#include "SliderInputObject.h"
#include "SwitchInputObject.h"

USING_NS_CC;

// @ios 1000f5814
bool BladeWeaponRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    // Before the type table is filled (as on iOS): builds the first sprite.
    setBladeType(1);
    // Blade 9 is not in the editor's list.
    static const unsigned int kBladeTypes[11] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12};
    for (int i = 0; i < 11; ++i)
    {
        _bladeTypes[i] = kBladeTypes[i];
    }
    setLevelItemID(34);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "angle", "reverse",
                                               "sleeping", "interactive", "bladeType"});
    return true;
}

// @ios 1000f5970
std::vector<std::string> BladeWeaponRef::propertyKeysForUI()
{
    return {"xMeters", "yMeters", "angle", "reverse", "sleeping", "interactive", "bladeType"};
}

// @ios 1000f5a24
InputObject* BladeWeaponRef::inputObjectForPropertyWithRect(const std::string& property,
                                                           const Rect& rect)
{
    if (property == "bladeType")
    {
        // The slider edits the index into _bladeTypes, not the type itself.
        return SliderInputObject::create(rect, "BLADE TYPE", "bladeSliderIndex",
                                         (float)bladeSliderIndex(), 1.0f, 11.0f, 10);
    }
    if (property == "reverse")
    {
        return SwitchInputObject::create(rect, "REVERSE", "reverse", _reverse ? 1.0f : 0.0f);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000f5bd4
unsigned int BladeWeaponRef::bladeSliderIndex()
{
    for (unsigned int i = 0; i < 11; ++i)
    {
        if (_bladeType == _bladeTypes[i])
        {
            return i + 1;
        }
    }
    return 1;
}

// @ios 1000f5cc8
void BladeWeaponRef::setBladeSliderIndex(unsigned int index)
{
    KeyValueChange kvo(this, "bladeSliderIndex");
    // iOS indexes _bladeTypes[index - 1] unchecked; the slider keeps index in 1..11.
    // RE-TODO(@1000f5cc8): out-of-range indices read neighbouring ivars on iOS; guarded here.
    if (index >= 1 && index <= 12)
    {
        _bladeType = _bladeTypes[index - 1];
    }
    updateSprite();
}

// @ios 1000f5cec
void BladeWeaponRef::setBladeType(unsigned int bladeType)
{
    KeyValueChange kvo(this, "bladeType");
    if (bladeType > 11)
    {
        bladeType = 12;
    }
    _bladeType = bladeType;
    updateSprite();
}

// @ios 1000f5d08
void BladeWeaponRef::updateSprite()
{
    Node* old = getChildByTag(1);
    if (old != nullptr)
    {
        old->removeFromParentAndCleanup(false);
    }
    Sprite* blade = Sprite::createWithSpriteFrameName(
        StringUtils::format("e_blade_%i.png", (int)_bladeType));
    blade->setPosition(Vec2(0.5f, 0.5f));
    blade->setTag(1);
    // port: art at its iOS point size (iOS only sets scaleX +-1).
    blade->setScaleY(editorArtScale());
    blade->setScaleX((_reverse ? -1.0f : 1.0f) * editorArtScale());
    addChild(blade);

    // textureRect in iOS points.
    const float width = blade->getTextureRect().size.width * editorArtScale();
    const float height = blade->getTextureRect().size.height * editorArtScale();
    setRefRect(Rect(width * -0.5f, height * -0.5f, width, height));
}

// @ios 1000f5e10
void BladeWeaponRef::setReverse(bool reverse)
{
    KeyValueChange kvo(this, "reverse");
    _reverse = reverse;
    updateSprite();
}

// @ios 1000f5e20
unsigned int BladeWeaponRef::bladeType()
{
    return _bladeType;
}

// @ios 1000f5e30
bool BladeWeaponRef::reverse()
{
    return _reverse;
}

Value BladeWeaponRef::valueForKey(const std::string& key)
{
    if (key == "reverse")
    {
        return Value(reverse());
    }
    if (key == "bladeType")
    {
        return Value((int)bladeType());
    }
    if (key == "bladeSliderIndex")
    {
        return Value((int)bladeSliderIndex());
    }
    return Special::valueForKey(key);
}

void BladeWeaponRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "reverse")
    {
        setReverse(kvcBool(value));
    }
    else if (key == "bladeType")
    {
        setBladeType(kvcUnsigned(value));
    }
    else if (key == "bladeSliderIndex")
    {
        setBladeSliderIndex(kvcUnsigned(value));
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
