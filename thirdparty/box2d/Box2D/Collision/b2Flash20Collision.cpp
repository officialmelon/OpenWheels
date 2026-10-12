// OpenWheels: Box2D 2.0's narrow phase, see b2Flash20Collision.h. Ported from Box2DFlash 2.0.2
// (b2Collision.as, the engine of the browser game), on 2.3's shape data.

#include "Box2D/Collision/b2Flash20Collision.h"
#include "Box2D/Collision/Shapes/b2CircleShape.h"
#include "Box2D/Collision/Shapes/b2PolygonShape.h"

#include <cfloat>

namespace
{

const uint8 kNullFeature = 255;

struct Flash20Id
{
	uint8 referenceEdge = 0;
	uint8 incidentEdge = 0;
	uint8 incidentVertex = 0;
	uint8 flip = 0;
	uint32 Key() const
	{
		return uint32(referenceEdge) | (uint32(incidentEdge) << 8) | (uint32(incidentVertex) << 16) | (uint32(flip) << 24);
	}
};

// A 2.0 manifold in world space: the normal points from shape A to shape B, each point is where
// 2.0 puts it (on the incident shape) with 2.0's separation.
struct Flash20Manifold
{
	b2Vec2 normal;
	int32 pointCount = 0;
	b2Vec2 points[b2_maxManifoldPoints];
	float32 separations[b2_maxManifoldPoints];
	Flash20Id ids[b2_maxManifoldPoints];
};

struct ClipVertex20
{
	b2Vec2 v;
	Flash20Id id;
};

void CollideCircles(Flash20Manifold* m, const b2CircleShape* circle1, const b2Transform& xf1,
	const b2CircleShape* circle2, const b2Transform& xf2)
{
	m->pointCount = 0;
	b2Vec2 p1 = b2Mul(xf1, circle1->m_p);
	b2Vec2 p2 = b2Mul(xf2, circle2->m_p);
	b2Vec2 d = p2 - p1;
	float32 distSqr = b2Dot(d, d);
	float32 r1 = circle1->m_radius;
	float32 r2 = circle2->m_radius;
	float32 radiusSum = r1 + r2;
	if (distSqr > radiusSum * radiusSum)
	{
		return;
	}
	float32 separation;
	if (distSqr < FLT_MIN)
	{
		separation = -radiusSum;
		m->normal.Set(0.0f, 1.0f);
	}
	else
	{
		float32 dist = b2Sqrt(distSqr);
		separation = dist - radiusSum;
		float32 a = 1.0f / dist;
		m->normal = a * d;
	}
	m->pointCount = 1;
	m->ids[0] = Flash20Id();
	m->separations[0] = separation;
	p1 += r1 * m->normal;
	p2 -= r2 * m->normal;
	m->points[0] = 0.5f * (p1 + p2);
}

void CollidePolygonAndCircle(Flash20Manifold* m, const b2PolygonShape* polygon, const b2Transform& xf1,
	const b2CircleShape* circle, const b2Transform& xf2)
{
	m->pointCount = 0;

	// Circle position in the frame of the polygon.
	b2Vec2 c = b2Mul(xf2, circle->m_p);
	b2Vec2 cLocal = b2MulT(xf1, c);

	// Find the min separating edge.
	int32 normalIndex = 0;
	float32 separation = -FLT_MAX;
	const float32 radius = circle->m_radius;
	const int32 vertexCount = polygon->m_count;
	const b2Vec2* vertices = polygon->m_vertices;
	const b2Vec2* normals = polygon->m_normals;
	for (int32 i = 0; i < vertexCount; ++i)
	{
		float32 s = b2Dot(normals[i], cLocal - vertices[i]);
		if (s > radius)
		{
			return;
		}
		if (s > separation)
		{
			separation = s;
			normalIndex = i;
		}
	}

	// The center is inside the polygon.
	if (separation < FLT_MIN)
	{
		m->pointCount = 1;
		m->normal = b2Mul(xf1.q, normals[normalIndex]);
		Flash20Id id;
		id.incidentEdge = (uint8)normalIndex;
		id.incidentVertex = kNullFeature;
		m->ids[0] = id;
		m->points[0] = c - radius * m->normal;
		m->separations[0] = separation - radius;
		return;
	}

	// Project the circle center onto the edge segment.
	int32 vertIndex1 = normalIndex;
	int32 vertIndex2 = vertIndex1 + 1 < vertexCount ? vertIndex1 + 1 : 0;
	b2Vec2 e = vertices[vertIndex2] - vertices[vertIndex1];
	float32 length = e.Normalize();

	float32 u = b2Dot(cLocal - vertices[vertIndex1], e);
	b2Vec2 p;
	Flash20Id id;
	if (u <= 0.0f)
	{
		p = vertices[vertIndex1];
		id.incidentEdge = kNullFeature;
		id.incidentVertex = (uint8)vertIndex1;
	}
	else if (u >= length)
	{
		p = vertices[vertIndex2];
		id.incidentEdge = kNullFeature;
		id.incidentVertex = (uint8)vertIndex2;
	}
	else
	{
		p = vertices[vertIndex1] + u * e;
		id.incidentEdge = (uint8)normalIndex;
		id.incidentVertex = 0;
	}

	b2Vec2 d = cLocal - p;
	float32 dist = d.Normalize();
	if (dist > radius)
	{
		return;
	}

	m->pointCount = 1;
	m->normal = b2Mul(xf1.q, d);
	m->ids[0] = id;
	m->points[0] = c - radius * m->normal;
	m->separations[0] = dist - radius;
}

// Separation along the given edge normal of poly1 (world space).
float32 EdgeSeparation(const b2PolygonShape* poly1, const b2Transform& xf1, int32 edge1,
	const b2PolygonShape* poly2, const b2Transform& xf2)
{
	const b2Vec2* vertices1 = poly1->m_vertices;
	const b2Vec2* normals1 = poly1->m_normals;
	const int32 count2 = poly2->m_count;
	const b2Vec2* vertices2 = poly2->m_vertices;

	b2Vec2 normal1World = b2Mul(xf1.q, normals1[edge1]);
	b2Vec2 normal1 = b2MulT(xf2.q, normal1World);

	// Find the support vertex on poly2 for -normal.
	int32 vertexIndex2 = 0;
	float32 minDot = FLT_MAX;
	for (int32 i = 0; i < count2; ++i)
	{
		float32 dot = b2Dot(vertices2[i], normal1);
		if (dot < minDot)
		{
			minDot = dot;
			vertexIndex2 = i;
		}
	}

	b2Vec2 v1 = b2Mul(xf1, vertices1[edge1]);
	b2Vec2 v2 = b2Mul(xf2, vertices2[vertexIndex2]);
	return b2Dot(v2 - v1, normal1World);
}

// 2.0's search: start from the edge facing poly2's centroid and climb to a local maximum.
float32 FindMaxSeparation(int32* edgeIndex, const b2PolygonShape* poly1, const b2Transform& xf1,
	const b2PolygonShape* poly2, const b2Transform& xf2)
{
	const int32 count1 = poly1->m_count;
	const b2Vec2* normals1 = poly1->m_normals;

	b2Vec2 d = b2Mul(xf2, poly2->m_centroid) - b2Mul(xf1, poly1->m_centroid);
	b2Vec2 dLocal1 = b2MulT(xf1.q, d);

	int32 edge = 0;
	float32 maxDot = -FLT_MAX;
	for (int32 i = 0; i < count1; ++i)
	{
		float32 dot = b2Dot(normals1[i], dLocal1);
		if (dot > maxDot)
		{
			maxDot = dot;
			edge = i;
		}
	}

	float32 s = EdgeSeparation(poly1, xf1, edge, poly2, xf2);
	if (s > 0.0f)
	{
		return s;
	}

	int32 prevEdge = edge - 1 >= 0 ? edge - 1 : count1 - 1;
	float32 sPrev = EdgeSeparation(poly1, xf1, prevEdge, poly2, xf2);
	if (sPrev > 0.0f)
	{
		return sPrev;
	}

	int32 nextEdge = edge + 1 < count1 ? edge + 1 : 0;
	float32 sNext = EdgeSeparation(poly1, xf1, nextEdge, poly2, xf2);
	if (sNext > 0.0f)
	{
		return sNext;
	}

	int32 bestEdge;
	float32 bestSeparation;
	int32 increment;
	if (sPrev > s && sPrev > sNext)
	{
		increment = -1;
		bestEdge = prevEdge;
		bestSeparation = sPrev;
	}
	else if (sNext > s)
	{
		increment = 1;
		bestEdge = nextEdge;
		bestSeparation = sNext;
	}
	else
	{
		*edgeIndex = edge;
		return s;
	}

	for (;;)
	{
		if (increment == -1)
		{
			edge = bestEdge - 1 >= 0 ? bestEdge - 1 : count1 - 1;
		}
		else
		{
			edge = bestEdge + 1 < count1 ? bestEdge + 1 : 0;
		}
		s = EdgeSeparation(poly1, xf1, edge, poly2, xf2);
		if (s > 0.0f)
		{
			return s;
		}
		if (s > bestSeparation)
		{
			bestEdge = edge;
			bestSeparation = s;
		}
		else
		{
			break;
		}
	}

	*edgeIndex = bestEdge;
	return bestSeparation;
}

void FindIncidentEdge(ClipVertex20 c[2], const b2PolygonShape* poly1, const b2Transform& xf1, int32 edge1,
	const b2PolygonShape* poly2, const b2Transform& xf2)
{
	const b2Vec2* normals1 = poly1->m_normals;
	const int32 count2 = poly2->m_count;
	const b2Vec2* vertices2 = poly2->m_vertices;
	const b2Vec2* normals2 = poly2->m_normals;

	// The reference edge normal in poly2's frame.
	b2Vec2 normal1 = b2MulT(xf2.q, b2Mul(xf1.q, normals1[edge1]));

	// The incident edge on poly2.
	int32 index = 0;
	float32 minDot = FLT_MAX;
	for (int32 i = 0; i < count2; ++i)
	{
		float32 dot = b2Dot(normal1, normals2[i]);
		if (dot < minDot)
		{
			minDot = dot;
			index = i;
		}
	}

	int32 i1 = index;
	int32 i2 = i1 + 1 < count2 ? i1 + 1 : 0;

	c[0].v = b2Mul(xf2, vertices2[i1]);
	c[0].id = Flash20Id();
	c[0].id.referenceEdge = (uint8)edge1;
	c[0].id.incidentEdge = (uint8)i1;
	c[0].id.incidentVertex = 0;

	c[1].v = b2Mul(xf2, vertices2[i2]);
	c[1].id = Flash20Id();
	c[1].id.referenceEdge = (uint8)edge1;
	c[1].id.incidentEdge = (uint8)i2;
	c[1].id.incidentVertex = 1;
}

int32 ClipSegmentToLine(ClipVertex20 vOut[2], const ClipVertex20 vIn[2], const b2Vec2& normal, float32 offset)
{
	int32 numOut = 0;
	b2Vec2 vIn0 = vIn[0].v;
	b2Vec2 vIn1 = vIn[1].v;
	float32 distance0 = b2Dot(normal, vIn0) - offset;
	float32 distance1 = b2Dot(normal, vIn1) - offset;
	if (distance0 <= 0.0f) vOut[numOut++] = vIn[0];
	if (distance1 <= 0.0f) vOut[numOut++] = vIn[1];
	if (distance0 * distance1 < 0.0f)
	{
		float32 interp = distance0 / (distance0 - distance1);
		vOut[numOut].v = vIn0 + interp * (vIn1 - vIn0);
		vOut[numOut].id = distance0 > 0.0f ? vIn[0].id : vIn[1].id;
		++numOut;
	}
	return numOut;
}

void CollidePolygons(Flash20Manifold* m, const b2PolygonShape* polyA, const b2Transform& xfA,
	const b2PolygonShape* polyB, const b2Transform& xfB, b2Vec2* frontNormalOut, float32* frontOffsetOut, bool* flipOut)
{
	m->pointCount = 0;

	int32 edgeA = 0;
	float32 separationA = FindMaxSeparation(&edgeA, polyA, xfA, polyB, xfB);
	if (separationA > 0.0f)
	{
		return;
	}

	int32 edgeB = 0;
	float32 separationB = FindMaxSeparation(&edgeB, polyB, xfB, polyA, xfA);
	if (separationB > 0.0f)
	{
		return;
	}

	const b2PolygonShape* poly1;
	const b2PolygonShape* poly2;
	b2Transform xf1, xf2;
	int32 edge1;
	uint8 flip;
	const float32 k_relativeTol = 0.98f;
	const float32 k_absoluteTol = 0.001f;
	if (separationB > k_relativeTol * separationA + k_absoluteTol)
	{
		poly1 = polyB;
		poly2 = polyA;
		xf1 = xfB;
		xf2 = xfA;
		edge1 = edgeB;
		flip = 1;
	}
	else
	{
		poly1 = polyA;
		poly2 = polyB;
		xf1 = xfA;
		xf2 = xfB;
		edge1 = edgeA;
		flip = 0;
	}

	ClipVertex20 incidentEdge[2];
	FindIncidentEdge(incidentEdge, poly1, xf1, edge1, poly2, xf2);

	const int32 count1 = poly1->m_count;
	const b2Vec2* vertices1 = poly1->m_vertices;
	b2Vec2 v11 = vertices1[edge1];
	b2Vec2 v12 = edge1 + 1 < count1 ? vertices1[edge1 + 1] : vertices1[0];

	b2Vec2 sideNormal = b2Mul(xf1.q, v12 - v11);
	sideNormal.Normalize();
	b2Vec2 frontNormal = b2Cross(sideNormal, 1.0f);

	v11 = b2Mul(xf1, v11);
	v12 = b2Mul(xf1, v12);

	float32 frontOffset = b2Dot(frontNormal, v11);
	float32 sideOffset1 = -b2Dot(sideNormal, v11);
	float32 sideOffset2 = b2Dot(sideNormal, v12);

	ClipVertex20 clipPoints1[2];
	ClipVertex20 clipPoints2[2];
	if (ClipSegmentToLine(clipPoints1, incidentEdge, -sideNormal, sideOffset1) < 2)
	{
		return;
	}
	if (ClipSegmentToLine(clipPoints2, clipPoints1, sideNormal, sideOffset2) < 2)
	{
		return;
	}

	m->normal = flip ? -frontNormal : frontNormal;
	int32 pointCount = 0;
	for (int32 i = 0; i < b2_maxManifoldPoints; ++i)
	{
		float32 separation = b2Dot(frontNormal, clipPoints2[i].v) - frontOffset;
		if (separation <= 0.0f)
		{
			m->points[pointCount] = clipPoints2[i].v;
			m->separations[pointCount] = separation;
			m->ids[pointCount] = clipPoints2[i].id;
			m->ids[pointCount].flip = flip;
			++pointCount;
		}
	}
	m->pointCount = pointCount;
	*frontNormalOut = frontNormal;
	*frontOffsetOut = frontOffset;
	*flipOut = flip != 0;
}

// A polygon and transform mirrored at y = 0, the vertices in the opposite order so that they run
// counter-clockwise again (vertex i is the mirror of vertex count - 1 - i, as in the browser game).
void MirrorPolygon(b2PolygonShape* out, const b2PolygonShape* in)
{
	const int32 n = in->m_count;
	out->m_count = n;
	out->m_radius = in->m_radius;
	out->m_centroid.Set(in->m_centroid.x, -in->m_centroid.y);
	for (int32 i = 0; i < n; ++i)
	{
		const b2Vec2& v = in->m_vertices[n - 1 - i];
		out->m_vertices[i].Set(v.x, -v.y);
		const b2Vec2& normal = in->m_normals[(2 * n - 2 - i) % n];
		out->m_normals[i].Set(normal.x, -normal.y);
	}
}

b2Transform MirrorTransform(const b2Transform& xf)
{
	b2Transform out;
	out.p.Set(xf.p.x, -xf.p.y);
	out.q.s = -xf.q.s;
	out.q.c = xf.q.c;
	return out;
}

// Writes a 2.0 manifold as a 2.3 one whose world manifold has 2.0's normal and separations, and
// whose points are 2.0's moved half the separation back along the normal (2.3's midpoint; the
// flash contact solver moves them forward again, b2ContactSolver::InitializeFlash20Constraints).
// The reference face is A's (e_faceA) unless 2.0 clipped against B's face (e_faceB).
void Encode(b2Manifold* out, const Flash20Manifold& m, bool faceB, const b2Transform& xfA, float32 radiusA,
	const b2Transform& xfB, float32 radiusB)
{
	out->pointCount = m.pointCount;
	if (m.pointCount == 0)
	{
		return;
	}
	if (!faceB)
	{
		// cA = clip + (rA - (clip - plane).n) n, cB = clip - rB n: clip = v + rB n, plane.n = v.n - s - rA.
		const b2Vec2 n = m.normal;
		const float32 planeOffset = b2Dot(m.points[0], n) - m.separations[0] - radiusA;
		out->type = b2Manifold::e_faceA;
		out->localNormal = b2MulT(xfA.q, n);
		out->localPoint = b2MulT(xfA, planeOffset * n);
		for (int32 i = 0; i < m.pointCount; ++i)
		{
			out->points[i].localPoint = b2MulT(xfB, m.points[i] + radiusB * n);
			out->points[i].id.key = m.ids[i].Key();
		}
	}
	else
	{
		// The face normal is B's (-n): clip = v - rA n, plane.(-n) = -v.n - s - rB.
		const b2Vec2 fn = -m.normal;
		const float32 planeOffset = b2Dot(m.points[0], fn) - m.separations[0] - radiusB;
		out->type = b2Manifold::e_faceB;
		out->localNormal = b2MulT(xfB.q, fn);
		out->localPoint = b2MulT(xfB, planeOffset * fn);
		for (int32 i = 0; i < m.pointCount; ++i)
		{
			out->points[i].localPoint = b2MulT(xfA, m.points[i] + radiusA * fn);
			out->points[i].id.key = m.ids[i].Key();
		}
	}
}

}  // namespace

