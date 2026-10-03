#include "BoostPanelRef.h"

#include "SliderInputObject.h"

#include <cmath>

USING_NS_CC;

// @ios 1000c55a0
bool BoostPanelRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    _num = 2;
    setCanDragModify(true);
    setPower(20);
    minNumPanels = 1;
    maxNumPanels = 10;
    setLevelItemID(12);
    segWidth = _ptmRatio * 2.88f;
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "angle", "num", "power"});
    setShapeCount(1);
    return true;
}

// @ios 1000c56e8
void BoostPanelRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000c573c
void BoostPanelRef::setNum(const Value& num)
{
    KeyValueChange kvo(this, "num");
    _num = (unsigned int)kvcInt(num);
    setUpSprites();
}

// @ios 1000c5770
unsigned int BoostPanelRef::num()
{
    return _num;
}

// @ios 1000c5780
std::vector<std::string> BoostPanelRef::propertyKeysForUI()
{
    return {"x", "y", "angle", "num", "power"};
}

// @ios 1000c5818
InputObject* BoostPanelRef::inputObjectForPropertyWithRect(const std::string& property,
                                                          const Rect& rect)
{
    if (property == "num")
    {
        // iOS passes min - max (negative) as the unsigned segment count; kept.
        return SliderInputObject::create(rect, "PANELS", "num", (float)_num,
                                         (float)minNumPanels, (float)maxNumPanels,
                                         minNumPanels - maxNumPanels);
    }
    if (property == "power")
    {
        return SliderInputObject::create(rect, "BOOST POWER", "power", (float)power(), 10.0f,
                                         100.0f, 90);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000c59e0
void BoostPanelRef::setUpSprites()
{
    removeAllChildrenWithCleanup(false);
    float count = 0.0f;
    if (_num != 0)
    {
        // First panel centre: (segWidth - num * segWidth) * 0.5 (fused multiply-subtract).
        float start = std::fma(-segWidth, (float)_num, segWidth) * 0.5f;
        unsigned int i = 0;
        do
        {
            float x = std::fma(segWidth, (float)i, start);
            Sprite* top = Sprite::createWithSpriteFrameName("e_boostpanel.png");
            top->setScale(editorArtScale());  // port: art at its iOS point size
            top->setAnchorPoint(Vec2(0.5f, 0.0f));
            top->setPosition(Vec2(x, 0.0f));
            Sprite* bottom = Sprite::createWithSpriteFrameName("e_boostpanel.png");
            bottom->setAnchorPoint(Vec2(0.5f, 0.0f));
            bottom->setScaleX(editorArtScale());
            bottom->setScaleY(-1.0f * editorArtScale());
            bottom->setPosition(Vec2(x, 0.0f));
            addChild(top);
            addChild(bottom);
            ++i;
        } while (i < _num);
        count = (float)_num;
    }
    float width = segWidth * count;
    setRefRect(Rect(width * -0.5f, std::fma(segWidth, -0.5f, 0.001f), width, segWidth));
}

// @ios 1000c5b3c
void BoostPanelRef::createRef()
{
    setUpSprites();
}

// @ios 1000c5b40
unsigned int BoostPanelRef::power()
{
    return _power;
}

// @ios 1000c5b50
void BoostPanelRef::setPower(unsigned int power)
{
    KeyValueChange kvo(this, "power");
    _power = power;
}

Value BoostPanelRef::valueForKey(const std::string& key)
{
    if (key == "num")
    {
        return Value((int)num());
    }
    if (key == "power")
    {
        return Value((int)power());
    }
    return Special::valueForKey(key);
}

void BoostPanelRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "num")
    {
        setNum(value);
    }
    else if (key == "power")
    {
        setPower(kvcUnsigned(value));
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
