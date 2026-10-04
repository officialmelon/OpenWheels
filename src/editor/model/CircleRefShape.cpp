#include "CircleRefShape.h"

#include <algorithm>

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 1000b3d40
bool CircleRefShape::init()
{
    if (!RefShape::init())
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    const float diameter = _ptmRatio * 3.2f;
    setHeight(diameter);
    setWidth(diameter);
    setLevelItemID(6001);
    return true;
}

// @ios 1000b3dec
float CircleRefShape::innerCutout()
{
    return _innerCutout;  // EDITOR (browser features, PC addition): iOS returns 0
}

// @ios 1000b3df4
void CircleRefShape::setInnerCutout(float innerCutout)
{
    KeyValueChange kvo(this, "innerCutout");
    // EDITOR (browser features, PC addition): kept for browser levels (Flash CircleShape p12).
    _innerCutout = std::max(0.0f, std::min(100.0f, innerCutout));
}

// @ios 1000b3df8
void CircleRefShape::setProperties(const ValueMap& properties)
{
    log("properties: %s", Value(properties).getDescription().c_str());
    RefShape::setProperties(properties);
}

// @ios 1000b3e50
std::vector<std::string> CircleRefShape::propertyKeysForUI()
{
    std::vector<std::string> keys = RefShape::propertyKeysForUI();
    // -[NSMutableArray removeObject:] removes every occurrence.
    keys.erase(std::remove(keys.begin(), keys.end(), "height"), keys.end());
    keys.erase(std::remove(keys.begin(), keys.end(), "angle"), keys.end());
    return keys;
}

// @ios 1000b3ec8
InputObject* CircleRefShape::inputObjectForPropertyWithRect(const std::string& property,
                                                            const Rect& rect)
{
    if (property == "width")
    {
        return SliderInputObject::create(rect, "DIAMETER", "width", width(), _ptmRatio * 0.08f,
                                         _ptmRatio * 80.0f, 0);
    }
    return RefShape::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000b4010
void CircleRefShape::updateDrawingWithNode(DrawNode* node)
{
    const Vec2 position = getPosition();
    const float radius = width() * 0.5f;
    node->drawDot(position, radius, innerColor());
    if (_innerCutout > 0.0f)  // EDITOR (browser features, PC addition): show the cutout
    {
        node->drawDot(position, radius * _innerCutout * 0.01f, Color4F(1.0f, 1.0f, 1.0f, 0.85f));
    }
}

// @ios 1000b4098
void CircleRefShape::updateRefRect()
{
    const float x = width() * -0.5f;
    const float y = height() * -0.5f;
    const float w = width();
    const float h = height();
    setRefRect(cg::Rect((double)x, (double)y, (double)w, (double)h));
}

// @ios 1000b410c
cg::Rect CircleRefShape::refBoundingBox()
{
    const double x = (double)getPosition().x - (double)(width() * 0.5f);
    const double y = (double)getPosition().y - (double)(width() * 0.5f);
    const double w = (double)width();
    const double h = (double)width();
    return cg::Rect(x, y, w, h);
}

// ---- KVC (port) -----------------------------------------------------------------------------------

Value CircleRefShape::valueForKey(const std::string& key)
{
    if (key == "innerCutout") return Value(innerCutout());
    return RefShape::valueForKey(key);
}

void CircleRefShape::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "innerCutout") { setInnerCutout(kvcFloat(value)); return; }
    RefShape::setValueForKey(value, key);
}
