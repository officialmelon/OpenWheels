#include "SlowMotionPanelRef.h"

#include "SliderInputObject.h"

#include <cmath>

USING_NS_CC;

// @ios 1000ded04
bool SlowMotionPanelRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    on = false;
    _duration = 5.0f;
    _panelHeightMeters = 2.0f;
    _panelWidthMeters = 2.0f;
    setLevelItemID(5001);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "angle", "duration",
                                               "panelWidthMeters", "panelHeightMeters"});
    setShapeCount(1);
    return true;
}

// @ios 1000dee50
void SlowMotionPanelRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000deea4
void SlowMotionPanelRef::setPanelWidthMeters(const Value& panelWidthMeters)
{
    KeyValueChange kvo(this, "panelWidthMeters");
    _panelWidthMeters = kvcFloat(panelWidthMeters);
    updateRefRect();
}

// @ios 1000deed8
float SlowMotionPanelRef::panelWidthMeters()
{
    return _panelWidthMeters;
}

// @ios 1000deee8
void SlowMotionPanelRef::setPanelHeightMeters(const Value& panelHeightMeters)
{
    KeyValueChange kvo(this, "panelHeightMeters");
    _panelHeightMeters = kvcFloat(panelHeightMeters);
    updateRefRect();
}

// @ios 1000def1c
float SlowMotionPanelRef::panelHeightMeters()
{
    return _panelHeightMeters;
}

// @ios 1000def2c
void SlowMotionPanelRef::updateRefRect()
{
    float width = _ptmRatio * _panelWidthMeters;
    float height = _panelHeightMeters * _ptmRatio;
    setRefRect(Rect(width * -0.5f, height * -0.5f, width, height));
}

// @ios 1000def7c
void SlowMotionPanelRef::setDuration(const Value& duration)
{
    KeyValueChange kvo(this, "duration");
    _duration = kvcFloat(duration);
}

// @ios 1000defac
float SlowMotionPanelRef::duration()
{
    return _duration;
}

// @ios 1000defbc
InputObject* SlowMotionPanelRef::inputObjectForPropertyWithRect(const std::string& property,
                                                               const Rect& rect)
{
    if (property == "duration")
    {
        return SliderInputObject::create(rect, "DURATION", "duration", _duration, 1.0f, 10.0f, 0);
    }
    if (property == "panelWidthMeters")
    {
        return SliderInputObject::create(rect, "PANEL WIDTH", "panelWidthMeters",
                                         _panelWidthMeters, 2.0f, 5.0f, 0);
    }
    if (property == "panelHeightMeters")
    {
        return SliderInputObject::create(rect, "PANEL HEIGHT", "panelHeightMeters",
                                         _panelHeightMeters, 2.0f, 5.0f, 0);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000df1a0
void SlowMotionPanelRef::createRef()
{
    Sprite* right = Sprite::createWithSpriteFrameName("e_slowmotionpanel.png");
    right->setAnchorPoint(Vec2(1.0f, 0.5f));
    right->setScaleY(editorArtScale());  // port: art at its iOS point size
    right->setScaleX(1.01f * editorArtScale());
    addChild(right);
    Sprite* left = Sprite::createWithSpriteFrameName("e_slowmotionpanel.png");
    left->setAnchorPoint(Vec2(1.0f, 0.5f));
    left->setScaleY(editorArtScale());
    left->setScaleX(-1.01f * editorArtScale());
    addChild(left);
    updateRefRect();
}

// @ios 1000df24c
std::vector<std::string> SlowMotionPanelRef::propertyKeysForUI()
{
    return {"x", "y", "angle", "duration", "panelWidthMeters", "panelHeightMeters"};
}

// @ios 1000df2ec
// Outline of the rotated refRect: CGAffineTransformMakeRotation, corners transformed in
// double (CGPointApplyAffineTransform, summation order kept), rounded to float, then added to
// the double position. Fill (0, 0, 0, 0.25), no border.
void SlowMotionPanelRef::updateDrawingWithNode(DrawNode* node)
{
    const double posX = getPosition().x;
    const double posY = getPosition().y;
    const float halfW = (float)((double)refRect().size.width * 0.5);
    const float halfH = (float)((double)refRect().size.height * 0.5);
    const double angle = (double)(getRotation() * -0.0174532924f);
    const double a = std::cos(angle);
    const double b = std::sin(angle);
    const double c = -b;
    const double d = a;
    const double tx = 0.0;
    const double ty = 0.0;

    const double nx = -halfW, ny = -halfH, px = halfW, py = halfH;
    const double aNx = a * nx, cNy = c * ny, bNx = b * nx, dNy = d * ny;
    const double aPx = a * px, bPx = b * px, cPy = c * py, dPy = d * py;

    double vx[4], vy[4];
    vx[0] = (double)(float)((cNy + aNx) + tx);
    vy[0] = (double)(float)((dNy + bNx) + ty);
    vx[1] = (double)(float)(cNy + (aPx + tx));
    vy[1] = (double)(float)(dNy + (bPx + ty));
    vx[2] = (double)(float)((cPy + aPx) + tx);
    vy[2] = (double)(float)((dPy + bPx) + ty);
    vx[3] = (double)(float)(aNx + (cPy + tx));
    vy[3] = (double)(float)(bNx + (dPy + ty));

    Vec2 verts[4];
    for (int i = 0; i < 4; ++i)
    {
        verts[i] = Vec2((float)(posX + vx[i]), (float)(posY + vy[i]));
    }
    node->drawPolygon(verts, 4, Color4F(0.0f, 0.0f, 0.0f, 0.25f), 0.0f,
                      Color4F(0.0f, 0.0f, 0.0f, 0.0f));
}

Value SlowMotionPanelRef::valueForKey(const std::string& key)
{
    if (key == "duration")
    {
        return Value(duration());
    }
    if (key == "panelWidthMeters")
    {
        return Value(panelWidthMeters());
    }
    if (key == "panelHeightMeters")
    {
        return Value(panelHeightMeters());
    }
    return Special::valueForKey(key);
}

void SlowMotionPanelRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "duration")
    {
        setDuration(value);
    }
    else if (key == "panelWidthMeters")
    {
        setPanelWidthMeters(value);
    }
    else if (key == "panelHeightMeters")
    {
        setPanelHeightMeters(value);
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
