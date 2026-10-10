// OpenWheels: Box2D 2.0's contact bookkeeping and continuous collision, for the browser game's
// physics. Ported from Box2DFlash 2.0.2 (b2ContactManager::PairAdded / PairRemoved, b2Shape and
// b2Body proxy handling, b2World::SolveTOI, b2TimeOfImpact, b2Distance, b2Island::SolveTOI).
//
// After b2World::Flash20Begin, 2.0's sweep-and-prune broad-phase (b2Flash20BroadPhase) makes and
// ends every contact, in 2.0's order (2.3's dynamic tree is still kept for queries). A shape on a
// body without mass counts as static, joints that do not collide make one body's proxies anew, a
// contact 2.0 has just paired is not "slow" until its first update (so it gets a TOI pass even
// between two moving bodies; 2.0's TOI then rewinds both bodies to the time of impact), and a body
// whose box leaves 2.0's world box is frozen.
//
// The distance and TOI code runs in double precision, like the JavaScript port of the game.

#include "Box2D/Dynamics/b2World.h"
#include "Box2D/Collision/Shapes/b2CircleShape.h"
#include "Box2D/Collision/Shapes/b2PolygonShape.h"
#include "Box2D/Collision/b2Flash20Collision.h"
#include "Box2D/Dynamics/Contacts/b2Contact.h"
#include "Box2D/Dynamics/Contacts/b2ContactSolver.h"
#include "Box2D/Dynamics/b2Body.h"
#include "Box2D/Dynamics/b2Fixture.h"
#include "Box2D/Dynamics/b2Island.h"
#include "Box2D/Dynamics/Joints/b2Joint.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_set>
#include <vector>

namespace
{

// JavaScript's Number.MIN_VALUE, which the port uses where 2.0 used FLT_EPSILON.
const double kMinValue = std::numeric_limits<double>::denorm_min();
const double kToiSlop = b2_flash20ToiSlop;

// ---------------------------------------------------------------------------------------------
// Sweeps (2.0 keeps the sweep's start time t0 in what 2.3 calls alpha0)

struct Vec2d
{
	double x = 0.0, y = 0.0;
	Vec2d() = default;
	Vec2d(double x_, double y_) : x(x_), y(y_) {}
	explicit Vec2d(const b2Vec2& v) : x(v.x), y(v.y) {}
};

inline Vec2d operator+(const Vec2d& a, const Vec2d& b) { return Vec2d(a.x + b.x, a.y + b.y); }
inline Vec2d operator-(const Vec2d& a, const Vec2d& b) { return Vec2d(a.x - b.x, a.y - b.y); }
inline Vec2d operator*(double s, const Vec2d& a) { return Vec2d(s * a.x, s * a.y); }
inline double Dot(const Vec2d& a, const Vec2d& b) { return a.x * b.x + a.y * b.y; }

struct XForm
{
	Vec2d p;
	double c = 1.0, s = 0.0;  // R = [c -s; s c]
	Vec2d Mul(const Vec2d& v) const { return Vec2d(p.x + c * v.x - s * v.y, p.y + s * v.x + c * v.y); }
};

// 2.0's b2Sweep::GetXForm.
XForm GetXForm(const b2Sweep& sweep, double t)
{
	const double t0 = sweep.alpha0;
	double cx = sweep.c.x, cy = sweep.c.y, a = sweep.a;
	if (1.0 - t0 > kMinValue)
	{
		const double alpha = (t - t0) / (1.0 - t0);
		cx = (1.0 - alpha) * sweep.c0.x + alpha * sweep.c.x;
		cy = (1.0 - alpha) * sweep.c0.y + alpha * sweep.c.y;
		a = (1.0 - alpha) * sweep.a0 + alpha * sweep.a;
	}
	XForm xf;
	xf.c = std::cos(a);
	xf.s = std::sin(a);
	const double lx = sweep.localCenter.x, ly = sweep.localCenter.y;
	xf.p = Vec2d(cx - (xf.c * lx - xf.s * ly), cy - (xf.s * lx + xf.c * ly));
	return xf;
}

// 2.0's b2Sweep::Advance.
void AdvanceSweep(b2Sweep* sweep, double t)
{
	const double t0 = sweep->alpha0;
	if (t0 < t && 1.0 - t0 > kMinValue)
	{
		const double alpha = (t - t0) / (1.0 - t0);
		sweep->c0.x = float32((1.0 - alpha) * sweep->c0.x + alpha * sweep->c.x);
		sweep->c0.y = float32((1.0 - alpha) * sweep->c0.y + alpha * sweep->c.y);
		sweep->a0 = float32((1.0 - alpha) * sweep->a0 + alpha * sweep->a);
		sweep->alpha0 = float32(t);
	}
}

// ---------------------------------------------------------------------------------------------
// Distance (2.0's b2Distance on "core" shapes, toiSlop inside the real ones)

struct Proxy
{
	bool circle = false;
	int32 count = 0;
	Vec2d core[b2_maxPolygonVertices];	// polygon core vertices, body frame
	Vec2d center;						// circle center, body frame
	double radius = 0.0;
	double sweepRadius = 0.0;