bool b2Flash20Collide(b2Manifold* manifold, const b2Shape* shapeA, const b2Transform& xfA,
	const b2Shape* shapeB, const b2Transform& xfB)
{
	Flash20Manifold m;
	bool faceB = false;
	const b2Shape::Type typeA = shapeA->GetType();
	const b2Shape::Type typeB = shapeB->GetType();
	if (typeA == b2Shape::e_circle && typeB == b2Shape::e_circle)
	{
		CollideCircles(&m, (const b2CircleShape*)shapeA, xfA, (const b2CircleShape*)shapeB, xfB);
	}
	else if (typeA == b2Shape::e_polygon && typeB == b2Shape::e_circle)
	{
		CollidePolygonAndCircle(&m, (const b2PolygonShape*)shapeA, xfA, (const b2CircleShape*)shapeB, xfB);
	}
	else if (typeA == b2Shape::e_circle && typeB == b2Shape::e_polygon)
	{
		// 2.0 keeps the polygon first; flip the result back to A -> B.
		CollidePolygonAndCircle(&m, (const b2PolygonShape*)shapeB, xfB, (const b2CircleShape*)shapeA, xfA);
		m.normal = -m.normal;
		faceB = true;  // the point is on the circle (A), against B's face
	}
	else if (typeA == b2Shape::e_polygon && typeB == b2Shape::e_polygon)
	{
		b2Vec2 frontNormal;
		float32 frontOffset;
		bool flip = false;
		// The game's world is the browser game's one mirrored, so each polygon's vertices run the
		// other way round, which changes the order of the points 2.0's clipping gives (and 2.0's
		// solver goes through them in order). Collide in the browser game's frame instead.
		b2PolygonShape mirrorA, mirrorB;
		MirrorPolygon(&mirrorA, (const b2PolygonShape*)shapeA);
		MirrorPolygon(&mirrorB, (const b2PolygonShape*)shapeB);
		CollidePolygons(&m, &mirrorA, MirrorTransform(xfA), &mirrorB, MirrorTransform(xfB),
			&frontNormal, &frontOffset, &flip);
		m.normal.y = -m.normal.y;
		for (int32 i = 0; i < m.pointCount; ++i)
		{
			m.points[i].y = -m.points[i].y;
		}
		faceB = flip;
	}
	else
	{
		return false;
	}
	Encode(manifold, m, faceB, xfA, shapeA->m_radius, xfB, shapeB->m_radius);
	return true;
}

