// PC addition (render fix): see PolyFill.h.
#include "PolyFill.h"

#include <algorithm>
#include <cmath>

namespace polyfill {

namespace {

struct P
{
    double x;
    double y;
};

// Above this many vertices the O(n^2..n^3) ear clipping is skipped for the scanline fill.
const size_t kMaxEarClipVerts = 400;
// Upper bound on scanline boundaries (vertex heights plus edge crossings).
const size_t kMaxSlabs = 8192;

double cross(const P& o, const P& a, const P& b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

double signedArea(const std::vector<P>& p)
{
    double area = 0.0;
    for (size_t i = 0, n = p.size(); i < n; i++)
    {
        const P& a = p[i];
        const P& b = p[(i + 1) % n];
        area += a.x * b.y - a.y * b.x;
    }
    return area * 0.5;
}

bool samePoint(const P& a, const P& b, double eps)
{
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

// Drops repeated points and points on a straight line (or a zero-width spike) between their
// neighbours.
std::vector<P> repaired(const std::vector<P>& in, double eps)
{
    std::vector<P> p;
    p.reserve(in.size());
    for (const P& q : in)
    {
        if (!p.empty() && samePoint(q, p.back(), eps))
        {
            continue;
        }
        p.push_back(q);
    }
    while (p.size() > 1 && samePoint(p.front(), p.back(), eps))
    {
        p.pop_back();
    }
    bool changed = true;
    while (changed && p.size() >= 3)
    {
        changed = false;
        for (size_t i = 0; i < p.size() && p.size() >= 3;)
        {
            const size_t n = p.size();
            const P& a = p[(i + n - 1) % n];
            const P& b = p[i];
            const P& c = p[(i + 1) % n];
            const double ab = std::hypot(b.x - a.x, b.y - a.y);
            const double bc = std::hypot(c.x - b.x, c.y - b.y);
            const double turn = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
            if (ab <= eps || bc <= eps || std::fabs(turn) <= 1e-7 * ab * bc)
            {
                p.erase(p.begin() + (long)i);
                changed = true;
            }
            else
            {
                i++;
            }
        }
    }
    return p;
}

// Proper crossing of segments ab and cd (touching does not count).
bool segmentsCross(const P& a, const P& b, const P& c, const P& d)
{
    const double d1 = cross(c, d, a);
    const double d2 = cross(c, d, b);
    const double d3 = cross(a, b, c);
    const double d4 = cross(a, b, d);
    return ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) &&
           ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0));
}

bool selfIntersecting(const std::vector<P>& p)
{
    const size_t n = p.size();
    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = i + 2; j < n; j++)
        {
            if (i == 0 && j == n - 1)
            {
                continue;  // adjacent through the closing edge
            }
            if (segmentsCross(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n]))
            {
                return true;
            }
        }
    }
    return false;
}

void emit(std::vector<cocos2d::Vec2>& out, const P& a, const P& b, const P& c)
{
    out.push_back(cocos2d::Vec2((float)a.x, (float)a.y));
    out.push_back(cocos2d::Vec2((float)b.x, (float)b.y));
    out.push_back(cocos2d::Vec2((float)c.x, (float)c.y));
}

// Ear clipping of a simple counter-clockwise ring with an inclusive point-in-triangle test.
bool earClip(const std::vector<P>& p, double areaEps, double eps, std::vector<cocos2d::Vec2>& out)
{
    const int n = (int)p.size();
    std::vector<int> prev(n), next(n);
    for (int i = 0; i < n; i++)
    {
        prev[i] = (i + n - 1) % n;
        next[i] = (i + 1) % n;
    }
    std::vector<cocos2d::Vec2> result;
    result.reserve((size_t)(n - 2) * 3);
    int remaining = n;
    int v = 0;
    int misses = 0;
    while (remaining > 3)
    {
        const int a = prev[v];
        const int c = next[v];
        bool ear = cross(p[a], p[v], p[c]) > areaEps;
        if (ear)
        {
            for (int k = next[c]; k != a; k = next[k])
            {
                const P& q = p[k];
                if (samePoint(q, p[a], eps) || samePoint(q, p[v], eps) || samePoint(q, p[c], eps))
                {
                    continue;
                }
                if (cross(p[a], p[v], q) >= -areaEps && cross(p[v], p[c], q) >= -areaEps &&
                    cross(p[c], p[a], q) >= -areaEps)
                {
                    ear = false;
                    break;
                }
            }
        }
        if (ear)
        {
            emit(result, p[a], p[v], p[c]);
            next[a] = c;
            prev[c] = a;
            remaining--;
            v = c;
            misses = 0;
        }
        else
        {
            v = next[v];
            if (++misses > remaining)
            {
                return false;
            }
        }
    }
    emit(result, p[prev[v]], p[v], p[next[v]]);
    out.insert(out.end(), result.begin(), result.end());
    return true;
}

