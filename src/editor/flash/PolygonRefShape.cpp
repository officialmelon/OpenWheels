// EDITOR (browser features, PC addition): see PolygonRefShape.h.
#include "PolygonRefShape.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "FlashEditor.h"
#include "online/FlashGeometry.h"

USING_NS_CC;
using namespace flashed;

namespace {

float cross(const Vec2& o, const Vec2& a, const Vec2& b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

float signedArea(const std::vector<Vec2>& p)
{
    float a = 0.0f;
    for (size_t i = 0; i < p.size(); ++i)
    {
        const Vec2& u = p[i];
        const Vec2& v = p[(i + 1) % p.size()];
        a += u.x * v.y - v.x * u.y;
    }
    return a * 0.5f;
}

bool inTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c)
{
    const float d1 = cross(a, b, p), d2 = cross(b, c, p), d3 = cross(c, a, p);
    const bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(neg && pos);
}

// Ear clipping of a simple polygon (any winding) into triangles.
std::vector<std::array<Vec2, 3>> triangulate(std::vector<Vec2> poly)
{
    std::vector<std::array<Vec2, 3>> tris;
    if (poly.size() < 3) return tris;
    if (signedArea(poly) < 0) std::reverse(poly.begin(), poly.end());  // CCW
    size_t guard = 0;
    while (poly.size() > 3 && guard++ < 10000)
    {
        bool clipped = false;
        for (size_t i = 0; i < poly.size(); ++i)
        {
            const Vec2& a = poly[(i + poly.size() - 1) % poly.size()];
            const Vec2& b = poly[i];
            const Vec2& c = poly[(i + 1) % poly.size()];
            if (cross(a, b, c) <= 0) continue;
            bool empty = true;
            for (size_t j = 0; j < poly.size() && empty; ++j)
            {
                const Vec2& p = poly[j];
                if (&p == &a || &p == &b || &p == &c) continue;
                if (inTriangle(p, a, b, c) && p != a && p != b && p != c) empty = false;
            }
            if (!empty) continue;
            tris.push_back({a, b, c});
            poly.erase(poly.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) break;  // self-intersecting: draw what we have
    }
    if (poly.size() == 3) tris.push_back({poly[0], poly[1], poly[2]});
    return tris;
}

}  // namespace

PolygonRefShape* PolygonRefShape::create(bool art)
{
    PolygonRefShape* ref = new (std::nothrow) PolygonRefShape();
    if (ref && ref->initWithArt(art))
    {
        ref->autorelease();
        return ref;
    }
    delete ref;
    return nullptr;
}

bool PolygonRefShape::initWithArt(bool art)
{
    if (!RefShape::init()) return false;
    _art = art;
    setLevelItemID(art ? kArtLevelItemID : kPolygonLevelItemID);
    setCanRotate(true);
    _interactive = !art;
    setShapeCount(art ? 0 : 1);
    setArtCount(art ? 1 : 0);
    // A default pentagon-ish outline (clockwise, y down), 100 px across.
    setVertsPx({Vec2(0, -50), Vec2(48, -15), Vec2(29, 40), Vec2(-29, 40), Vec2(-48, -15)});
    return true;
}

void PolygonRefShape::setVertsPx(const std::vector<Vec2>& verts)
{
    _verts = verts;
    _handlesIn.clear();
    _handlesOut.clear();
    updateRefRect();
}

void PolygonRefShape::setHandlesPx(const std::vector<Vec2>& handlesIn, const std::vector<Vec2>& handlesOut)
{
    if (handlesIn.size() != _verts.size() || handlesOut.size() != _verts.size()) return;
    _handlesIn = handlesIn;
    _handlesOut = handlesOut;
    updateRefRect();
}

bool PolygonRefShape::hasHandles() const
{
    for (size_t i = 0; i < _handlesIn.size(); ++i)
        if (_handlesIn[i] != Vec2::ZERO || _handlesOut[i] != Vec2::ZERO) return true;
    return false;
}

std::vector<Vec2> PolygonRefShape::outlinePx() const
{
    if (!hasHandles()) return _verts;
    std::vector<online::geom::Pt> pts, hin, hout;
    for (size_t i = 0; i < _verts.size(); ++i)
    {
        pts.emplace_back(_verts[i].x, _verts[i].y);
        hin.emplace_back(_handlesIn[i].x, _handlesIn[i].y);
        hout.emplace_back(_handlesOut[i].x, _handlesOut[i].y);
    }
    std::vector<Vec2> out;
    for (const auto& p : online::geom::flattenArt(pts, hin, hout, _closed)) out.push_back(Vec2((float)p.x, (float)p.y));
    return out;
}

float PolygonRefShape::extentPx(bool alongX) const
{
    // Flash EdgeShape default extent: min/max of the vertices together with the origin.
    float lo = 0.0f, hi = 0.0f;
    for (const Vec2& v : _verts)
    {
        lo = std::min(lo, alongX ? v.x : v.y);
        hi = std::max(hi, alongX ? v.x : v.y);
    }
    return (float)((int)hi - (int)lo);
}

bool PolygonRefShape::validPolygon(const std::vector<Vec2>& verts)
{
    if (verts.size() < 3 || verts.size() > (size_t)kMaxPolygonVerts) return false;
    // Clockwise in Flash coordinates (y down) == positive signed area there.
    if (signedArea(verts) <= 1.0f) return false;
    for (size_t i = 0; i < verts.size(); ++i)
    {
        const Vec2& a = verts[i];
        const Vec2& b = verts[(i + 1) % verts.size()];
        const Vec2& c = verts[(i + 2) % verts.size()];
        if (cross(a, b, c) < 0.0f) return false;  // convex
    }
    return true;
}

float PolygonRefShape::width() { return pxToStageLength(extentPx(true)); }
float PolygonRefShape::height() { return pxToStageLength(extentPx(false)); }

void PolygonRefShape::setWidth(float width)
{
    KeyValueChange kvo(this, "width");
    const float current = extentPx(true);
    const float target = stageToPxLength(width);
    if (current <= 0.0f || target <= 0.0f) return;
    const float s = std::max(0.1f, std::min(10.0f, target / current));
    for (Vec2& v : _verts) v.x *= s;
    for (Vec2& v : _handlesIn) v.x *= s;
    for (Vec2& v : _handlesOut) v.x *= s;
    updateRefRect();
}

void PolygonRefShape::setHeight(float height)
{
    KeyValueChange kvo(this, "height");
    const float current = extentPx(false);
    const float target = stageToPxLength(height);
    if (current <= 0.0f || target <= 0.0f) return;
    const float s = std::max(0.1f, std::min(10.0f, target / current));
    for (Vec2& v : _verts) v.y *= s;
    for (Vec2& v : _handlesIn) v.y *= s;
    for (Vec2& v : _handlesOut) v.y *= s;
    updateRefRect();
}

void PolygonRefShape::updateRefRect()
{
    if (_verts.empty()) return;
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const Vec2& v : outlinePx())
    {
        minX = std::min(minX, v.x);
        maxX = std::max(maxX, v.x);
        minY = std::min(minY, -v.y);
        maxY = std::max(maxY, -v.y);
    }
    const float px = pxToStageLength(1.0f);
    const Vec2 o(getContentSize() * 0.5f);
    setRefRect(cg::Rect(o.x + minX * px, o.y + minY * px, (maxX - minX) * px, (maxY - minY) * px));
}