	Vec2d FirstVertex(const XForm& xf) const { return xf.Mul(core[0]); }

	// 2.0's b2PolygonShape::Support.
	Vec2d Support(const XForm& xf, const Vec2d& d) const
	{
		const double lx = d.x * xf.c + d.y * xf.s;
		const double ly = -d.x * xf.s + d.y * xf.c;
		int32 best = 0;
		double bestValue = core[0].x * lx + core[0].y * ly;
		for (int32 i = 1; i < count; ++i)
		{
			const double value = core[i].x * lx + core[i].y * ly;
			if (value > bestValue)
			{
				best = i;
				bestValue = value;
			}
		}
		return xf.Mul(core[best]);
	}
};

bool MakeProxy(Proxy* proxy, const b2Shape* shape, const b2Vec2& localCenter)
{
	const Vec2d lc(localCenter);
	if (shape->GetType() == b2Shape::e_circle)
	{
		const b2CircleShape* circle = (const b2CircleShape*)shape;
		proxy->circle = true;
		proxy->center = Vec2d(circle->m_p);
		proxy->radius = circle->m_radius;
		const Vec2d d = proxy->center - lc;
		proxy->sweepRadius = std::sqrt(Dot(d, d)) + proxy->radius - kToiSlop;
		return true;
	}
	if (shape->GetType() != b2Shape::e_polygon)
	{
		return false;
	}

	const b2PolygonShape* polygon = (const b2PolygonShape*)shape;
	const int32 n = polygon->m_count;
	proxy->circle = false;
	proxy->count = n;
	Vec2d v[b2_maxPolygonVertices];
	Vec2d normals[b2_maxPolygonVertices];
	for (int32 i = 0; i < n; ++i)
	{
		v[i] = Vec2d(polygon->m_vertices[i]);
	}
	for (int32 i = 0; i < n; ++i)
	{
		const Vec2d e = v[i + 1 < n ? i + 1 : 0] - v[i];
		const double length = std::sqrt(Dot(e, e));
		normals[i] = Vec2d(e.y / length, -e.x / length);
	}

	// 2.0's b2PolygonShape::ComputeCentroid (triangles fanned from the origin).
	Vec2d centroid;
	double area = 0.0;
	for (int32 i = 0; i < n; ++i)
	{
		const Vec2d& p2 = v[i];
		const Vec2d& p3 = v[i + 1 < n ? i + 1 : 0];
		const double triangleArea = 0.5 * (p2.x * p3.y - p2.y * p3.x);
		area += triangleArea;
		centroid.x += triangleArea * (1.0 / 3.0) * (p2.x + p3.x);
		centroid.y += triangleArea * (1.0 / 3.0) * (p2.y + p3.y);
	}
	centroid = (1.0 / area) * centroid;

	// The core polygon: every edge moved toiSlop inwards.
	proxy->sweepRadius = 0.0;
	for (int32 i = 0; i < n; ++i)
	{
		const Vec2d& n1 = normals[i - 1 >= 0 ? i - 1 : n - 1];
		const Vec2d& n2 = normals[i];
		const Vec2d d = v[i] - centroid;
		const double d1 = Dot(n1, d) - kToiSlop;
		const double d2 = Dot(n2, d) - kToiSlop;
		const double det = 1.0 / (n1.x * n2.y - n1.y * n2.x);
		proxy->core[i] = Vec2d(det * (n2.y * d1 - n1.y * d2) + centroid.x, det * (n1.x * d2 - n2.x * d1) + centroid.y);
		const Vec2d r = proxy->core[i] - lc;
		proxy->sweepRadius = b2Max(proxy->sweepRadius, std::sqrt(Dot(r, r)));
	}
	return true;
}

// The support shape of DistanceGeneric: a core polygon, or a circle's center.
struct Support
{
	const Proxy* polygon;
	XForm xf;
	Vec2d point;

