#include "platform/debug/WorldDump.h"

#include <cstdio>
#include <map>
#include <sstream>

#include "Box2D/Box2D.h"

namespace openwheels {
namespace debug {
namespace {

// Same textual precision on both sides: Python's json.dump writes repr(float), which is the
// shortest round-trip form of the *double*; we emit 9 significant digits of the float, and the
// diff tool compares numerically with a tolerance, so formatting never causes false diffs.
void num(std::ostringstream& o, float v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(v));
    o << buf;
}

void vec(std::ostringstream& o, const b2Vec2& v) {
    o << '[';
    num(o, v.x);
    o << ", ";
    num(o, v.y);
    o << ']';
}

// b2Body keeps its flags private; rebuild the raw bit set from the public accessors
// (values match Box2D 2.3 b2Body::e_* flags).
unsigned bodyFlags(const b2Body* b) {
    unsigned f = 0;
    if (b->IsAwake()) f |= 0x0002;
    if (b->IsSleepingAllowed()) f |= 0x0004;
    if (b->IsBullet()) f |= 0x0008;
    if (b->IsFixedRotation()) f |= 0x0010;
    if (b->IsActive()) f |= 0x0020;
    return f;
}

}  // namespace

bool dumpWorldJson(b2World* world, const std::string& path, int frames) {
    std::map<const b2Body*, int> index;
    int n = 0;
    for (b2Body* b = world->GetBodyList(); b; b = b->GetNext()) index[b] = n++;

    std::ostringstream o;
    o << "{\"gravity\": ";
    vec(o, world->GetGravity());
    o << ", \"frames\": " << frames << ", \"bodies\": [";
    bool firstBody = true;
    for (b2Body* b = world->GetBodyList(); b; b = b->GetNext()) {
        if (!firstBody) o << ", ";
        firstBody = false;
        b2MassData md;
        b->GetMassData(&md);
        o << "{\"type\": " << static_cast<int>(b->GetType()) << ", \"pos\": ";
        vec(o, b->GetPosition());
        o << ", \"angle\": "; num(o, b->GetAngle());
        o << ", \"linvel\": "; vec(o, b->GetLinearVelocity());
        o << ", \"angvel\": "; num(o, b->GetAngularVelocity());
        o << ", \"flags\": " << bodyFlags(b);
        o << ", \"mass\": "; num(o, b->GetMass());
        // b2Body::m_I is the rotational inertia about the centre of mass
        o << ", \"I\": "; num(o, b->GetInertia() - b->GetMass() * b2Dot(b->GetLocalCenter(), b->GetLocalCenter()));
        o << ", \"linDamp\": "; num(o, b->GetLinearDamping());
        o << ", \"angDamp\": "; num(o, b->GetAngularDamping());
        o << ", \"gravityScale\": "; num(o, b->GetGravityScale());
        o << ", \"localCenter\": "; vec(o, b->GetLocalCenter());
        o << ", \"fixtures\": [";
        bool firstFx = true;
        for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext()) {
            if (!firstFx) o << ", ";
            firstFx = false;
            const b2Filter& fl = f->GetFilterData();
            const b2Shape* s = f->GetShape();
            o << "{\"density\": "; num(o, f->GetDensity());
            o << ", \"friction\": "; num(o, f->GetFriction());
            o << ", \"restitution\": "; num(o, f->GetRestitution());
            o << ", \"sensor\": " << (f->IsSensor() ? 1 : 0);
            o << ", \"filter\": [" << fl.categoryBits << ", " << fl.maskBits << ", " << fl.groupIndex << "]";
            o << ", \"shape\": " << static_cast<int>(s->GetType());
            o << ", \"radius\": "; num(o, s->m_radius);
            switch (s->GetType()) {
                case b2Shape::e_circle:
                    o << ", \"center\": "; vec(o, static_cast<const b2CircleShape*>(s)->m_p);
                    break;
                case b2Shape::e_edge: {
                    auto e = static_cast<const b2EdgeShape*>(s);
                    o << ", \"v\": ["; vec(o, e->m_vertex1); o << ", "; vec(o, e->m_vertex2); o << "]";
                    break;
                }
                case b2Shape::e_polygon: {
                    auto p = static_cast<const b2PolygonShape*>(s);
                    o << ", \"v\": [";
                    for (int i = 0; i < p->m_count; ++i) { if (i) o << ", "; vec(o, p->m_vertices[i]); }
                    o << "]";
                    break;
                }
                case b2Shape::e_chain: {
                    auto c = static_cast<const b2ChainShape*>(s);
                    o << ", \"v\": [";
                    for (int i = 0; i < c->m_count; ++i) { if (i) o << ", "; vec(o, c->m_vertices[i]); }
                    o << "]";
                    break;
                }
                default:
                    break;
            }
            o << "}";
        }
        o << "]}";
    }
    o << "], \"joints\": [";
    bool firstJ = true;
    for (b2Joint* j = world->GetJointList(); j; j = j->GetNext()) {
        if (!firstJ) o << ", ";
        firstJ = false;
        auto ia = index.find(j->GetBodyA());
        auto ib = index.find(j->GetBodyB());
        o << "{\"type\": " << static_cast<int>(j->GetType())
          << ", \"a\": " << (ia == index.end() ? -1 : ia->second)
          << ", \"b\": " << (ib == index.end() ? -1 : ib->second)
          << ", \"collide\": " << (j->GetCollideConnected() ? 1 : 0);
        if (j->GetType() == e_revoluteJoint) {
            auto r = static_cast<b2RevoluteJoint*>(j);
            o << ", \"anchorA\": "; vec(o, r->GetLocalAnchorA());
            o << ", \"anchorB\": "; vec(o, r->GetLocalAnchorB());
            o << ", \"motor\": " << (r->IsMotorEnabled() ? 1 : 0);
            o << ", \"maxTorque\": "; num(o, r->GetMaxMotorTorque());
            o << ", \"speed\": "; num(o, r->GetMotorSpeed());
            o << ", \"limit\": " << (r->IsLimitEnabled() ? 1 : 0);
            o << ", \"ref\": "; num(o, r->GetReferenceAngle());
            o << ", \"lower\": "; num(o, r->GetLowerLimit());
            o << ", \"upper\": "; num(o, r->GetUpperLimit());
        } else if (j->GetType() == e_prismaticJoint) {
            auto p = static_cast<b2PrismaticJoint*>(j);
            o << ", \"anchorA\": "; vec(o, p->GetLocalAnchorA());
            o << ", \"anchorB\": "; vec(o, p->GetLocalAnchorB());
            o << ", \"axis\": "; vec(o, p->GetLocalAxisA());
            o << ", \"ref\": "; num(o, p->GetReferenceAngle());
            o << ", \"lower\": "; num(o, p->GetLowerLimit());
            o << ", \"upper\": "; num(o, p->GetUpperLimit());
            o << ", \"maxForce\": "; num(o, p->GetMaxMotorForce());
            o << ", \"speed\": "; num(o, p->GetMotorSpeed());
            o << ", \"limit\": " << (p->IsLimitEnabled() ? 1 : 0);
            o << ", \"motor\": " << (p->IsMotorEnabled() ? 1 : 0);
        }
        // Other joint types: the oracle emits raw private fields; compare type/bodies only.
        o << "}";
    }
    o << "]}";

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    const std::string s = o.str();
    std::fwrite(s.data(), 1, s.size(), fp);
    std::fclose(fp);
    return true;
}

}  // namespace debug
}  // namespace openwheels