void PolygonRefShape::updateDrawingWithNode(DrawNode* node)
{
    if (_verts.size() < 2) return;
    const float px = pxToStageLength(1.0f);
    const AffineTransform t = getNodeToParentAffineTransform();
    const Vec2 o(getContentSize() * 0.5f);
    std::vector<Vec2> pts;
    const std::vector<Vec2> outline = outlinePx();
    pts.reserve(outline.size());
    for (const Vec2& v : outline) pts.push_back(PointApplyAffineTransform(o + Vec2(v.x * px, -v.y * px), t));
    const Color4F fill = innerColor();
    if (!_closed)
    {
        for (size_t i = 0; i + 1 < pts.size(); ++i) node->drawSegment(pts[i], pts[i + 1], 1.0f * px, fill);
        return;
    }
    for (const auto& tri : triangulate(pts)) node->drawTriangle(tri[0], tri[1], tri[2], fill);
    if (outlineColor() >= 0.0f)
    {
        const int c = (int)outlineColor();
        const Color4F line(((c >> 16) & 0xff) / 255.0f, ((c >> 8) & 0xff) / 255.0f, (c & 0xff) / 255.0f, fill.a);
        for (size_t i = 0; i < pts.size(); ++i) node->drawSegment(pts[i], pts[(i + 1) % pts.size()], 0.6f * px, line);
    }
}

