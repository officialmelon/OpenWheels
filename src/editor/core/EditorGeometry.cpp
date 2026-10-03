#include "EditorGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cg {

const Point PointZero(0.0, 0.0);
const Rect RectZero(0.0, 0.0, 0.0, 0.0);
const Rect RectNull(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                    0.0, 0.0);

Rect rectStandardize(const Rect& r)
{
    Rect s = r;
    if (s.size.width < 0.0)
    {
        s.origin.x += s.size.width;
        s.size.width = -s.size.width;
    }
    if (s.size.height < 0.0)
    {
        s.origin.y += s.size.height;
        s.size.height = -s.size.height;
    }
    return s;
}

bool rectIsNull(const Rect& r)
{
    return std::isinf(r.origin.x) || std::isinf(r.origin.y);
}

bool rectIsEmpty(const Rect& r)
{
    return rectIsNull(r) || r.size.width == 0.0 || r.size.height == 0.0;
}

double rectGetMinX(const Rect& r) { return rectStandardize(r).origin.x; }
double rectGetMaxX(const Rect& r) { Rect s = rectStandardize(r); return s.origin.x + s.size.width; }
double rectGetMinY(const Rect& r) { return rectStandardize(r).origin.y; }
double rectGetMaxY(const Rect& r) { Rect s = rectStandardize(r); return s.origin.y + s.size.height; }
double rectGetMidX(const Rect& r) { Rect s = rectStandardize(r); return s.origin.x + s.size.width * 0.5; }
double rectGetMidY(const Rect& r) { Rect s = rectStandardize(r); return s.origin.y + s.size.height * 0.5; }

bool rectContainsPoint(const Rect& r, const Point& p)
{
    if (rectIsNull(r))
    {
        return false;
    }
    Rect s = rectStandardize(r);
    return p.x >= s.origin.x && p.x < s.origin.x + s.size.width && p.y >= s.origin.y &&
           p.y < s.origin.y + s.size.height;
}

bool rectContainsRect(const Rect& r, const Rect& inner)
{
    if (rectIsNull(r) || rectIsNull(inner))
    {
        return false;
    }
    // CGRectContainsRect == CGRectEqualToRect(CGRectUnion(r, inner), r) on standardized rects.
    Rect a = rectStandardize(r);
    Rect b = rectStandardize(inner);
    return b.origin.x >= a.origin.x && b.origin.y >= a.origin.y &&
           b.origin.x + b.size.width <= a.origin.x + a.size.width &&
           b.origin.y + b.size.height <= a.origin.y + a.size.height;
}

bool rectIntersectsRect(const Rect& a, const Rect& b)
{
    if (rectIsNull(a) || rectIsNull(b))
    {
        return false;
    }
    Rect s = rectStandardize(a);
    Rect t = rectStandardize(b);
    // CGRectIntersectsRect is !CGRectIsNull(CGRectIntersection(a, b)): touching edges intersect
    // (the intersection is an empty but non-null rect).
    double minX = std::max(s.origin.x, t.origin.x);
    double maxX = std::min(s.origin.x + s.size.width, t.origin.x + t.size.width);
    double minY = std::max(s.origin.y, t.origin.y);
    double maxY = std::min(s.origin.y + s.size.height, t.origin.y + t.size.height);
    return minX <= maxX && minY <= maxY;
}

Rect rectUnion(const Rect& a, const Rect& b)
{
    if (rectIsNull(a))
    {
        return rectStandardize(b);
    }
    if (rectIsNull(b))
    {
        return rectStandardize(a);
    }
    Rect s = rectStandardize(a);
    Rect t = rectStandardize(b);
    double minX = std::min(s.origin.x, t.origin.x);
    double minY = std::min(s.origin.y, t.origin.y);
    double maxX = std::max(s.origin.x + s.size.width, t.origin.x + t.size.width);
    double maxY = std::max(s.origin.y + s.size.height, t.origin.y + t.size.height);
    return Rect(minX, minY, maxX - minX, maxY - minY);
}

}  // namespace cg
