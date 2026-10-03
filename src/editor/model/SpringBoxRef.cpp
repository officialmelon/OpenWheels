#include "SpringBoxRef.h"

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 1000b6024
bool SpringBoxRef::init()
{
    if (!Special::initWithSpriteFrameName("e_springbox.png"))
    {
        return false;
    }
    _delay = 0.0f;
    setLevelItemID(5);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "angle", "delay"});
    setShapeCount(2);
    return true;
}

// @ios 1000b6130
void SpringBoxRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000b6184
Value SpringBoxRef::delay()
{
    return Value(_delay);
}

// @ios 1000b61a0
void SpringBoxRef::setDelay(const Value& delay)
{
    KeyValueChange kvo(this, "delay");
    _delay = kvcFloat(delay);
}

// @ios 1000b61d0
InputObject* SpringBoxRef::inputObjectForPropertyWithRect(const std::string& property,
                                                         const Rect& rect)
{
    if (property == "delay")
    {
        return SliderInputObject::create(rect, "DELAY", "delay", _delay, 0.0f, 2.0f, 4);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000b62e4
void SpringBoxRef::createRef()
{
    // textureRect in iOS points.
    const Size size = getTextureRect().size * editorArtScale();
    setRefRect(Rect(0.0f, 0.0f, size.width, size.height));
}

// @ios 1000b6328
std::vector<std::string> SpringBoxRef::propertyKeysForUI()
{
    return {"x", "y", "angle", "delay"};
}

Value SpringBoxRef::valueForKey(const std::string& key)
{
    if (key == "delay")
    {
        return delay();
    }
    return Special::valueForKey(key);
}

void SpringBoxRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "delay")
    {
        setDelay(value);
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
