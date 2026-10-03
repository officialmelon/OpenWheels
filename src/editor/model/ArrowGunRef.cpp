#include "ArrowGunRef.h"

#include "SliderInputObject.h"

#include <cmath>

USING_NS_CC;

// @ios 10003fc08
bool ArrowGunRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    _fixed = true;
    setShapeCount(7);
    _rateOfFire = 5;

    Sprite* gun = Sprite::createWithSpriteFrameName("e_arrow_gun.png");
    gun->setScale(editorArtScale());  // port: art at its iOS point size
    gun->setAnchorPoint(Vec2(0.5f, 0.0f));
    float gunY = std::fma(_ptmRatio, -0.128f, 0.5f);
    gun->setPosition(Vec2(0.5f, gunY));
    addChild(gun);

    setLevelItemID(29);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "angle", "fixed",
                                               "rateOfFire", "dontShootPlayer"});

    // textureRect in iOS points (port: art units * editorArtScale).
    const float width = gun->getTextureRect().size.width * editorArtScale();
    const float height = gun->getTextureRect().size.height * editorArtScale();
    setRefRect(Rect(width * -0.5f, gunY, width, height));
    return true;
}

// @ios 10003fdfc
std::vector<std::string> ArrowGunRef::propertyKeysForUI()
{
    return {"x", "y", "angle", "fixed", "rateOfFire"};
}

// @ios 10003fe98
InputObject* ArrowGunRef::inputObjectForPropertyWithRect(const std::string& property,
                                                        const Rect& rect)
{
    if (property == "rateOfFire")
    {
        return SliderInputObject::create(rect, "RATE OF FIRE", "rateOfFire",
                                         (float)rateOfFire(), 1.0f, 10.0f, 9);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 10003ffbc
void ArrowGunRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 100040010
unsigned int ArrowGunRef::rateOfFire()
{
    return _rateOfFire;
}

// @ios 100040020
void ArrowGunRef::setRateOfFire(unsigned int rateOfFire)
{
    KeyValueChange kvo(this, "rateOfFire");
    _rateOfFire = rateOfFire;
}

// @ios 100040030
bool ArrowGunRef::dontShootPlayer()
{
    return _dontShootPlayer;
}

// @ios 100040040
void ArrowGunRef::setDontShootPlayer(bool dontShootPlayer)
{
    KeyValueChange kvo(this, "dontShootPlayer");
    _dontShootPlayer = dontShootPlayer;
}

Value ArrowGunRef::valueForKey(const std::string& key)
{
    if (key == "rateOfFire")
    {
        return Value((int)rateOfFire());
    }
    if (key == "dontShootPlayer")
    {
        return Value(dontShootPlayer());
    }
    return Special::valueForKey(key);
}

void ArrowGunRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "rateOfFire")
    {
        setRateOfFire(kvcUnsigned(value));
    }
    else if (key == "dontShootPlayer")
    {
        setDontShootPlayer(kvcBool(value));
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
