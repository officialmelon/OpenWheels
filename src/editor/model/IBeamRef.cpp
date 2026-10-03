#include "IBeamRef.h"

#include "SliderInputObject.h"

#include <cmath>

USING_NS_CC;

namespace
{
// arm64 fcvtmu: floor, then convert to unsigned (negative/NaN -> 0).
unsigned int floorToUnsigned(float v)
{
    float f = std::floor(v);
    if (!(f > 0.0f))
    {
        return 0;
    }
    if (f >= 4294967296.0f)
    {
        return 0xffffffffu;
    }
    return (unsigned int)f;
}
}  // namespace

// @ios 1000ba794
bool IBeamRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanDragModify(true);
    _fixed = true;
    _widthPoints = _ptmRatio * 6.4f;
    _heightPoints = _ptmRatio * 0.512f;
    setLevelItemID(3);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "widthMeters",
                                               "heightMeters", "angle", "fixed", "sleeping"});
    setShapeCount(1);
    return true;
}

// @ios 1000ba920
void IBeamRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000ba974
std::vector<std::string> IBeamRef::propertyKeysForUI()
{
    if (_fixed)
    {
        return {"x", "y", "width", "height", "angle", "fixed"};
    }
    return {"x", "y", "width", "height", "angle", "fixed", "sleeping"};
}

// @ios 1000baa10
void IBeamRef::setFixed(const Value& fixed)
{
    KeyValueChange kvo(this, "fixed");
    if (_fixed != kvcBool(fixed))
    {
        postNotification(REF_UI_KEYS_WILL_CHANGE, this);
        Special::setFixed(fixed);
        postNotification(REF_UI_KEYS_CHANGED, this);
    }
}

// @ios 1000baab4
void IBeamRef::setWidth(float width)
{
    KeyValueChange kvo(this, "width");
    _widthPoints = width;
    setUpSprites();
}

// @ios 1000baac4
float IBeamRef::width()
{
    return _widthPoints;
}

// @ios 1000baae0
void IBeamRef::setHeight(float height)
{
    KeyValueChange kvo(this, "height");
    _heightPoints = height;
    setUpSprites();
}

// @ios 1000baaf0
float IBeamRef::height()
{
    return _heightPoints;
}

// @ios 1000bab0c
void IBeamRef::setWidthMeters(const Value& widthMeters)
{
    KeyValueChange kvo(this, "widthMeters");
    _widthPoints = _ptmRatio * kvcFloat(widthMeters);
}

// @ios 1000bab50
void IBeamRef::setHeightMeters(const Value& heightMeters)
{
    KeyValueChange kvo(this, "heightMeters");
    _heightPoints = _ptmRatio * kvcFloat(heightMeters);
}

// @ios 1000bab94
Value IBeamRef::heightMeters()
{
    return Value(_heightPoints / _ptmRatio);
}

// @ios 1000babc4
Value IBeamRef::widthMeters()
{
    return Value(_widthPoints / _ptmRatio);
}

// @ios 1000babf4
InputObject* IBeamRef::inputObjectForPropertyWithRect(const std::string& property,
                                                     const Rect& rect)
{
    if (property == "width")
    {
        return SliderInputObject::create(rect, "WIDTH", "width", _widthPoints,
                                         _ptmRatio * 3.2f, _ptmRatio * 25.6f, 0);
    }
    if (property == "height")
    {
        return SliderInputObject::create(rect, "HEIGHT", "height", _heightPoints,
                                         _ptmRatio * 0.512f, _ptmRatio * 1.024f, 0);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000badc4
// Tiles "e_ibeam.png" along the width (alternately mirrored, scaleX +-1.005) and adds a
// cropped piece for the remainder; everything is computed in meters, positioned in points.
void IBeamRef::setUpSprites()
{
    removeAllChildrenWithCleanup(false);
    const float widthMeters = _widthPoints / _ptmRatio;
    const float scaleY = _heightPoints * 1.95312488f / _ptmRatio;

    Sprite* probe = Sprite::createWithSpriteFrameName("e_ibeam.png");
    // port: art sizes and scales converted to iOS points with editorArtScale.
    const float art = editorArtScale();
    const float segment =
        (float)((double)(probe->getTextureRect().size.width * art) / (double)_ptmRatio);
    const unsigned int count = floorToUnsigned(widthMeters / segment);
    const float remainder = std::fmod(widthMeters, segment);
    const float halfSegment = segment * 0.5f;

    float x = 0.0f;
    if (count != 0)
    {
        x = std::fma(widthMeters, -0.5f, halfSegment);
        unsigned int i = 0;
        do
        {
            Sprite* piece = Sprite::createWithSpriteFrameName("e_ibeam.png");
            unsigned int n = i + 1;
            int sign = (n & 1) != 0 ? 1 : -1;
            piece->setScaleX((float)sign * 1.005f * art);
            piece->setScaleY(scaleY * art);
            piece->setPosition(Vec2(_ptmRatio * x, 0.0f));
            addChild(piece);
            x = (i < count - 1 ? segment : -0.0f) + x;
            i = n;
        } while (i != count);
    }
    if (remainder != 0.0f)
    {
        float pieceX = count == 0 ? remainder * -0.5f : x + halfSegment;
        Sprite* piece = Sprite::createWithSpriteFrameName("e_ibeam.png");
        piece->setPosition(Vec2(_ptmRatio * pieceX, 0.0f));
        piece->setAnchorPoint(Vec2(0.0f, 0.5f));
        piece->setScaleX(art);
        piece->setScaleY(scaleY * art);
        Rect texture = piece->getTextureRect();
        bool rotated = piece->isTextureRectRotated();
        // iOS crops to ptm * remainder points; the texture rect is in art units.
        float cropWidth = _ptmRatio * remainder / art;
        piece->setTextureRect(Rect(texture.origin.x, texture.origin.y, cropWidth,
                                   texture.size.height),
                              rotated, Size(cropWidth, texture.size.height));
        addChild(piece);
    }
    setRefRect(Rect(_widthPoints * -0.5f, _heightPoints * -0.5f, _widthPoints, _heightPoints));
}

// @ios 1000bb044
void IBeamRef::createRef()
{
    setUpSprites();
}

Value IBeamRef::valueForKey(const std::string& key)
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
    return Special::valueForKey(key);
}

void IBeamRef::setValueForKey(const Value& value, const std::string& key)
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
    else
    {
        Special::setValueForKey(value, key);
    }
}