	Vec2d First() const { return polygon ? polygon->FirstVertex(xf) : point; }
	Vec2d Get(const Vec2d& d) const { return polygon ? polygon->Support(xf, d) : point; }
};

int32 ProcessTwo(Vec2d* x1, Vec2d* x2, Vec2d* p1s, Vec2d* p2s, Vec2d* points)
{
	const Vec2d r = -1.0 * points[1];
	Vec2d d = points[0] - points[1];
	const double length = std::sqrt(Dot(d, d));
	d = (1.0 / length) * d;
	double lambda = Dot(r, d);
	if (lambda <= 0.0 || length < kMinValue)
	{
		*x1 = p1s[1];
		*x2 = p2s[1];
		p1s[0] = p1s[1];
		p2s[0] = p2s[1];
		points[0] = points[1];
		return 1;
	}
	lambda /= length;
	*x1 = p1s[1] + lambda * (p1s[0] - p1s[1]);
	*x2 = p2s[1] + lambda * (p2s[0] - p2s[1]);
	return 2;
}

int32 ProcessThree(Vec2d* x1, Vec2d* x2, Vec2d* p1s, Vec2d* p2s, Vec2d* points)
{
	const Vec2d a = points[0], b = points[1], c = points[2];
	const Vec2d ab = b - a, ac = c - a, bc = c - b;
	const double sn = -Dot(a, ac), sd = Dot(c, ac);
	const double tn = -Dot(b, bc), td = Dot(c, bc);

	if (sd <= 0.0 && td <= 0.0)
	{
		*x1 = p1s[2];
		*x2 = p2s[2];
		p1s[0] = p1s[2];
		p2s[0] = p2s[2];
		points[0] = points[2];
		return 1;
	}

	const double n = ab.x * ac.y - ab.y * ac.x;
	const double vc = n * (a.x * b.y - a.y * b.x);
	const double va = n * (b.x * c.y - b.y * c.x);
	if (va <= 0.0 && tn >= 0.0 && td >= 0.0 && tn + td > 0.0)
	{
		const double lambda = tn / (tn + td);
		*x1 = p1s[1] + lambda * (p1s[2] - p1s[1]);
		*x2 = p2s[1] + lambda * (p2s[2] - p2s[1]);
		p1s[0] = p1s[2];
		p2s[0] = p2s[2];
		points[0] = points[2];
		return 2;
	}

	const double vb = n * (c.x * a.y - c.y * a.x);
	if (vb <= 0.0 && sn >= 0.0 && sd >= 0.0 && sn + sd > 0.0)
	{
		const double lambda = sn / (sn + sd);
		*x1 = p1s[0] + lambda * (p1s[2] - p1s[0]);
		*x2 = p2s[0] + lambda * (p2s[2] - p2s[0]);
		p1s[1] = p1s[2];
		p2s[1] = p2s[2];
		points[1] = points[2];
		return 2;
	}

	const double denom = 1.0 / (va + vb + vc);
	const double u = va * denom;
	const double v = vb * denom;
	const double w = 1.0 - u - v;
	*x1 = Vec2d(u * p1s[0].x + v * p1s[1].x + w * p1s[2].x, u * p1s[0].y + v * p1s[1].y + w * p1s[2].y);
	*x2 = Vec2d(u * p2s[0].x + v * p2s[1].x + w * p2s[2].x, u * p2s[0].y + v * p2s[1].y + w * p2s[2].y);
	return 3;
}

// 2.0's DistanceGeneric (GJK with a 1% tolerance).
double DistanceGeneric(Vec2d* x1, Vec2d* x2, const Support& shape1, const Support& shape2)
{
	Vec2d p1s[3], p2s[3], points[3];
	int32 pointCount = 0;
	*x1 = shape1.First();
	*x2 = shape2.First();
	double vSqr = 0.0;
	for (int32 iter = 0; iter < 20; ++iter)
	{
		const Vec2d v = *x2 - *x1;
		const Vec2d w1 = shape1.Get(v);
		const Vec2d w2 = shape2.Get(-1.0 * v);
		vSqr = Dot(v, v);
		const Vec2d w = w2 - w1;
		if (vSqr - Dot(v, w) <= 0.01 * vSqr)
		{
			if (pointCount == 0)
			{
				*x1 = w1;
				*x2 = w2;
			}
			return std::sqrt(vSqr);
		}

		switch (pointCount)
		{
		case 0:
			p1s[0] = w1;
			p2s[0] = w2;
			points[0] = w;
			*x1 = p1s[0];
			*x2 = p2s[0];
			++pointCount;
			break;
		case 1:
			p1s[1] = w1;
			p2s[1] = w2;
			points[1] = w;
			pointCount = ProcessTwo(x1, x2, p1s, p2s, points);
			break;
		case 2:
			p1s[2] = w1;
			p2s[2] = w2;
			points[2] = w;
			pointCount = ProcessThree(x1, x2, p1s, p2s, points);
			break;
		}

		if (pointCount == 3)
		{
			return 0.0;
		}

		double maxSqr = -std::numeric_limits<double>::max();
		for (int32 i = 0; i < pointCount; ++i)
		{
			maxSqr = b2Max(maxSqr, Dot(points[i], points[i]));
		}
		if (vSqr <= 100.0 * kMinValue * maxSqr)
		{
			const Vec2d d = *x2 - *x1;
			return std::sqrt(Dot(d, d));
		}
	}
	return std::sqrt(vSqr);
}

// 2.0's DistancePC: x1 on the polygon, x2 on the circle.
double DistancePC(Vec2d* x1, Vec2d* x2, const Proxy& polygon, const XForm& xf1, const Proxy& circle, const XForm& xf2)
{
	Support s1{&polygon, xf1, Vec2d()};
	Support s2{nullptr, XForm(), xf2.Mul(circle.center)};
	double distance = DistanceGeneric(x1, x2, s1, s2);
	const double r = circle.radius - kToiSlop;
	if (distance > r)
	{
		distance -= r;
		Vec2d d = *x2 - *x1;
		d = (1.0 / std::sqrt(Dot(d, d))) * d;
		*x2 = *x2 - r * d;
	}
	else
	{
		distance = 0.0;
		*x2 = *x1;
	}
	return distance;
}

double Distance(Vec2d* x1, Vec2d* x2, const Proxy& shape1, const XForm& xf1, const Proxy& shape2, const XForm& xf2)
{
	if (shape1.circle && shape2.circle)
	{
		const Vec2d p1 = xf1.Mul(shape1.center);
		const Vec2d p2 = xf2.Mul(shape2.center);
		Vec2d d = p2 - p1;
		const double dSqr = Dot(d, d);
		const double r1 = shape1.radius - kToiSlop;
		const double r2 = shape2.radius - kToiSlop;
		const double r = r1 + r2;
		if (dSqr > r * r)
		{
			const double length = std::sqrt(Dot(d, d));
			d = (1.0 / length) * d;
			*x1 = p1 + r1 * d;
			*x2 = p2 - r2 * d;
			return length - r;
		}
		if (dSqr > kMinValue * kMinValue)
		{
			d = (1.0 / std::sqrt(Dot(d, d))) * d;
			*x1 = p1 + r1 * d;
			*x2 = *x1;
			return 0.0;
		}
		*x1 = p1;
		*x2 = *x1;
		return 0.0;
	}
	if (!shape1.circle && shape2.circle)
	{
		return DistancePC(x1, x2, shape1, xf1, shape2, xf2);
	}
	if (shape1.circle && !shape2.circle)
	{
		return DistancePC(x2, x1, shape2, xf2, shape1, xf1);
	}
	Support s1{&shape1, xf1, Vec2d()};
	Support s2{&shape2, xf2, Vec2d()};
	return DistanceGeneric(x1, x2, s1, s2);
}

// 2.0's b2TimeOfImpact: conservative advancement over the sweeps, from sweep1's t0. Returns the
// fraction of the rest of the step (from that t0) at which the core shapes come within toiSlop.
double TimeOfImpact(const Proxy& shape1, const b2Sweep& sweep1, const Proxy& shape2, const b2Sweep& sweep2)
{
	const double r1 = shape1.sweepRadius;
	const double r2 = shape2.sweepRadius;
	const double t0 = sweep1.alpha0;
	const Vec2d v1 = Vec2d(sweep1.c) - Vec2d(sweep1.c0);
	const Vec2d v2 = Vec2d(sweep2.c) - Vec2d(sweep2.c0);
	const double omega1 = double(sweep1.a) - double(sweep1.a0);
	const double omega2 = double(sweep2.a) - double(sweep2.a0);

	double alpha = 0.0;
	Vec2d p1, p2;
	double target = 0.0;
	for (int32 iter = 0;; ++iter)
	{
		const double t = (1.0 - alpha) * t0 + alpha;
		const XForm xf1 = GetXForm(sweep1, t);
		const XForm xf2 = GetXForm(sweep2, t);
		const double distance = Distance(&p1, &p2, shape1, xf1, shape2, xf2);
		if (iter == 0)
		{
			if (distance > 2.0 * kToiSlop)
			{
				target = 1.5 * kToiSlop;
			}
			else
			{
				target = b2Max(0.05 * kToiSlop, distance - 0.5 * kToiSlop);
			}
		}
		if (distance - target < 0.05 * kToiSlop || iter == 20)
		{
			break;
		}

		Vec2d normal = p2 - p1;
		normal = (1.0 / std::sqrt(Dot(normal, normal))) * normal;
		const double approachVelocityBound = Dot(normal, v1 - v2) + std::fabs(omega1) * r1 + std::fabs(omega2) * r2;
		if (approachVelocityBound == 0.0)
		{
			alpha = 1.0;
			break;
		}
		const double newAlpha = alpha + (distance - target) / approachVelocityBound;
		if (newAlpha < 0.0 || newAlpha > 1.0)
		{
			alpha = 1.0;
			break;
		}
		if (newAlpha < (1.0 + 100.0 * kMinValue) * alpha)
		{
			break;
		}
		alpha = newAlpha;
	}
	return alpha;
}

bool IsStatic20(const b2Body* body)
{
	return body->GetType() != b2_dynamicBody;
}

}  // namespace

