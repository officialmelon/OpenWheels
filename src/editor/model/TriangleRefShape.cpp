#include "TriangleRefShape.h"

#include <cmath>

#include "SliderInputObject.h"

USING_NS_CC;

// @ios 10001b058
bool TriangleRefShape::init()
{
    if (!RefShape::init())
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    const float size = _ptmRatio * 3.2f;
    setHeight(size);
    setWidth(size);
    setLevelItemID(6002);
    return true;
}

// @ios 10001b104
void TriangleRefShape::updateDrawingWithNode(DrawNode* node)
{
    const double posX = getPosition().x;
    const double posY = getPosition().y;
    const float w = width();
    const float halfW = w * 0.5f;
    const float h = height();

    // CGAffineTransformMakeRotation((double)(rotation * -0.017453292f))
    const double radians = (double)(getRotation() * -0.017453292f);
    const double a = std::cos(radians), b = std::sin(radians);
    const double c = -b, d = a, tx = 0.0, ty = 0.0;

    const double top = (double)(h * 0.6666667f);
    const double left = (double)-(w * 0.5f);  // fnmul
    const double base = (double)(h * -0.33333334f);
    const double right = (double)halfW;

    // The iOS build fuses these multiply-adds (fmla): one rounding per fma.
    const double baseX = c * base, baseY = d * base;
    const double apexX = std::fma(c, top, tx), apexY = std::fma(d, top, ty);
    const double leftX = std::fma(a, left, baseX) + tx, leftY = std::fma(b, left, baseY) + ty;
    const double rightX = std::fma(a, right, baseX) + tx, rightY = std::fma(b, right, baseY) + ty;

    const Vec2 verts[3] = {
        Vec2((float)(posX + (double)(float)apexX), (float)(posY + (double)(float)apexY)),
        Vec2((float)(posX + (double)(float)leftX), (float)(posY + (double)(float)leftY)),
        Vec2((float)(posX + (double)(float)rightX), (float)(posY + (double)(float)rightY)),
    };
    // Border width 0 (iOS passes an uninitialised border colour; no outline is drawn).
    node->drawPolygon(verts, 3, innerColor(), 0.0f, Color4F(0.0f, 0.0f, 0.0f, 0.0f));
}

// @ios 10001b270
InputObject* TriangleRefShape::inputObjectForPropertyWithRect(const std::string& property,
                                                              const Rect& rect)
{
    if (property == "width")
    {
        return SliderInputObject::create(rect, "WIDTH", "width", width(), _ptmRatio * 0.08f,
                                         _ptmRatio * 24.0f, 0);
    }
    if (property == "height")
    {
        return SliderInputObject::create(rect, "HEIGHT", "height", height(), _ptmRatio * 0.24f,
                                         _ptmRatio * 72.0f, 0);
    }
    return RefShape::inputObjectForPropertyWithRect(property, rect);
}

// @ios 10001b444
void TriangleRefShape::updateRefRect()
{
    const float x = width() * -0.5f;
    const float y = height() * -0.33333334f;
    const float w = width();
    const float h = height();
    setRefRect(cg::Rect((double)x, (double)y, (double)w, (double)h));
}
