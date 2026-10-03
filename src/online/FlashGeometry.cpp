#include "online/FlashGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <queue>

namespace online {
namespace geom {

namespace {

bool parseNumber(const char* begin, const char* end, double* out) {
    if (begin >= end) return false;
    char buffer[64];
    size_t length = (size_t)(end - begin);
    if (length >= sizeof(buffer)) return false;
    memcpy(buffer, begin, length);
    buffer[length] = '\0';
    char* stop = nullptr;
    double value = strtod(buffer, &stop);
    if (stop == buffer || !std::isfinite(value)) return false;
    *out = value;
    return true;
}

double cross(const Pt& o, const Pt& a, const Pt& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

double triangleArea(const Pt& a, const Pt& b, const Pt& c) {
    return std::fabs(cross(a, b, c)) * 0.5;
}

double cubic(double t, double p0, double p1, double p2, double p3) {
    double u = 1.0 - t;
    return u * u * u * p0 + 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t * p3;
}

void appendSegment(std::vector<Pt>& out, const Pt& from, const Pt& to, const Pt& outHandle,
                   const Pt& inHandle) {
    // Flash (ArtShape.addBezierCommands) draws a line when both absolute control points are
    // (0,0); with zero handles the control points sit on the end points, i.e. also a line.
    Pt c1(from.x + outHandle.x, from.y + outHandle.y);
    Pt c2(to.x + inHandle.x, to.y + inHandle.y);
    bool zeroHandles = outHandle.x == 0.0 && outHandle.y == 0.0 && inHandle.x == 0.0 && inHandle.y == 0.0;
    bool originControls = c1.x == 0.0 && c1.y == 0.0 && c2.x == 0.0 && c2.y == 0.0;
    if (zeroHandles || originControls) {
        out.push_back(to);
        return;
    }
    double length = std::hypot(c1.x - from.x, c1.y - from.y) + std::hypot(c2.x - c1.x, c2.y - c1.y) +
                    std::hypot(to.x - c2.x, to.y - c2.y);
    int steps = (int)std::ceil(length / 6.0);
    steps = std::max(2, std::min(24, steps));
    for (int i = 1; i <= steps; i++) {
        double t = (double)i / steps;
        out.push_back(Pt(cubic(t, from.x, c1.x, c2.x, to.x), cubic(t, from.y, c1.y, c2.y, to.y)));
    }
}

// Visvalingam-Whyatt over a doubly linked ring/line. Fixed points (open line ends) never go.
std::vector<Pt> visvalingam(const std::vector<Pt>& input, size_t maxCount, bool closed) {
    const size_t n = input.size();
    if (n <= maxCount) return input;
    std::vector<int> prev(n), next(n);
    std::vector<double> area(n, 0.0);
    std::vector<int> version(n, 0);
    std::vector<bool> alive(n, true);
    for (size_t i = 0; i < n; i++) {
        prev[i] = (int)((i + n - 1) % n);
        next[i] = (int)((i + 1) % n);
    }
    auto fixed = [&](int i) { return !closed && (i == 0 || i == (int)n - 1); };
    auto computeArea = [&](int i) {
        return triangleArea(input[prev[i]], input[i], input[next[i]]);
    };
    typedef std::pair<double, std::pair<int, int>> Entry;  // area, (index, version)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap;
    for (size_t i = 0; i < n; i++) {
        if (fixed((int)i)) continue;
        area[i] = computeArea((int)i);
        heap.push(Entry(area[i], std::make_pair((int)i, 0)));
    }
    size_t count = n;
    double lastArea = 0.0;
    while (count > maxCount && !heap.empty()) {
        Entry top = heap.top();
        heap.pop();
        int i = top.second.first;
        if (!alive[i] || version[i] != top.second.second) continue;
        alive[i] = false;
        count--;
        lastArea = std::max(lastArea, top.first);
        int p = prev[i];
        int q = next[i];
        next[p] = q;
        prev[q] = p;
        for (int k : {p, q}) {
            if (fixed(k)) continue;
            // Standard VW refinement: a neighbour never gets a smaller area than the point just
            // removed, so removal order stays monotonic.
            area[k] = std::max(computeArea(k), lastArea);
            version[k]++;
            heap.push(Entry(area[k], std::make_pair(k, version[k])));
        }
    }
    std::vector<Pt> out;
    out.reserve(count);
    for (size_t i = 0; i < n; i++) {
        if (alive[i]) out.push_back(input[i]);
    }
    return out;
}

double pointSegmentDistance(const Pt& p, const Pt& a, const Pt& b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0) return std::hypot(p.x - a.x, p.y - a.y);
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared;
    t = std::max(0.0, std::min(1.0, t));
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

void rdp(const std::vector<Pt>& points, size_t first, size_t last, double epsilon,
         std::vector<bool>& keep) {
    if (last <= first + 1) return;
    double best = -1.0;
    size_t bestIndex = first;
    for (size_t i = first + 1; i < last; i++) {
        double d = pointSegmentDistance(points[i], points[first], points[last]);
        if (d > best) {
            best = d;
            bestIndex = i;
        }
    }
    if (best > epsilon) {
        keep[bestIndex] = true;
        rdp(points, first, bestIndex, epsilon, keep);
        rdp(points, bestIndex, last, epsilon, keep);
    }
}

std::vector<Pt> rdpClosed(const std::vector<Pt>& ring, double epsilon) {
    const size_t n = ring.size();
    // Split the ring at point 0 and the point farthest from it.
    size_t far = 0;
    double farDistance = -1.0;
    for (size_t i = 1; i < n; i++) {
        double d = std::hypot(ring[i].x - ring[0].x, ring[i].y - ring[0].y);
        if (d > farDistance) {
            farDistance = d;
            far = i;
        }
    }
    std::vector<Pt> loop(ring);
    loop.push_back(ring[0]);
    std::vector<bool> keep(n + 1, false);
    keep[0] = keep[far] = keep[n] = true;
    rdp(loop, 0, far, epsilon, keep);
    rdp(loop, far, n, epsilon, keep);
    std::vector<Pt> out;
    for (size_t i = 0; i < n; i++) {
        if (keep[i]) out.push_back(ring[i]);
    }
    return out;
}

// --- FFDrawNode ear clipping (float, as cocos2d::Vec2) ---

struct EVec {
    float x;
    float y;
};

struct EVert {
    bool isActive;
    bool isConvex;
    bool isEar;
    EVec p;
    EVert* previous;
    EVert* next;
    float angle;
};

bool eIsConvex(EVec p1, EVec p2, EVec p3) {
    float tmp = (p3.y - p1.y) * (p2.x - p1.x) - (p3.x - p1.x) * (p2.y - p1.y);
    return tmp > 0;
}

bool eIsInside(EVec p1, EVec p2, EVec p3, EVec p) {
    if (eIsConvex(p1, p, p2)) return false;
    if (eIsConvex(p2, p, p3)) return false;
    if (eIsConvex(p3, p, p1)) return false;
    return true;
}

void eNormalize(EVec& v) {
    float n = v.x * v.x + v.y * v.y;
    if (n == 1.0f) return;
    n = std::sqrt(n);
    if (n < 2e-37f) return;
    n = 1.0f / n;
    v.x *= n;
    v.y *= n;
}

void eUpdateVertex(EVert* v, EVert* vertices, int numVertices) {
    EVert* v1 = v->previous;
    EVert* v3 = v->next;
    v->isConvex = eIsConvex(v1->p, v->p, v3->p);
    EVec vec1 = {v1->p.x - v->p.x, v1->p.y - v->p.y};
    eNormalize(vec1);
    EVec vec3 = {v3->p.x - v->p.x, v3->p.y - v->p.y};
    eNormalize(vec3);
    v->angle = vec1.x * vec3.x + vec1.y * vec3.y;
    if (v->isConvex) {
        v->isEar = true;
        for (int i = 0; i < numVertices; i++) {
            const EVec& q = vertices[i].p;
            if (q.x == v->p.x && q.y == v->p.y) continue;
            if (q.x == v1->p.x && q.y == v1->p.y) continue;
            if (q.x == v3->p.x && q.y == v3->p.y) continue;
            if (eIsInside(v1->p, v->p, v3->p, q)) {
                v->isEar = false;
                break;
            }
        }
    } else {
        v->isEar = false;
    }
}

}  // namespace

bool parseVertex(const char* text, bool dotSeparated, Pt* point, Pt* handleIn, Pt* handleOut) {
    if (!text) return false;
    const char separator = dotSeparated ? '.' : '_';
    double values[6] = {0, 0, 0, 0, 0, 0};
    bool valid[6] = {false, false, false, false, false, false};
    const char* start = text;
    int field = 0;
    for (const char* c = text;; c++) {
        if (*c == separator || *c == '\0') {
            if (field < 6) valid[field] = parseNumber(start, c, &values[field]);
            field++;
            if (*c == '\0') break;
            start = c + 1;
        }
    }
    if (!valid[0] || !valid[1]) return false;
    *point = Pt(values[0], values[1]);
    if (handleIn) *handleIn = (valid[2] && valid[3]) ? Pt(values[2], values[3]) : Pt();
    if (handleOut) *handleOut = (valid[4] && valid[5]) ? Pt(values[4], values[5]) : Pt();
    return true;
}

double flashDefaultExtent(const std::vector<Pt>& points, bool alongX) {
    double low = 0.0;
    double high = 0.0;
    for (const Pt& p : points) {
        double v = alongX ? p.x : p.y;
        if (v < low) low = std::trunc(v);
        if (v > high) high = std::trunc(v);
    }
    return high - low;
}

std::vector<Pt> flattenArt(const std::vector<Pt>& verts, const std::vector<Pt>& handlesIn,
                           const std::vector<Pt>& handlesOut, bool closed) {
    std::vector<Pt> out;
    if (verts.empty()) return out;
    out.push_back(verts[0]);
    for (size_t i = 1; i < verts.size(); i++) {
        appendSegment(out, verts[i - 1], verts[i], handlesOut[i - 1], handlesIn[i]);
    }
    if (closed && verts.size() > 2) {
        appendSegment(out, verts.back(), verts[0], handlesOut.back(), handlesIn[0]);
        out.pop_back();  // the closing point is verts[0] again
    }
    return out;
}

void removeDuplicates(std::vector<Pt>& ring, double epsilon) {
    std::vector<Pt> out;
    out.reserve(ring.size());
    for (const Pt& p : ring) {
        if (!out.empty() && std::fabs(out.back().x - p.x) <= epsilon &&
            std::fabs(out.back().y - p.y) <= epsilon) {
            continue;
        }
        out.push_back(p);
    }
    while (out.size() > 1 && std::fabs(out.back().x - out[0].x) <= epsilon &&
           std::fabs(out.back().y - out[0].y) <= epsilon) {
        out.pop_back();
    }
    ring.swap(out);
}

void removeCollinear(std::vector<Pt>& ring, double epsilon) {
    bool changed = true;
    while (changed && ring.size() > 3) {
        changed = false;
        for (size_t i = 0; i < ring.size() && ring.size() > 3; i++) {
            const Pt& a = ring[(i + ring.size() - 1) % ring.size()];
            const Pt& b = ring[i];
            const Pt& c = ring[(i + 1) % ring.size()];
            double length = std::hypot(c.x - a.x, c.y - a.y);
            double distance = length > 0.0 ? std::fabs(cross(a, b, c)) / length
                                           : std::hypot(b.x - a.x, b.y - a.y);
            // Only drop points lying between their neighbours (not spikes folding back).
            double dot = (b.x - a.x) * (c.x - a.x) + (b.y - a.y) * (c.y - a.y);
            if (distance <= epsilon && dot >= 0.0 && dot <= length * length) {
                ring.erase(ring.begin() + i);
                changed = true;
                i--;
            }
        }
    }
}

std::vector<Pt> simplifyClosedVW(const std::vector<Pt>& ring, size_t maxCount) {
    return visvalingam(ring, std::max<size_t>(3, maxCount), true);
}

std::vector<Pt> simplifyOpenVW(const std::vector<Pt>& line, size_t maxCount) {
    return visvalingam(line, std::max<size_t>(2, maxCount), false);
}

std::vector<Pt> simplifyClosedRDP(const std::vector<Pt>& ring, size_t maxCount) {
    if (ring.size() <= maxCount || ring.size() < 4) return ring;
    double minX = ring[0].x, maxX = ring[0].x, minY = ring[0].y, maxY = ring[0].y;
    for (const Pt& p : ring) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    double low = 0.0;
    double high = std::max(maxX - minX, maxY - minY) + 1.0;
    std::vector<Pt> best = rdpClosed(ring, high);
    for (int iteration = 0; iteration < 40; iteration++) {
        double mid = (low + high) * 0.5;
        std::vector<Pt> candidate = rdpClosed(ring, mid);
        if (candidate.size() <= maxCount) {
            best.swap(candidate);
            high = mid;
        } else {
            low = mid;
        }
    }
    return best;
}

double signedArea(const std::vector<Pt>& ring) {
    double area = 0.0;
    for (size_t i = 0; i < ring.size(); i++) {
        const Pt& a = ring[i];
        const Pt& b = ring[(i + 1) % ring.size()];
        area += a.x * b.y - a.y * b.x;
    }
    return area * 0.5;
}

std::vector<Pt> convexHull(std::vector<Pt> points) {
    std::sort(points.begin(), points.end(), [](const Pt& a, const Pt& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    if (points.size() < 3) return points;
    std::vector<Pt> hull(points.size() * 2);
    size_t k = 0;
    for (size_t i = 0; i < points.size(); i++) {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], points[i]) <= 0) k--;
        hull[k++] = points[i];
    }
    for (size_t i = points.size() - 1, t = k + 1; i > 0; i--) {
        while (k >= t && cross(hull[k - 2], hull[k - 1], points[i - 1]) <= 0) k--;
        hull[k++] = points[i - 1];
    }
    hull.resize(k > 0 ? k - 1 : 0);
    return hull;
}

bool earClipSucceeds(const std::vector<Pt>& ring) {
    const int count = (int)ring.size();
    if (count < 3) return false;
    std::vector<EVec> verts(count);
    for (int i = 0; i < count; i++) {
        verts[i].x = (float)ring[i].x;
        verts[i].y = (float)ring[i].y;
    }
    float area = 0;
    for (int i1 = 0; i1 < count; i1++) {
        int i2 = (i1 + 1 == count) ? 0 : i1 + 1;
        area += verts[i1].x * verts[i2].y - verts[i1].y * verts[i2].x;
    }
    if (area < 0) std::reverse(verts.begin(), verts.end());
    if (count == 3) return true;

    std::vector<EVert> vertices(count);
    for (int i = 0; i < count; i++) {
        vertices[i].isActive = true;
        vertices[i].isConvex = false;
        vertices[i].isEar = false;
        vertices[i].angle = 0;
        vertices[i].p = verts[i];
        vertices[i].next = &vertices[(i + 1) % count];
        vertices[i].previous = &vertices[(i + count - 1) % count];
    }
    for (int i = 0; i < count; i++) eUpdateVertex(&vertices[i], vertices.data(), count);
    for (int i = 0; i < count; i++) {
        EVert* ear = nullptr;
        for (int j = 0; j < count; j++) {
            if (!vertices[j].isActive || !vertices[j].isEar) continue;
            if (!ear || vertices[j].angle > ear->angle) ear = &vertices[j];
        }
        if (!ear) return false;
        ear->isActive = false;
        ear->previous->next = ear->next;
        ear->next->previous = ear->previous;
        if (i == count - 4) break;
        eUpdateVertex(ear->previous, vertices.data(), count);
        eUpdateVertex(ear->next, vertices.data(), count);
    }
    return true;
}

std::vector<Pt> strokeOutline(const std::vector<Pt>& line, double width) {
    std::vector<Pt> out;
    const size_t n = line.size();
    if (n < 2) return out;
    const double half = width * 0.5;
    std::vector<Pt> left(n), right(n);
    for (size_t i = 0; i < n; i++) {
        // Average of the adjacent segment normals, miter-limited.
        Pt normal;
        double nx = 0.0, ny = 0.0;
        for (int side = 0; side < 2; side++) {
            size_t a = side == 0 ? (i > 0 ? i - 1 : i) : i;
            size_t b = side == 0 ? i : (i + 1 < n ? i + 1 : i);
            double dx = line[b].x - line[a].x;
            double dy = line[b].y - line[a].y;
            double length = std::hypot(dx, dy);
            if (length > 0.0) {
                nx += -dy / length;
                ny += dx / length;
            }
        }
        double length = std::hypot(nx, ny);
        if (length < 1e-9) {
            nx = 0.0;
            ny = 1.0;
        } else {
            nx /= length;
            ny /= length;
        }
        left[i] = Pt(line[i].x + nx * half, line[i].y + ny * half);
        right[i] = Pt(line[i].x - nx * half, line[i].y - ny * half);
    }
    out.insert(out.end(), left.begin(), left.end());
    out.insert(out.end(), right.rbegin(), right.rend());
    return out;
}

}  // namespace geom
}  // namespace online
