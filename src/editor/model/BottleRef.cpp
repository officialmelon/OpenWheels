#include "BottleRef.h"

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 100089064
bool BottleRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    setLevelItemID(20);
    setBottleType(1);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "angle", "bottleType",
                                               "sleeping", "interactive"});
    return true;
}

// @ios 100089198
InputObject* BottleRef::inputObjectForPropertyWithRect(const std::string& property,
                                                      const Rect& rect)
{
    if (property == "bottleType")
    {
        return SliderInputObject::create(rect, "BOTTLE TYPE", "bottleType", (float)bottleType(),
                                         1.0f, 4.0f, 3);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000892bc
std::vector<std::string> BottleRef::propertyKeysForUI()
{
    return {"xMeters", "yMeters", "angle", "bottleType", "sleeping", "interactive"};
}

// @ios 100089364
void BottleRef::setBottleType(unsigned int bottleType)
{
    KeyValueChange kvo(this, "bottleType");
    if (bottleType > 3)
    {
        bottleType = 4;
    }
    if (bottleType < 2)
    {
        bottleType = 1;
    }
    _bottleType = bottleType;
    removeAllChildren();
    Sprite* bottle = Sprite::createWithSpriteFrameName(
        StringUtils::format("e_bottle_%i.png", (int)bottleType));
    bottle->setScale(editorArtScale());  // port: art at its iOS point size
    addChild(bottle);
    // contentSize in iOS points.
    Size size = bottle->getContentSize() * editorArtScale();
    setRefRect(Rect(size.width * -0.5f, size.height * -0.5f, size.width, size.height));
}

// @ios 100089414
unsigned int BottleRef::bottleType()
{
    return _bottleType;
}

Value BottleRef::valueForKey(const std::string& key)
{
    if (key == "bottleType")
    {
        return Value((int)bottleType());
    }
    return Special::valueForKey(key);
}

void BottleRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "bottleType")
    {
        setBottleType(kvcUnsigned(value));
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