uint32 b2World::GetCreationSerial(const b2Body* body)
{
	return body->m_creationSerial;
}

uint32 b2World::GetCreationSerial(const b2Joint* joint)
{
	return joint->m_creationSerial;
}

uint32 b2World::GetCreationSerial(const b2Fixture* fixture)
{
	return fixture->m_creationSerial;
}

void b2World::Flash20Begin(const b2Vec2& worldLower, const b2Vec2& worldUpper, bool mirrored,
	float32 mirrorY, const Flash20BuildStep* steps, int32 stepCount)
{
	if (m_flash20)
	{
		return;
	}

	while (m_contactManager.m_contactList)
	{
		m_contactManager.Destroy(m_contactManager.m_contactList);
	}
	m_contactManager.m_flash20 = true;
	m_flash20 = new b2Flash20BroadPhase(worldLower, worldUpper, mirrored, mirrorY, this);

	// The bodies and joints in the order 2.0 made them: the given steps, then the rest from the end
	// of their lists (the newest are first).
	std::vector<Flash20BuildStep> order(steps, steps + stepCount);
	std::unordered_set<const void*> listed;
	for (const Flash20BuildStep& step : order)
	{
		listed.insert(step.body ? (const void*)step.body : (const void*)step.joint);
	}
	std::vector<Flash20BuildStep> rest;
	for (b2Body* b = m_bodyList; b; b = b->m_next)
	{
		b->m_flags &= ~b2Body::e_flash20FrozenFlag;
		if (!listed.count(b))
		{
			rest.push_back({b, nullptr, nullptr});
		}
	}
	for (b2Joint* j = m_jointList; j; j = j->m_next)
	{
		if (!listed.count(j))
		{
			rest.push_back({nullptr, j, nullptr});
		}
	}
	order.insert(order.end(), rest.rbegin(), rest.rend());

	for (const Flash20BuildStep& step : order)
	{
		if (step.joint)
		{
			if (!step.joint->m_collideConnected)
			{
				Flash20RefilterJoint(step.joint->m_bodyA, step.joint->m_bodyB);
			}
			continue;
		}
		if (step.fixture)
		{
			if (step.body->m_flags & b2Body::e_activeFlag)
			{
				Flash20CreateProxy(step.fixture, step.body->m_xf);
			}
			continue;
		}

		// 2.0 creates the shapes (oldest first) on a body without mass, which counts as static,
		// then SetMassFromShapes makes it dynamic and its proxies are made anew.
		b2Body* body = step.body;
		if ((body->m_flags & b2Body::e_activeFlag) == 0)
		{
			continue;
		}
		std::vector<b2Fixture*> fixtures;
		for (b2Fixture* f = body->m_fixtureList; f; f = f->m_next)
		{
			fixtures.push_back(f);
		}
		m_flash20PendingMass = body;
		for (auto it = fixtures.rbegin(); it != fixtures.rend(); ++it)
		{
			Flash20CreateProxy(*it, body->m_xf);
		}
		m_flash20PendingMass = nullptr;
		if (body->GetType() == b2_dynamicBody)
		{
			Flash20RefilterBody(body);
		}
	}
}