// Even-odd fill: the plane is cut into horizontal slabs at every vertex and crossing height, so
// inside one slab the edges do not cross; the edges spanning a slab, sorted by x, bound filled
// trapezoids pairwise.
void scanlineFill(const std::vector<P>& p, double eps, std::vector<cocos2d::Vec2>& out)
{
    struct Edge
    {
        P lo;
        P hi;
    };
    std::vector<Edge> edges;
    std::vector<double> ys;
    const size_t n = p.size();
    for (size_t i = 0; i < n; i++)
    {
        const P& a = p[i];
        const P& b = p[(i + 1) % n];
        ys.push_back(a.y);
        if (a.y == b.y)
        {
            continue;  // horizontal edges bound no slab
        }
        edges.push_back(a.y < b.y ? Edge{a, b} : Edge{b, a});
    }
    for (size_t i = 0; i < edges.size() && ys.size() < kMaxSlabs; i++)
    {
        for (size_t j = i + 1; j < edges.size() && ys.size() < kMaxSlabs; j++)
        {
            const P& a = edges[i].lo;
            const P& b = edges[i].hi;
            const P& c = edges[j].lo;
            const P& d = edges[j].hi;
            if (!segmentsCross(a, b, c, d))
            {
                continue;
            }
            const double denominator = (b.x - a.x) * (d.y - c.y) - (b.y - a.y) * (d.x - c.x);
            if (denominator == 0.0)
            {
                continue;
            }
            const double t = ((c.x - a.x) * (d.y - c.y) - (c.y - a.y) * (d.x - c.x)) / denominator;
            ys.push_back(a.y + t * (b.y - a.y));
        }
    }
    std::sort(ys.begin(), ys.end());
    std::vector<double> levels;
    for (double y : ys)
    {
        if (levels.empty() || y - levels.back() > eps)
        {
            levels.push_back(y);
        }
    }

    struct Crossing
    {
        double x0;
        double x1;
        double xm;
    };
    std::vector<Crossing> crossings;
    for (size_t s = 0; s + 1 < levels.size(); s++)
    {
        const double y0 = levels[s];
        const double y1 = levels[s + 1];
        const double ym = (y0 + y1) * 0.5;
        crossings.clear();
        for (const Edge& e : edges)
        {
            if (e.lo.y > ym || e.hi.y < ym)
            {
                continue;
            }
            const double slope = (e.hi.x - e.lo.x) / (e.hi.y - e.lo.y);
            crossings.push_back({e.lo.x + (y0 - e.lo.y) * slope, e.lo.x + (y1 - e.lo.y) * slope,
                                 e.lo.x + (ym - e.lo.y) * slope});
        }
        std::sort(crossings.begin(), crossings.end(),
                  [](const Crossing& l, const Crossing& r) { return l.xm < r.xm; });
        for (size_t k = 0; k + 1 < crossings.size(); k += 2)
        {
            const Crossing& l = crossings[k];
            const Crossing& r = crossings[k + 1];
            if (r.x0 - l.x0 <= eps && r.x1 - l.x1 <= eps)
            {
                continue;
            }
            const P a = {l.x0, y0};
            const P b = {r.x0, y0};
            const P c = {r.x1, y1};
            const P d = {l.x1, y1};
            emit(out, a, b, c);
            emit(out, a, c, d);
        }
    }
}

}  // namespace

void triangulate(const cocos2d::Vec2* verts, int count, std::vector<cocos2d::Vec2>& out)
{
    if (verts == nullptr || count < 3)
    {
        return;
    }
    std::vector<P> input;
    input.reserve((size_t)count);
    double minX = verts[0].x, maxX = verts[0].x, minY = verts[0].y, maxY = verts[0].y;
    for (int i = 0; i < count; i++)
    {
        const P q = {verts[i].x, verts[i].y};
        if (!std::isfinite(q.x) || !std::isfinite(q.y))
        {
            continue;
        }
        input.push_back(q);
        minX = std::min(minX, q.x);
        maxX = std::max(maxX, q.x);
        minY = std::min(minY, q.y);
        maxY = std::max(maxY, q.y);
    }
    const double size = std::max(maxX - minX, maxY - minY);
    if (input.size() < 3 || !(size > 0.0))
    {
        return;
    }
    const double eps = size * 1e-6;
    const double areaEps = size * size * 1e-12;

    std::vector<P> ring = repaired(input, eps);
    if (ring.size() >= 3 && ring.size() <= kMaxEarClipVerts && !selfIntersecting(ring))
    {
        if (signedArea(ring) < 0.0)
        {
            std::reverse(ring.begin(), ring.end());
        }
        if (earClip(ring, areaEps, eps, out))
        {
            return;
        }
    }
    // Self-intersecting (or still not clippable): even-odd over the outline as given (repeated
    // and collinear points change nothing there).
    scanlineFill(input, eps, out);
}

}  // namespace polyfill
