#pragma once
// CoreGraphics geometry for the editor port (not an iOS class).
//
// cocos2d-iphone 2.x on arm64 keeps node positions, content sizes and every CGRect in CGFloat =
// double, and the editor code mixes those doubles with float casts (selection rect, marquee,
// bounding boxes; EDITOR_PORT.md rule 3: "keep the double arithmetic where it changes results").
// cocos2d::Vec2/Rect are float, so the editor uses these small double types wherever iOS stores
// or combines CGPoint/CGRect values (EditorSpriteBatchNode ivars, Special::refRect /
// refBoundingBox, align/centre maths). The cg::rect* functions follow CoreGraphics semantics
// (standardised rects, null/empty rules), not cocos2d::Rect's.

#include "math/Vec2.h"
#include "math/CCGeometry.h"

namespace cg {

struct Point
{
    double x = 0.0;
    double y = 0.0;

    Point() = default;
    Point(double px, double py) : x(px), y(py) {}
    explicit Point(const cocos2d::Vec2& v) : x(v.x), y(v.y) {}
    cocos2d::Vec2 toVec2() const { return cocos2d::Vec2((float)x, (float)y); }
};

struct Size
{
    double width = 0.0;
    double height = 0.0;

    Size() = default;
    Size(double w, double h) : width(w), height(h) {}
};

struct Rect
{
    Point origin;
    Size size;

    Rect() = default;
    Rect(double x, double y, double w, double h) : origin(x, y), size(w, h) {}
    explicit Rect(const cocos2d::Rect& r)
        : origin(r.origin.x, r.origin.y), size(r.size.width, r.size.height) {}
    cocos2d::Rect toRect() const
    {
        return cocos2d::Rect((float)origin.x, (float)origin.y, (float)size.width, (float)size.height);
    }
};

extern const Point PointZero;  // CGPointZero
extern const Rect RectZero;    // CGRectZero
extern const Rect RectNull;    // CGRectNull (origin +inf)

Rect rectStandardize(const Rect& r);                    // CGRectStandardize
bool rectIsNull(const Rect& r);                         // CGRectIsNull
bool rectIsEmpty(const Rect& r);                        // CGRectIsEmpty
double rectGetMinX(const Rect& r);                      // CGRectGetMinX ... (standardised)
double rectGetMaxX(const Rect& r);
double rectGetMinY(const Rect& r);
double rectGetMaxY(const Rect& r);
double rectGetMidX(const Rect& r);
double rectGetMidY(const Rect& r);
bool rectContainsPoint(const Rect& r, const Point& p);  // CGRectContainsPoint: min <= p < max
bool rectContainsRect(const Rect& r, const Rect& inner);// CGRectContainsRect
bool rectIntersectsRect(const Rect& a, const Rect& b);  // CGRectIntersectsRect (null/empty -> false)
Rect rectUnion(const Rect& a, const Rect& b);           // CGRectUnion (null operands ignored)

}  // namespace cg
