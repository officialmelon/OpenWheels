#include "JetRef.h"

#include "SliderInputObject.h"
#include "SwitchInputObject.h"

#include <cmath>

USING_NS_CC;

// @ios 10008c020
bool JetRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    _power = 1;
    _mc = Sprite::createWithSpriteFrameName("e_jet.png");
    _mc->setScale(editorArtScale());  // port: art at its iOS point size
    _mc->setPosition(Vec2(0.0f, _ptmRatio * -0.096f));
    addChild(_mc);
    updateSprite();
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(),
                         {"xMeters", "yMeters", "angle", "sleeping", "power", "firingTime",
                          "accelTime", "fixedAngle"});
    setLevelItemID(28);
    return true;
}

// @ios 10008c1b0
Value JetRef::power()
{
    return Value((int)_power);
}

// @ios 10008c1cc
void JetRef::setPower(const Value& power)
{
    KeyValueChange kvo(this, "power");
    _power = (unsigned int)kvcInt(power);
    updateSprite();
}

// @ios 10008c200
void JetRef::updateSprite()
{
    // iOS scales the ref itself (not _mc): 0.5 at power 1 .. 1.0 at power 10.
    setScale(std::fma((float)(_power - 1), 0.0555555560f, 0.5f));
    // textureRect in iOS points.
    float width = _mc->getTextureRect().size.width * editorArtScale();
    float height = _mc->getTextureRect().size.height * editorArtScale();
    setRefRect(Rect(width * -0.5f, height * -0.59375f, width, height));
}

// @ios 10008c294
Value JetRef::firingTime()
{
    return Value((int)_firingTime);
}

// @ios 10008c2b0
void JetRef::setFiringTime(const Value& firingTime)
{
    KeyValueChange kvo(this, "firingTime");
    _firingTime = (unsigned int)kvcInt(firingTime);
}

// @ios 10008c2e0
Value JetRef::accelTime()
{
    return Value((int)_accelTime);
}

// @ios 10008c2fc
void JetRef::setAccelTime(const Value& accelTime)
{
    KeyValueChange kvo(this, "accelTime");
    _accelTime = (unsigned int)kvcInt(accelTime);
}

// @ios 10008c32c
Value JetRef::fixedAngle()
{
    return Value(_fixedAngle);
}

// @ios 10008c348
void JetRef::setFixedAngle(const Value& fixedAngle)
{
    KeyValueChange kvo(this, "fixedAngle");
    _fixedAngle = kvcBool(fixedAngle);
}

// @ios 10008c378
void JetRef::onEnter()
{
    Special::onEnter();
    setShapeCount(1);
}

// @ios 10008c3c4
std::vector<std::string> JetRef::propertyKeysForUI()
{
    return {"x", "y", "angle", "sleeping", "power", "firingTime", "accelTime", "fixedAngle"};
}

// @ios 10008c47c
InputObject* JetRef::inputObjectForPropertyWithRect(const std::string& property,
                                                   const Rect& rect)
{
    if (property == "power")
    {
        return SliderInputObject::create(rect, "POWER", "power", (float)_power, 1.0f, 10.0f, 9);
    }
    if (property == "firingTime")
    {
        return SliderInputObject::create(rect, "FIRING TIME", "firingTime", (float)_firingTime,
                                         0.0f, 50.0f, 50);
    }
    if (property == "accelTime")
    {
        return SliderInputObject::create(rect, "ACCEL TIME", "accelTime", (float)_accelTime,
                                         0.0f, 5.0f, 5);
    }
    if (property == "fixedAngle")
    {
        return SwitchInputObject::create(rect, "FIXED ANGLE", "fixedAngle",
                                         _fixedAngle ? 1.0f : 0.0f);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

Value JetRef::valueForKey(const std::string& key)
{
    if (key == "power")
    {
        return power();
    }
    if (key == "firingTime")
    {
        return firingTime();
    }
    if (key == "accelTime")
    {
        return accelTime();
    }
    if (key == "fixedAngle")
    {
        return fixedAngle();
    }
    return Special::valueForKey(key);
}

void JetRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "power")
    {
        setPower(value);
    }
    else if (key == "firingTime")
    {
        setFiringTime(value);
    }
    else if (key == "accelTime")
    {
        setAccelTime(value);
    }
    else if (key == "fixedAngle")
    {
        setFixedAngle(value);
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