int32 b2World::Flash20Query(const b2AABB& aabb, b2Fixture** fixtures, int32 maxCount)
{
	return m_flash20->QueryAABB(aabb, reinterpret_cast<void**>(fixtures), maxCount);
}

bool b2World::Flash20IsStatic(const b2Body* body) const
{
	return body->GetType() != b2_dynamicBody || body == m_flash20PendingMass;
}

// 2.0's b2ContactManager::PairAdded.
void b2World::Flash20PairAdded(void* userData1, void* userData2)
{
	b2Fixture* fixture1 = (b2Fixture*)userData1;
	b2Fixture* fixture2 = (b2Fixture*)userData2;
	b2Body* body1 = fixture1->m_body;
	b2Body* body2 = fixture2->m_body;
	if (Flash20IsStatic(body1) && Flash20IsStatic(body2))
	{
		return;
	}
	if (body1 == body2)
	{
		return;
	}
	// 2.0's b2Body::IsConnected: the first joint between the two decides.
	for (b2JointEdge* je = body2->m_jointList; je; je = je->next)
	{
		if (je->other == body1)
		{
			if (!je->joint->m_collideConnected)
			{
				return;
			}
			break;
		}
	}
	b2ContactFilter* filter = m_contactManager.m_contactFilter;
	if (filter && !filter->ShouldCollide(fixture1, fixture2))
	{
		return;
	}
	m_contactManager.AddFlash20Contact(fixture1, fixture2);
}

