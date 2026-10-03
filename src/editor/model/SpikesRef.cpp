#include "SpikesRef.h"

#include "SliderInputObject.h"

#include <cmath>

USING_NS_CC;

// @ios 1000b4258
bool SpikesRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanDragModify(true);
    numSpikes = 20;
    minNumSpikes = 20;
    maxNumSpikes = 150;
    _fixed = true;
    _sleeping = false;
    _spikeWidth = _ptmRatio * 0.24f;
    setLevelItemID(6);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(),
                         {"xMeters", "yMeters", "angle", "fixed", "num", "sleeping"});
    setShapeCount(2);
    return true;
}

// @ios 1000b43f0
void SpikesRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000b4444
void SpikesRef::setNum(const Value& num)
{
    KeyValueChange kvo(this, "num");
    numSpikes = (unsigned int)kvcInt(num);
    setUpSprites();
}

// @ios 1000b4478
unsigned int SpikesRef::num()
{
    return numSpikes;
}

// @ios 1000b4488
void SpikesRef::setFixed(const Value& fixed)
{
    KeyValueChange kvo(this, "fixed");
    if (_fixed != kvcBool(fixed))
    {
        postNotification(REF_UI_KEYS_WILL_CHANGE, this);
        Special::setFixed(fixed);
        postNotification(REF_UI_KEYS_CHANGED, this);
    }
}

// @ios 1000b452c
std::vector<std::string> SpikesRef::propertyKeysForUI()
{
    std::vector<std::string> keys = {"x", "y", "angle", "fixed", "num"};
    if (!_fixed)
    {
        keys.push_back("sleeping");
    }
    return keys;
}

// @ios 1000b45c4
InputObject* SpikesRef::inputObjectForPropertyWithRect(const std::string& property,
                                                      const Rect& rect)
{
    if (property == "num")
    {
        return SliderInputObject::create(rect, "SPIKES", "num", (float)numSpikes,
                                         (float)minNumSpikes, (float)maxNumSpikes,
                                         maxNumSpikes - minNumSpikes);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000b46f4
void SpikesRef::setUpSprites()
{
    removeAllChildrenWithCleanup(false);
    // Measuring sprite (never added).
    Sprite* probe = Sprite::createWithSpriteFrameName("e_spike.png");
    const float y = _ptmRatio * -0.16f;
    float count = 0.0f;
    if (numSpikes != 0)
    {
        float start = std::fma(-_spikeWidth, (float)numSpikes, _spikeWidth) * 0.5f;
        unsigned int i = 0;
        do
        {
            Sprite* spike = Sprite::createWithSpriteFrameName("e_spike.png");
            spike->setPosition(Vec2(std::fma(_spikeWidth, (float)i, start), y));
            spike->setScaleY(editorArtScale());  // port: art at its iOS point size
            spike->setScaleX(1.05f * editorArtScale());
            addChild(spike);
            ++i;
        } while (i < numSpikes);
        count = (float)numSpikes;
    }
    float width = _spikeWidth * count;
    float height = probe->getTextureRect().size.height * editorArtScale();  // iOS points
    setRefRect(Rect(width * -0.5f, std::fma(height, -0.5f, y), width, height));
}

// @ios 1000b4854
void SpikesRef::createRef()
{
    setUpSprites();
}

Value SpikesRef::valueForKey(const std::string& key)
{
    if (key == "num")
    {
        return Value((int)num());
    }
    return Special::valueForKey(key);
}

void SpikesRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "num")
    {
        setNum(value);
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
