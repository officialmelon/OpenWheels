#pragma once

// PC addition (render fix): fallback fill for polygons the original ear clipping
// (FFDrawNode / TerrainNode::drawPolyWithVerts, a port of polypartition's Triangulate_EC) cannot
// triangulate. The original then draws nothing at all, so a shape with a duplicated or collinear
// vertex, a self-intersecting outline or a near-collinear corner that classifies differently on
// this platform (the arm64 original is built with fused multiply-adds, x86/MSVC builds are not;
// unverified for IsConvex) silently vanishes. Only called where the original would have drawn
// nothing.

#include <vector>

#include "math/Vec2.h"

namespace polyfill {

// Appends the triangles (3 points each) of the polygon `verts[0..count)` to `out`:
// 1. repaired copy (duplicate and collinear points removed, counter-clockwise) ear-clipped in
//    double precision when the outline is simple;
// 2. otherwise (self-intersecting, or the repaired ear clipping fails) an even-odd scanline fill,
//    which is how Flash fills a self-intersecting path.
// Appends nothing for a polygon without area.
void triangulate(const cocos2d::Vec2* verts, int count, std::vector<cocos2d::Vec2>& out);

}  // namespace polyfill