// 2.0's b2ContactManager::PairRemoved.
void b2World::Flash20PairRemoved(void* userData1, void* userData2)
{
	m_contactManager.DestroyFlash20Contact((b2Fixture*)userData1, (b2Fixture*)userData2);
}

// 2.0's b2Shape::CreateProxy: no proxy outside the world box.
void b2World::Flash20CreateProxy(b2Fixture* fixture, const b2Transform& xf)
{
	b2AABB aabb;
	b2Flash20ComputeAABB(fixture->m_shape, xf, &aabb);
	fixture->m_flash20ProxyId = m_flash20->InRange(aabb) ? m_flash20->CreateProxy(aabb, fixture)
		: int32(b2Flash20BroadPhase::e_nullProxy);
}

void b2World::Flash20DestroyProxy(b2Fixture* fixture)
{
	if (fixture->m_flash20ProxyId != b2Flash20BroadPhase::e_nullProxy)
	{
		m_flash20->DestroyProxy(fixture->m_flash20ProxyId);
		fixture->m_flash20ProxyId = b2Flash20BroadPhase::e_nullProxy;
	}
}

// 2.0's b2Shape::RefilterProxy.
void b2World::Flash20Refilter(b2Fixture* fixture)
{
	if (fixture->m_flash20ProxyId == b2Flash20BroadPhase::e_nullProxy)
	{
		return;
	}
	Flash20DestroyProxy(fixture);
	Flash20CreateProxy(fixture, fixture->m_body->m_xf);
}

void b2World::Flash20RefilterBody(b2Body* body)
{
	for (b2Fixture* f = body->m_fixtureList; f; f = f->m_next)
	{
		Flash20Refilter(f);
	}
}

// 2.0's CreateJoint / DestroyJoint without collideConnected: the body with fewer shapes.
void b2World::Flash20RefilterJoint(b2Body* bodyA, b2Body* bodyB)
{
	Flash20RefilterBody(bodyA->m_fixtureCount < bodyB->m_fixtureCount ? bodyA : bodyB);
}

// 2.0's b2Body::SynchronizeShapes: a body with a shape outside the world box (or without a
// proxy) is frozen, stopped and loses its proxies.
bool b2World::Flash20Synchronize(b2Body* body, const b2Transform& xf1, const b2Transform& xf2)
{
	bool inRange = true;
	for (b2Fixture* f = body->m_fixtureList; f; f = f->m_next)
	{
		if (f->m_flash20ProxyId == b2Flash20BroadPhase::e_nullProxy)
		{
			inRange = false;
			break;
		}
		b2AABB box1, box2, aabb;
		b2Flash20ComputeAABB(f->m_shape, xf1, &box1);
		b2Flash20ComputeAABB(f->m_shape, xf2, &box2);
		aabb.Combine(box1, box2);
		if (!m_flash20->InRange(aabb))
		{
			inRange = false;
			break;
		}
		m_flash20->MoveProxy(f->m_flash20ProxyId, aabb);
	}
	if (inRange)
	{
		return true;
	}

	body->m_flags |= b2Body::e_flash20FrozenFlag;
	body->m_linearVelocity.SetZero();
	body->m_angularVelocity = 0.0f;
	for (b2Fixture* f = body->m_fixtureList; f; f = f->m_next)
	{
		Flash20DestroyProxy(f);
	}
	return false;
}

