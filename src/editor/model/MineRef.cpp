#include "MineRef.h"

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 1000d5f00
bool MineRef::init()
{
    if (!Special::initWithSpriteFrameName("e_mine.png"))
    {
        return false;
    }
    _slowMoDuration = 0.0f;
    setLevelItemID(2);
    // Appended to Special's defaults: p5.
    _propertyKeys.push_back("slowMoDuration");
    setShapeCount(2);
    return true;
}

// @ios 1000d5f88
void MineRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000d5fdc
void MineRef::setFixed(const Value& fixed)
{
    KeyValueChange kvo(this, "fixed");
    if (_fixed != kvcBool(fixed))
    {
        postNotification(REF_UI_KEYS_WILL_CHANGE, this);
        Special::setFixed(fixed);
        postNotification(REF_UI_KEYS_CHANGED, this);
    }
}

// @ios 1000d6080
std::vector<std::string> MineRef::propertyKeysForUI()
{
    if (_fixed)
    {
        return {"x", "y", "angle", "fixed", "slowMoDuration"};
    }
    return {"x", "y", "angle", "fixed", "sleeping", "slowMoDuration"};
}

// @ios 1000d6108
InputObject* MineRef::inputObjectForPropertyWithRect(const std::string& property,
                                                    const Rect& rect)
{
    if (property == "slowMoDuration")
    {
        // iOS: label key misspelt "SLOW DURACTION" and the slider always starts at 0.
        return SliderInputObject::create(rect, "SLOW DURACTION", "slowMoDuration", 0.0f, 0.0f,
                                         10.0f, 0);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000d6214
Value MineRef::slowMoDuration()
{
    return Value(_slowMoDuration);
}

// @ios 1000d6230
void MineRef::setSlowMoDuration(const Value& slowMoDuration)
{
    KeyValueChange kvo(this, "slowMoDuration");
    _slowMoDuration = kvcFloat(slowMoDuration);
}

// @ios 1000d6260
void MineRef::createRef()
{
    // textureRect in iOS points.
    const Size size = getTextureRect().size * editorArtScale();
    setRefRect(Rect(0.0f, 0.0f, size.width, size.height));
}

Value MineRef::valueForKey(const std::string& key)
{
    if (key == "slowMoDuration")
    {
        return slowMoDuration();
    }
    return Special::valueForKey(key);
}

void MineRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "slowMoDuration")
    {
        setSlowMoDuration(value);
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