std::vector<std::string> PolygonRefShape::propertyKeysForUI()
{
    std::vector<std::string> keys = RefShape::propertyKeysForUI();
    return keys;
}

void PolygonRefShape::setInteractive(const Value& interactive)
{
    // A non-interactive polygon is an art shape (Flash loads <sh t="3" i="f"> as ArtShape).
    const bool value = kvcBool(interactive);
    RefShape::setInteractive(interactive);
    _art = !value;
    setLevelItemID(_art ? kArtLevelItemID : kPolygonLevelItemID);
}

ValueMap PolygonRefShape::properties()
{
    ValueMap d = RefShape::properties();
    d["i"] = Value(interactive());
    ValueVector verts;
    for (const Vec2& v : _verts)
    {
        verts.push_back(Value(v.x));
        verts.push_back(Value(v.y));
    }
    d["verts"] = Value(verts);
    ValueVector handles;
    for (size_t i = 0; i < _handlesIn.size(); ++i)
    {
        handles.push_back(Value(_handlesIn[i].x));
        handles.push_back(Value(_handlesIn[i].y));
        handles.push_back(Value(_handlesOut[i].x));
        handles.push_back(Value(_handlesOut[i].y));
    }
    d["handles"] = Value(handles);
    d["closed"] = Value(_closed);
    return d;
}

void PolygonRefShape::setProperties(const ValueMap& properties)
{
    auto it = properties.find("verts");
    if (it != properties.end() && it->second.getType() == Value::Type::VECTOR)
    {
        const ValueVector& v = it->second.asValueVector();
        std::vector<Vec2> verts;
        for (size_t i = 0; i + 1 < v.size(); i += 2) verts.push_back(Vec2(v[i].asFloat(), v[i + 1].asFloat()));
        _verts = verts;
        _handlesIn.clear();
        _handlesOut.clear();
    }
    auto h = properties.find("handles");
    if (h != properties.end() && h->second.getType() == Value::Type::VECTOR)
    {
        const ValueVector& v = h->second.asValueVector();
        std::vector<Vec2> hin, hout;
        for (size_t i = 0; i + 3 < v.size(); i += 4)
        {
            hin.push_back(Vec2(v[i].asFloat(), v[i + 1].asFloat()));
            hout.push_back(Vec2(v[i + 2].asFloat(), v[i + 3].asFloat()));
        }
        if (hin.size() == _verts.size())
        {
            _handlesIn = hin;
            _handlesOut = hout;
        }
    }
    auto c = properties.find("closed");
    if (c != properties.end()) _closed = c->second.asBool();
    RefShape::setProperties(properties);  // p2/p3 set _widthMeters only; the outline keeps its size
    _art = !interactive();
    setLevelItemID(_art ? kArtLevelItemID : kPolygonLevelItemID);
    updateRefRect();
}