// 2.0's b2World::SolveTOI.
void b2World::SolveFlash20TOI(const b2TimeStep& step)
{
	b2Island island(m_bodyCount, b2_flash20MaxTOIContactsPerIsland, 0, &m_stackAllocator,
		m_contactManager.m_contactListener);

	for (b2Body* b = m_bodyList; b; b = b->m_next)
	{
		b->m_flags &= ~b2Body::e_islandFlag;
		b->m_sweep.alpha0 = 0.0f;
	}
	for (b2Contact* c = m_contactManager.m_contactList; c; c = c->m_next)
	{
		c->m_flags &= ~(b2Contact::e_toiFlag | b2Contact::e_islandFlag);
	}

	b2Body** stack = (b2Body**)m_stackAllocator.Allocate(m_bodyCount * sizeof(b2Body*));

	// 2.0's b2Body::Advance: the body goes back to its sweep at t.
	auto advanceBody = [](b2Body* body, double t)
	{
		AdvanceSweep(&body->m_sweep, t);
		body->m_sweep.c = body->m_sweep.c0;
		body->m_sweep.a = body->m_sweep.a0;
		body->SynchronizeTransform();
	};

	for (int32 iter = 0;; ++iter)
	{
		// Find the first TOI.
		b2Contact* minContact = nullptr;
		double minTOI = 1.0;
		for (b2Contact* c = m_contactManager.m_contactList; c; c = c->m_next)
		{
			if ((c->m_flags & b2Contact::e_flash20PairFlag) == 0 || (c->m_flags & b2Contact::e_flash20SlowFlag) ||
				c->m_fixtureA->IsSensor() || c->m_fixtureB->IsSensor() || !c->IsEnabled())
			{
				continue;
			}

			double toi = 1.0;
			if (c->m_flags & b2Contact::e_toiFlag)
			{
				toi = c->m_toi;
			}
			else
			{
				b2Body* bA = c->m_fixtureA->m_body;
				b2Body* bB = c->m_fixtureB->m_body;
				if ((IsStatic20(bA) || !bA->IsAwake()) && (IsStatic20(bB) || !bB->IsAwake()))
				{
					continue;
				}

				Proxy proxyA, proxyB;
				if (!MakeProxy(&proxyA, c->m_fixtureA->GetShape(), bA->m_sweep.localCenter) ||
					!MakeProxy(&proxyB, c->m_fixtureB->GetShape(), bB->m_sweep.localCenter))
				{
					continue;
				}

				double t0 = bA->m_sweep.alpha0;
				if (bA->m_sweep.alpha0 < bB->m_sweep.alpha0)
				{
					t0 = bB->m_sweep.alpha0;
					AdvanceSweep(&bA->m_sweep, t0);
				}
				else if (bB->m_sweep.alpha0 < bA->m_sweep.alpha0)
				{
					t0 = bA->m_sweep.alpha0;
					AdvanceSweep(&bB->m_sweep, t0);
				}

				toi = TimeOfImpact(proxyA, bA->m_sweep, proxyB, bB->m_sweep);
				if (toi > 0.0 && toi < 1.0)
				{
					toi = b2Min((1.0 - toi) * t0 + toi, 1.0);
				}
				c->m_toi = float32(toi);
				c->m_flags |= b2Contact::e_toiFlag;
				toi = c->m_toi;
			}

			if (kMinValue < toi && toi < minTOI)
			{
				minContact = c;
				minTOI = toi;
			}
		}

		if (minContact == nullptr || 1.0 - 100.0 * kMinValue < minTOI)
		{
			break;
		}
		if (iter == b2_flash20MaxTOIIterations)
		{
			break;
		}

		// Advance the bodies to the TOI and update the contact.
		b2Body* bA = minContact->m_fixtureA->m_body;
		b2Body* bB = minContact->m_fixtureB->m_body;
		advanceBody(bA, minTOI);
		advanceBody(bB, minTOI);
		minContact->Update(m_contactManager.m_contactListener);
		minContact->m_flags &= ~b2Contact::e_toiFlag;
		if (minContact->m_manifold.pointCount == 0 || !minContact->IsEnabled())
		{
			continue;
		}

		// The island of touching contacts around the moving body, at most 32 contacts.
		b2Body* seed = IsStatic20(bA) ? bB : bA;
		island.Clear();
		int32 stackCount = 0;
		stack[stackCount++] = seed;
		seed->m_flags |= b2Body::e_islandFlag;
		while (stackCount > 0)
		{
			b2Body* b = stack[--stackCount];
			island.Add(b);
			b->m_flags |= b2Body::e_awakeFlag;
			if (IsStatic20(b))
			{
				continue;
			}
			for (b2ContactEdge* ce = b->m_contactList; ce; ce = ce->next)
			{
				b2Contact* contact = ce->contact;
				if (island.m_contactCount == island.m_contactCapacity)
				{
					continue;
				}
				if ((contact->m_flags & (b2Contact::e_islandFlag | b2Contact::e_flash20SlowFlag)) ||
					contact->m_fixtureA->IsSensor() || contact->m_fixtureB->IsSensor() ||
					contact->m_manifold.pointCount == 0 || !contact->IsEnabled())
				{
					continue;
				}
				island.Add(contact);
				contact->m_flags |= b2Contact::e_islandFlag;

				b2Body* other = ce->other;
				if (other->m_flags & b2Body::e_islandFlag)
				{
					continue;
				}
				if (!IsStatic20(other))
				{
					advanceBody(other, minTOI);
					other->SetAwake(true);
				}
				stack[stackCount++] = other;
				other->m_flags |= b2Body::e_islandFlag;
			}
		}

		b2TimeStep subStep;
		subStep.dt = float32((1.0 - minTOI) * step.dt);
		subStep.inv_dt = 1.0f / subStep.dt;
		subStep.dtRatio = 1.0f;
		subStep.velocityIterations = step.velocityIterations;
		subStep.positionIterations = step.positionIterations;
		subStep.warmStarting = true;
		island.SolveFlash20TOI(subStep);

		for (int32 i = 0; i < island.m_bodyCount; ++i)
		{
			b2Body* b = island.m_bodies[i];
			b->m_flags &= ~b2Body::e_islandFlag;
			if (!b->IsAwake() || (b->m_flags & b2Body::e_flash20FrozenFlag) || IsStatic20(b))
			{
				continue;
			}
			b->SynchronizeFixtures();
			for (b2ContactEdge* ce = b->m_contactList; ce; ce = ce->next)
			{
				ce->contact->m_flags &= ~b2Contact::e_toiFlag;
			}
		}
		for (int32 i = 0; i < island.m_contactCount; ++i)
		{
			island.m_contacts[i]->m_flags &= ~(b2Contact::e_toiFlag | b2Contact::e_islandFlag);
		}

		m_flash20->Commit();
	}

	m_stackAllocator.Free(stack);
}

