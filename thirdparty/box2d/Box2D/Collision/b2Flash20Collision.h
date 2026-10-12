// OpenWheels: Box2D 2.0's narrow phase (Box2DFlash 2.0.2 b2CollideCircles,
// b2CollidePolygonAndCircle, b2CollidePolygons), used by b2Contact::Update while g_flash20Solver
// is set. 2.0 polygons have no skin radius, its polygon test climbs from the edge facing the other
// polygon's centroid (so it can report an overlap a full search would not), and its points sit on
// the incident shape. The result is written as a 2.3 b2Manifold (e_faceA, see the .cpp) so the
// solver and the game read it like any other manifold.

#ifndef B2_FLASH20_COLLISION_H
#define B2_FLASH20_COLLISION_H

#include "Box2D/Collision/b2Collision.h"

class b2Shape;

/// Collides two shapes the way Box2D 2.0 does. Returns false for shape types 2.0 did not have
/// (edges, chains): the caller then uses 2.3's own collision.
bool b2Flash20Collide(b2Manifold* manifold, const b2Shape* shapeA, const b2Transform& xfA,
	const b2Shape* shapeB, const b2Transform& xfB);

/// The box Box2D 2.0 gives a shape: the world box of its oriented bounding box (polygons, see
/// 2.0's b2PolygonShape::ComputeOBB) or of its circle, with no skin.
void b2Flash20ComputeAABB(const b2Shape* shape, const b2Transform& xf, b2AABB* aabb);

#endif
