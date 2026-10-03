#include "RectangleRefShape.h"

#include <cmath>

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 1000c5b60
bool RectangleRefShape::init()
{
    if (!RefShape::init())
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    setWidth(_ptmRatio * 4.8f);
    setHeight(_ptmRatio * 1.6f);
    setLevelItemID(6000);
    return true;
}

// @ios 1000c5c10
void RectangleRefShape::updateDrawingWithNode(DrawNode* node)
{
    const double posX = getPosition().x;
    const double posY = getPosition().y;
    const float w = width();
    const float halfW = w * 0.5f;
    const float h = height();
    const float halfH = h * 0.5f;

    // CGAffineTransformMakeRotation((double)(rotation * -0.017453292f))
    const double radians = (double)(getRotation() * -0.017453292f);
    const double a = std::cos(radians), b = std::sin(radians);
    const double c = -b, d = a, tx = 0.0, ty = 0.0;

    const double nx = (double)-(w * 0.5f);  // fnmul
    const double ny = (double)-(h * 0.5f);
    const double px = (double)halfW;
    const double py = (double)halfH;

    // CGPointApplyAffineTransform per corner (separate multiplies/adds), rounded to float, then
    // offset by the double position.
    const double axn = a * nx, cyn = c * ny, bxn = b * nx, dyn = d * ny;
    const double axp = a * px, bxp = b * px, cyp = c * py, dyp = d * py;
    const double vx[4] = {(double)(float)(cyn + axn + tx), (double)(float)(cyn + axp + tx),
                          (double)(float)(cyp + axp + tx), (double)(float)(cyp + axn + tx)};
    const double vy[4] = {(double)(float)(dyn + bxn + ty), (double)(float)(dyn + bxp + ty),
                          (double)(float)(dyp + bxp + ty), (double)(float)(dyp + bxn + ty)};
    Vec2 verts[4];
    for (int i = 0; i < 4; ++i)
    {
        verts[i] = Vec2((float)(posX + vx[i]), (float)(posY + vy[i]));
    }
    // Border width 0 (iOS passes an uninitialised border colour; no outline is drawn).
    node->drawPolygon(verts, 4, innerColor(), 0.0f, Color4F(0.0f, 0.0f, 0.0f, 0.0f));
}

// @ios 1000c5dfc
void RectangleRefShape::updateRefRect()
{
    const float x = width() * -0.5f;
    const float y = height() * -0.5f;
    const float w = width();
    const float h = height();
    setRefRect(cg::Rect((double)x, (double)y, (double)w, (double)h));
}

// @ios 1000c5e70
InputObject* RectangleRefShape::inputObjectForPropertyWithRect(const std::string& property,
                                                               const Rect& rect)
{
    if (property == "width")
    {
        return SliderInputObject::create(rect, "WIDTH", "width", width(), _ptmRatio * 0.08f,
                                         _ptmRatio * 80.0f, 0);
    }
    if (property == "height")
    {
        return SliderInputObject::create(rect, "HEIGHT", "height", height(), _ptmRatio * 0.08f,
                                         _ptmRatio * 80.0f, 0);
    }
    return RefShape::inputObjectForPropertyWithRect(property, rect);
}