// 2.0's b2Island::SolveTOI: the contacts start from their stored impulses (not applied again),
// velocities, then the move from the TOI to the step's end, then position correction with
// the TOI baumgarte. Nothing is clamped and the impulses are not stored.
void b2Island::SolveFlash20TOI(const b2TimeStep& subStep)
{
	for (int32 i = 0; i < m_bodyCount; ++i)
	{
		b2Body* b = m_bodies[i];
		m_positions[i].c = b->m_sweep.c;
		m_positions[i].a = b->m_sweep.a;
		m_velocities[i].v = b->m_linearVelocity;
		m_velocities[i].w = b->m_angularVelocity;
	}

	b2ContactSolverDef contactSolverDef;
	contactSolverDef.contacts = m_contacts;
	contactSolverDef.count = m_contactCount;
	contactSolverDef.allocator = m_allocator;
	contactSolverDef.step = subStep;
	contactSolverDef.positions = m_positions;
	contactSolverDef.velocities = m_velocities;
	b2ContactSolver contactSolver(&contactSolverDef);
	contactSolver.InitializeVelocityConstraints();

	for (int32 i = 0; i < subStep.velocityIterations; ++i)
	{
		contactSolver.SolveVelocityConstraints();
	}

	const float32 h = subStep.dt;
	for (int32 i = 0; i < m_bodyCount; ++i)
	{
		b2Body* b = m_bodies[i];
		if (b->GetType() != b2_dynamicBody)
		{
			continue;
		}
		b->m_sweep.c0 = b->m_sweep.c;
		b->m_sweep.a0 = b->m_sweep.a;
		m_positions[i].c += h * m_velocities[i].v;
		m_positions[i].a += h * m_velocities[i].w;
	}

	for (int32 i = 0; i < subStep.positionIterations; ++i)
	{
		if (contactSolver.SolveFlash20PositionConstraints(b2_flash20ToiBaumgarte))
		{
			break;
		}
	}

	for (int32 i = 0; i < m_bodyCount; ++i)
	{
		b2Body* b = m_bodies[i];
		if (b->GetType() != b2_dynamicBody)
		{
			continue;
		}
		b->m_sweep.c = m_positions[i].c;
		b->m_sweep.a = m_positions[i].a;
		b->m_linearVelocity = m_velocities[i].v;
		b->m_angularVelocity = m_velocities[i].w;
		b->SynchronizeTransform();
	}

	Report(contactSolver.m_velocityConstraints);
}