void b2Flash20ComputeAABB(const b2Shape* shape, const b2Transform& xf, b2AABB* aabb)
{
	if (shape->GetType() == b2Shape::e_circle)
	{
		const b2CircleShape* circle = (const b2CircleShape*)shape;
		b2Vec2 p = b2Mul(xf, circle->m_p);
		b2Vec2 r(circle->m_radius, circle->m_radius);
		aabb->lowerBound = p - r;
		aabb->upperBound = p + r;
		return;
	}
	if (shape->GetType() != b2Shape::e_polygon)
	{
		shape->ComputeAABB(aabb, xf, 0);
		return;
	}

	// 2.0's b2PolygonShape::ComputeOBB: the box along the first edge whose box is 5% smaller than
	// the best so far.
	const b2PolygonShape* polygon = (const b2PolygonShape*)shape;
	const int32 count = polygon->m_count;
	const b2Vec2* v = polygon->m_vertices;
	float32 minArea = FLT_MAX;
	b2Rot obbR;
	b2Vec2 obbCenter(0.0f, 0.0f), obbExtents(0.0f, 0.0f);
	obbR.SetIdentity();
	for (int32 i = 1; i <= count; ++i)
	{
		const b2Vec2 root = v[i - 1];
		b2Vec2 ux = v[i % count] - root;
		ux.Normalize();
		const b2Vec2 uy(-ux.y, ux.x);
		b2Vec2 lower(FLT_MAX, FLT_MAX), upper(-FLT_MAX, -FLT_MAX);
		for (int32 j = 0; j < count; ++j)
		{
			const b2Vec2 d = v[j] - root;
			const b2Vec2 r(b2Dot(ux, d), b2Dot(uy, d));
			lower = b2Min(lower, r);
			upper = b2Max(upper, r);
		}
		const float32 area = (upper.x - lower.x) * (upper.y - lower.y);
		if (area < 0.95f * minArea)
		{
			minArea = area;
			obbR.c = ux.x;
			obbR.s = ux.y;
			const b2Vec2 center = 0.5f * (lower + upper);
			obbCenter = root + center.x * ux + center.y * uy;
			obbExtents = 0.5f * (upper - lower);
		}
	}

	// 2.0's b2PolygonShape::ComputeAABB: the world box of that box.
	const b2Rot R = b2Mul(xf.q, obbR);
	const b2Vec2 col1 = R.GetXAxis();
	const b2Vec2 col2 = R.GetYAxis();
	const b2Vec2 h(b2Abs(col1.x) * obbExtents.x + b2Abs(col2.x) * obbExtents.y,
		b2Abs(col1.y) * obbExtents.x + b2Abs(col2.y) * obbExtents.y);
	const b2Vec2 position = b2Mul(xf, obbCenter);
	aabb->lowerBound = position - h;
	aabb->upperBound = position + h;
}
