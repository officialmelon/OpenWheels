#include "LogRef.h"

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 1000f4130
bool LogRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    _logSprite = Sprite::createWithSpriteFrameName("e_log.png");
    _logSprite->setPosition(Vec2(0.5f, 0.5f));
    addChild(_logSprite);
    setCanDragModify(false);
    _fixed = false;
    _strength = 5;
    _widthPoints = _ptmRatio * 0.576f;
    _heightPoints = _ptmRatio * 6.4f;
    setLevelItemID(4);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(),
                         {"xMeters", "yMeters", "widthMeters", "heightMeters", "angle", "fixed",
                          "sleeping", "strength"});
    setShapeCount(2);
    setUpSprites();
    return true;
}

// @ios 1000f42d8
void LogRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000f432c
void LogRef::setStrength(const Value& strength)
{
    KeyValueChange kvo(this, "strength");
    _strength = (unsigned int)kvcInt(strength);
}

// @ios 1000f435c
void LogRef::setFixed(const Value& fixed)
{
    KeyValueChange kvo(this, "fixed");
    if (_fixed != kvcBool(fixed))
    {
        postNotification(REF_UI_KEYS_WILL_CHANGE, this);
        Special::setFixed(fixed);
        postNotification(REF_UI_KEYS_CHANGED, this);
    }
}

// @ios 1000f4400
Value LogRef::strength()
{
    return Value((int)_strength);
}

// @ios 1000f441c
std::vector<std::string> LogRef::propertyKeysForUI()
{
    if (_fixed)
    {
        return {"x", "y", "width", "height", "angle", "fixed"};
    }
    return {"x", "y", "width", "height", "angle", "fixed", "sleeping"};
}

// @ios 1000f44ac
InputObject* LogRef::inputObjectForPropertyWithRect(const std::string& property,
                                                   const Rect& rect)
{
    if (property == "strength")
    {
        // Unreachable on iOS (strength is not a UI key): literal, unlocalised label and an
        // empty 10..10 range - kept as is.
        return SliderInputObject::create(rect, "strength", "strength", (float)_strength, 10.0f,
                                         10.0f, 9);
    }
    if (property == "width")
    {
        return SliderInputObject::create(rect, "WIDTH", "width", _widthPoints,
                                         _ptmRatio * 0.576f, _ptmRatio * 0.864f, 0);
    }
    if (property == "height")
    {
        return SliderInputObject::create(rect, "HEIGHT", "height", _heightPoints,
                                         _ptmRatio * 3.2f, _ptmRatio * 9.6f, 0);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000f46d8
void LogRef::setWidth(float width)
{
    KeyValueChange kvo(this, "width");
    _widthPoints = width;
    setUpSprites();
}

// @ios 1000f46e8
float LogRef::width()
{
    return _widthPoints;
}

// @ios 1000f4704
void LogRef::setHeight(float height)
{
    KeyValueChange kvo(this, "height");
    _heightPoints = height;
    setUpSprites();
}

// @ios 1000f4714
void LogRef::setUpSprites()
{
    float scaleY = _heightPoints * 0.15625f / _ptmRatio;
    float scaleX = _widthPoints * 1.73611116f / _ptmRatio;
    // port: iOS scales are relative to the art's point size.
    _logSprite->setScaleY(scaleY * editorArtScale());
    _logSprite->setScaleX(scaleX * editorArtScale());
    setRefRect(Rect(_widthPoints * -0.5f, _heightPoints * -0.5f, _widthPoints, _heightPoints));
}

// @ios 1000f47cc
float LogRef::height()
{
    return _heightPoints;
}

// @ios 1000f47e8
void LogRef::setWidthMeters(const Value& widthMeters)
{
    KeyValueChange kvo(this, "widthMeters");
    _widthPoints = _ptmRatio * kvcFloat(widthMeters);
}

// @ios 1000f482c
void LogRef::setHeightMeters(const Value& heightMeters)
{
    KeyValueChange kvo(this, "heightMeters");
    _heightPoints = _ptmRatio * kvcFloat(heightMeters);
}

// @ios 1000f4870
Value LogRef::heightMeters()
{
    return Value(_heightPoints / _ptmRatio);
}

// @ios 1000f48a0
Value LogRef::widthMeters()
{
    return Value(_widthPoints / _ptmRatio);
}

// @ios 1000f48d0
void LogRef::createRef()
{
    // textureRect in iOS points.
    const Size size = getTextureRect().size * editorArtScale();
    setRefRect(Rect(0.0f, 0.001f, size.width, size.height));
    setUpSprites();
}

Value LogRef::valueForKey(const std::string& key)
{
    if (key == "width")
    {
        return Value(width());
    }
    if (key == "height")
    {
        return Value(height());
    }
    if (key == "widthMeters")
    {
        return widthMeters();
    }
    if (key == "heightMeters")
    {
        return heightMeters();
    }
    if (key == "strength")
    {
        return strength();
    }
    return Special::valueForKey(key);
}

void LogRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "width")
    {
        setWidth(kvcFloat(value));
    }
    else if (key == "height")
    {
        setHeight(kvcFloat(value));
    }
    else if (key == "widthMeters")
    {
        setWidthMeters(value);
    }
    else if (key == "heightMeters")
    {
        setHeightMeters(value);
    }
    else if (key == "strength")
    {
        setStrength(value);
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
