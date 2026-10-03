#pragma once
// Polygon helpers for FlashLevelConverter: vertex string parsing, art-shape bezier flattening,
// outline simplification to the mobile loader's fixed vertex limits, and a copy of the ear
// clipping FFDrawNode uses (so a simplified outline that the game could not fill can be rejected
// before it reaches the game).

#include <string>
#include <vector>

namespace online {
namespace geom {

struct Pt {
    double x = 0.0;
    double y = 0.0;
    Pt() = default;
    Pt(double px, double py) : x(px), y(py) {}
};

// One browser vertex string: "x_y" or, for art shapes, "x_y_inX_inY_outX_outY" (handles relative
// to the vertex). Levels older than v1.84 separate with '.' instead of '_' (integers only then).
// Returns false when x/y are missing or not finite; missing/invalid handles read as 0.
bool parseVertex(const char* text, bool dotSeparated, Pt* point, Pt* handleIn, Pt* handleOut);

// Flash's EdgeShape default dimension along one axis: the extent of the vertices together with
// the origin, with min/max truncated to int (the editor keeps them in int variables).
double flashDefaultExtent(const std::vector<Pt>& points, bool alongX);

// Flattens an art outline (cubic segments wherever a handle pair is non-zero, as Flash's
// ArtShape draws it) into a polyline. closed: also the segment from the last vertex to the first.
std::vector<Pt> flattenArt(const std::vector<Pt>& verts, const std::vector<Pt>& handlesIn,
                           const std::vector<Pt>& handlesOut, bool closed);

// Removes consecutive duplicates (and a closing duplicate of the first point).
void removeDuplicates(std::vector<Pt>& ring, double epsilon);
// Removes points that lie on the line through their neighbours (closed ring).
void removeCollinear(std::vector<Pt>& ring, double epsilon);

// Reduces a closed ring to at most maxCount points (Visvalingam-Whyatt: repeatedly drops the
// point whose triangle with its neighbours has the smallest area).
std::vector<Pt> simplifyClosedVW(const std::vector<Pt>& ring, size_t maxCount);
// Same for an open polyline (end points kept).
std::vector<Pt> simplifyOpenVW(const std::vector<Pt>& line, size_t maxCount);
// Ramer-Douglas-Peucker on a closed ring with the tolerance searched so the result has at most
// maxCount points.
std::vector<Pt> simplifyClosedRDP(const std::vector<Pt>& ring, size_t maxCount);

double signedArea(const std::vector<Pt>& ring);
std::vector<Pt> convexHull(std::vector<Pt> points);

// True when FFDrawNode::drawPolyWithVerts would produce triangles for this outline (its port of
// polypartition's Triangulate_EC gives up, drawing nothing, when it runs out of ears).
bool earClipSucceeds(const std::vector<Pt>& ring);

// A closed outline of a stroked open polyline (width w), for open art paths.
std::vector<Pt> strokeOutline(const std::vector<Pt>& line, double width);

}  // namespace geom
}  // namespace online
