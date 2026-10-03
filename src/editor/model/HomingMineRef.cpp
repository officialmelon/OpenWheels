#include "HomingMineRef.h"

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 1000dc934
bool HomingMineRef::init()
{
    if (!Special::initWithSpriteFrameName("e_homingMine.png"))
    {
        return false;
    }
    setCanRotate(true);
    setLevelItemID(25);
    _speed = 1;
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "speed", "delay"});
    return true;
}

// @ios 1000dca38
void HomingMineRef::setRotation(float /*rotation*/)
{
}

// @ios 1000dca3c
std::vector<std::string> HomingMineRef::propertyKeysForUI()
{
    return {"x", "y", "speed", "delay"};
}

// @ios 1000dcac4
void HomingMineRef::createRef()
{
}

// @ios 1000dcac8
InputObject* HomingMineRef::inputObjectForPropertyWithRect(const std::string& property,
                                                          const Rect& rect)
{
    if (property == "speed")
    {
        return SliderInputObject::create(rect, "SPEED", "speed", (float)_speed, 1.0f, 10.0f, 9);
    }
    if (property == "delay")
    {
        return SliderInputObject::create(rect, "DELAY", "delay", (float)_delay, 0.0f, 5.0f, 5);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000dcc68
unsigned int HomingMineRef::speed()
{
    return _speed;
}

// @ios 1000dcc78
void HomingMineRef::setSpeed(unsigned int speed)
{
    KeyValueChange kvo(this, "speed");
    _speed = speed;
}

// @ios 1000dcc88
unsigned int HomingMineRef::delay()
{
    return _delay;
}

// @ios 1000dcc98
void HomingMineRef::setDelay(unsigned int delay)
{
    KeyValueChange kvo(this, "delay");
    _delay = delay;
}

Value HomingMineRef::valueForKey(const std::string& key)
{
    if (key == "speed")
    {
        return Value((int)speed());
    }
    if (key == "delay")
    {
        return Value((int)delay());
    }
    return Special::valueForKey(key);
}

void HomingMineRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "speed")
    {
        setSpeed(kvcUnsigned(value));
    }
    else if (key == "delay")
    {
        setDelay(kvcUnsigned(value));
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
