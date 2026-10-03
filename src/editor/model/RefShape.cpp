#include "RefShape.h"

#include "ColorInputObject.h"
#include "InputObject.h"
#include "SliderInputObject.h"

USING_NS_CC;

namespace {

// iOS file statics @101bc32e0..ec: colour and opacity of the next new shape ("last used"),
// written by setInnerRed/Green/Blue: and setShapeOpacity: (not by setColor:).
unsigned int s_innerRed = 61;        // DAT_101bc32e0
unsigned int s_innerGreen = 136;     // DAT_101bc32e4
unsigned int s_innerBlue = 199;      // DAT_101bc32e8
unsigned int s_shapeOpacity = 100;   // DAT_101bc32ec

const float kByteToUnit = 0.003921569f;  // 1/255 (0x3b808081)
const float kPercentToUnit = 0.01f;      // 0x3c23d70a

}  // namespace

RefShape::RefShape()
    : _shapeOpacityWorkaround(0)
    , _collision(0)
    , _widthMin(0.0f)
    , _widthMax(0.0f)
    , _heightMin(0.0f)
    , _heightMax(0.0f)
    , _widthMeters(0.0f)
    , _heightMeters(0.0f)
    , _density(0.0f)
    , _innerRed(0)
    , _innerGreen(0)
    , _innerBlue(0)
    , _outlineColor(0.0f)
    , _innerColor(0.0f, 0.0f, 0.0f, 0.0f)
{
}

// @ios 1000da374
bool RefShape::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    _innerRed = s_innerRed;
    _innerGreen = s_innerGreen;
    _innerBlue = s_innerBlue;
    _shapeOpacityWorkaround = s_shapeOpacity;
    _innerColor.r = (float)_innerRed * kByteToUnit;
    _innerColor.g = (float)_innerGreen * kByteToUnit;
    _innerColor.b = (float)_innerBlue * kByteToUnit;
    _innerColor.a = (float)_shapeOpacityWorkaround * kPercentToUnit;
    setOutlineColor(-1.0f);
    setDensity(1.0f);
    setCollision(1);
    _fixed = true;
    _interactive = true;
    _sleeping = false;
    propertyKeys().clear();
    propertyKeys().insert(propertyKeys().end(),
                          {"xMeters", "yMeters", "widthMeters", "heightMeters", "angle", "fixed",
                           "sleeping", "density", "color", "outlineColor", "shapeOpacity",
                           "collision"});
    setShapeCount(1);
    setArtCount(0);
    return true;
}

// @ios 1000da5a0
void RefShape::setProperties(const ValueMap& properties)
{
    auto interactive = properties.find("i");
    _interactive = interactive == properties.end() ? true : kvcBool(interactive->second);

    auto p12 = properties.find("p12");
    if (p12 == properties.end())
    {
        Special::setProperties(properties);
        return;
    }
    // Older 13-property shapes: the last value moves to p11.
    ValueMap upgraded(properties);
    upgraded["p11"] = p12->second;
    upgraded.erase("p12");
    Special::setProperties(upgraded);
}

// @ios 1000da698
void RefShape::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000da6ec
void RefShape::setShapeOpacity(unsigned int shapeOpacity)
{
    KeyValueChange kvo(this, "shapeOpacity");
    _innerColor.a = (float)shapeOpacity * kPercentToUnit;
    s_shapeOpacity = shapeOpacity;
    _shapeOpacityWorkaround = shapeOpacity;
}

// @ios 1000da728
unsigned int RefShape::shapeOpacity()
{
    return _shapeOpacityWorkaround;
}

// @ios 1000da738
void RefShape::setWidthMeters(float widthMeters)
{
    KeyValueChange kvo(this, "widthMeters");
    _widthMeters = widthMeters;
    updateRefRect();
}

// @ios 1000da748
void RefShape::setHeightMeters(float heightMeters)
{
    KeyValueChange kvo(this, "heightMeters");
    _heightMeters = heightMeters;
    updateRefRect();
}

// @ios 1000da758
void RefShape::setInteractive(const Value& interactive)
{
    KeyValueChange kvo(this, "interactive");
    const bool value = kvcBool(interactive);
    if (_interactive != value)
    {
        postNotification(REF_UI_KEYS_WILL_CHANGE, this);
        Special::setInteractive(interactive);
        postNotification(REF_UI_KEYS_CHANGED, this);
        setShapeCount(value ? 1u : 0u);
        setArtCount(value ? 0u : 1u);
    }
}

// @ios 1000da81c
float RefShape::width()
{
    return _ptmRatio * widthMeters();
}

// @ios 1000da850
float RefShape::height()
{
    return _ptmRatio * heightMeters();
}

// @ios 1000da884
void RefShape::setWidth(float width)
{
    KeyValueChange kvo(this, "width");
    setWidthMeters(width / _ptmRatio);
}

// @ios 1000da89c
void RefShape::setHeight(float height)
{
    KeyValueChange kvo(this, "height");
    setHeightMeters(height / _ptmRatio);
}

// @ios 1000da8b4
void RefShape::setInnerRed(unsigned int innerRed)
{
    KeyValueChange kvo(this, "innerRed");
    _innerRed = innerRed;
    s_innerRed = innerRed;
    _innerColor.r = (float)innerRed * kByteToUnit;
}

// @ios 1000da8ec
void RefShape::setInnerGreen(unsigned int innerGreen)
{
    KeyValueChange kvo(this, "innerGreen");
    _innerGreen = innerGreen;
    s_innerGreen = innerGreen;
    _innerColor.g = (float)innerGreen * kByteToUnit;
}

// @ios 1000da928
void RefShape::setInnerBlue(unsigned int innerBlue)
{
    KeyValueChange kvo(this, "innerBlue");
    _innerBlue = innerBlue;
    s_innerBlue = innerBlue;
    _innerColor.b = (float)innerBlue * kByteToUnit;
}

namespace {
// fcvtzu
unsigned int toUnsigned(float f)
{
    return f > 0.0f ? (unsigned int)f : 0u;
}
}  // namespace

// @ios 1000da964
void RefShape::setColor(unsigned int color)
{
    KeyValueChange kvo(this, "color");
    const Color4F c = ccColorFromRGB((long long)color);
    _innerRed = toUnsigned(c.r * 255.0f);
    _innerGreen = toUnsigned(c.g * 255.0f);
    _innerBlue = toUnsigned(c.b * 255.0f);
    _innerColor.r = c.r;
    _innerColor.g = c.g;
    _innerColor.b = c.b;
}

// @ios 1000da9e0
unsigned int RefShape::color()
{
    return (unsigned int)hexFromColor(_innerColor);
}

// @ios 1000da9f8
int RefShape::hexFromColor(const Color4F& color)
{
    // Float maths throughout; every byte 0..255 survives x*(1/255)*255 truncation exactly.
    const int r = (int)(color.r * 255.0f);
    const int g = (int)(color.g * 255.0f);
    const int b = (int)(color.b * 255.0f);
    return b + g * 0x100 + r * 0x10000;
}

// @ios 1000daa24
Color4F RefShape::ccColorFromRGB(long long rgb)
{
    return Color4F((float)((rgb >> 16) & 0xff) * kByteToUnit,
                   (float)((rgb >> 8) & 0xff) * kByteToUnit,
                   (float)(rgb & 0xff) * kByteToUnit, 1.0f);
}

// @ios 1000daa5c
std::vector<std::string> RefShape::propertyKeysForUI()
{
    if (!interactive())
    {
        return {"x", "y", "width", "height", "angle", "innerRed", "innerGreen", "innerBlue",
                "shapeOpacity", "interactive", "collision"};
    }
    return {"x", "y", "width", "height", "angle", "innerRed", "innerGreen", "innerBlue",
            "shapeOpacity", "interactive", "fixed", "sleeping", "density", "collision"};
}

// @ios 1000dabd4
InputObject* RefShape::inputObjectForPropertyWithRect(const std::string& property,
                                                      const Rect& rect)
{
    if (property == "width")
    {
        return InputObject::create(rect, "WIDTH", "width", width(), true);
    }
    if (property == "height")
    {
        return InputObject::create(rect, "HEIGHT", "height", height(), true);
    }
    if (property == "innerRed")
    {
        return ColorInputObject::create(rect, "COLOR", "r", "innerRed", (float)_innerRed, 0.0f,
                                        255.0f, 254);
    }
    if (property == "innerGreen")
    {
        return ColorInputObject::create(rect, "", "g", "innerGreen", (float)_innerGreen, 0.0f,
                                        255.0f, 254);
    }
    if (property == "innerBlue")
    {
        return ColorInputObject::create(rect, "", "b", "innerBlue", (float)_innerBlue, 0.0f,
                                        255.0f, 254);
    }
    if (property == "shapeOpacity")
    {
        return ColorInputObject::create(rect, "", "a", "shapeOpacity", (float)shapeOpacity(),
                                        0.0f, 100.0f, 99);
    }
    if (property == "collision")
    {
        return SliderInputObject::create(rect, "COLLISION", "collision", (float)collision(), 1.0f,
                                         6.0f, 5);
    }
    if (property == "density")
    {
        InputObject* input = InputObject::create(rect, "DENSITY", "density", density(), true);
        if (input)
        {
            input->setMinValue(0.1f, 100.0f);
        }
        return input;
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000db034
void RefShape::setCollision(unsigned int collision)
{
    KeyValueChange kvo(this, "collision");
    _collision = collision;
}

// @ios 1000db044
unsigned int RefShape::collision()
{
    return _collision;
}

// @ios 1000db054
void RefShape::updateRefRect()
{
}

// @ios 1000db058
void RefShape::updateDrawingWithNode(DrawNode* node)
{
}

// @ios 1000db05c
float RefShape::widthMin()
{
    return _widthMin;
}

// @ios 1000db06c
void RefShape::setWidthMin(float widthMin)
{
    KeyValueChange kvo(this, "widthMin");
    _widthMin = widthMin;
}

// @ios 1000db07c
float RefShape::widthMax()
{
    return _widthMax;
}

// @ios 1000db08c
void RefShape::setWidthMax(float widthMax)
{
    KeyValueChange kvo(this, "widthMax");
    _widthMax = widthMax;
}

// @ios 1000db09c
float RefShape::heightMin()
{
    return _heightMin;
}

// @ios 1000db0ac
void RefShape::setHeightMin(float heightMin)
{
    KeyValueChange kvo(this, "heightMin");
    _heightMin = heightMin;
}

// @ios 1000db0bc
float RefShape::heightMax()
{
    return _heightMax;
}

// @ios 1000db0cc
void RefShape::setHeightMax(float heightMax)
{
    KeyValueChange kvo(this, "heightMax");
    _heightMax = heightMax;
}

// @ios 1000db0dc
float RefShape::widthMeters()
{
    return _widthMeters;
}

// @ios 1000db0ec
float RefShape::heightMeters()
{
    return _heightMeters;
}

// @ios 1000db0fc
float RefShape::density()
{
    return _density;
}

// @ios 1000db10c
void RefShape::setDensity(float density)
{
    KeyValueChange kvo(this, "density");
    _density = density;
}

// @ios 1000db11c
unsigned int RefShape::innerRed()
{
    return _innerRed;
}

// @ios 1000db12c
unsigned int RefShape::innerGreen()
{
    return _innerGreen;
}

// @ios 1000db13c
unsigned int RefShape::innerBlue()
{
    return _innerBlue;
}

// @ios 1000db14c
float RefShape::outlineColor()
{
    return _outlineColor;
}

// @ios 1000db15c
void RefShape::setOutlineColor(float outlineColor)
{
    KeyValueChange kvo(this, "outlineColor");
    _outlineColor = outlineColor;
}

// @ios 1000db16c
Color4F RefShape::innerColor()
{
    return _innerColor;
}

// @ios 1000db184
void RefShape::setInnerColor(const Color4F& innerColor)
{
    KeyValueChange kvo(this, "innerColor");
    _innerColor = innerColor;
}

// ---- KVC (port) -----------------------------------------------------------------------------------

Value RefShape::valueForKey(const std::string& key)
{
    if (key == "widthMeters") return Value(widthMeters());
    if (key == "heightMeters") return Value(heightMeters());
    if (key == "density") return Value(density());
    if (key == "color") return Value(color());
    if (key == "outlineColor") return Value(outlineColor());
    if (key == "shapeOpacity") return Value(shapeOpacity());
    if (key == "collision") return Value(collision());
    if (key == "innerRed") return Value(innerRed());
    if (key == "innerGreen") return Value(innerGreen());
    if (key == "innerBlue") return Value(innerBlue());
    if (key == "widthMin") return Value(widthMin());
    if (key == "widthMax") return Value(widthMax());
    if (key == "heightMin") return Value(heightMin());
    if (key == "heightMax") return Value(heightMax());
    return Special::valueForKey(key);
}

void RefShape::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "widthMeters") { setWidthMeters(kvcFloat(value)); return; }
    if (key == "heightMeters") { setHeightMeters(kvcFloat(value)); return; }
    if (key == "density") { setDensity(kvcFloat(value)); return; }
    if (key == "color") { setColor(kvcUnsigned(value)); return; }
    if (key == "outlineColor") { setOutlineColor(kvcFloat(value)); return; }
    if (key == "shapeOpacity") { setShapeOpacity(kvcUnsigned(value)); return; }
    if (key == "collision") { setCollision(kvcUnsigned(value)); return; }
    if (key == "innerRed") { setInnerRed(kvcUnsigned(value)); return; }
    if (key == "innerGreen") { setInnerGreen(kvcUnsigned(value)); return; }
    if (key == "innerBlue") { setInnerBlue(kvcUnsigned(value)); return; }
    if (key == "widthMin") { setWidthMin(kvcFloat(value)); return; }
    if (key == "widthMax") { setWidthMax(kvcFloat(value)); return; }
    if (key == "heightMin") { setHeightMin(kvcFloat(value)); return; }
    if (key == "heightMax") { setHeightMax(kvcFloat(value)); return; }
    Special::setValueForKey(value, key);
}
