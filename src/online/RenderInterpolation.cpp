// ONLINE (PC addition): see RenderInterpolation.h.
#include "online/RenderInterpolation.h"

#include <cmath>
#include <unordered_map>
#include <vector>

#include "Box2D/Box2D.h"
#include "2d/CCNode.h"
#include "LevelItem.h"
#include "online/FlashPhysics.h"

namespace online {
namespace interp {
namespace {

struct Pose
{
    b2Vec2 position;
    float angle;
    void* userData;  // a body created at a destroyed body's address is not the same body
};

struct Moved
{
    b2Body* body;
    b2Transform xf;  // the simulated state, put back by endDraw
    b2Sweep sweep;
};

std::unordered_map<b2Body*, Pose> g_previous;
std::vector<Moved> g_moved;  // bodies moved by beginDraw
bool g_drawing = false;

bool g_cameraValid = false;
cocos2d::Vec2 g_cameraPrevious;
cocos2d::Vec2 g_cameraCurrent;

// The step moved the body further than its velocity explains: it was placed (SetTransform,
// respawn, a trigger's teleport). Draw it where it is instead of sliding it there.
bool teleported(const b2Body* body, const Pose& previous, float step)
{
    const b2Vec2 d = body->GetPosition() - previous.position;
    const float reach = body->GetLinearVelocity().Length() * step * 2.0f + 0.5f;
    if (d.LengthSquared() > reach * reach) return true;
    const float turn = std::fabs(body->GetAngularVelocity()) * step * 2.0f + 0.5f;
    return std::fabs(body->GetAngle() - previous.angle) > turn;
}

}  // namespace

bool enabled() { return browserPhysics(); }

void reset()
{
    g_previous.clear();
    g_moved.clear();
    g_drawing = false;
    g_cameraValid = false;
}

void beforeStep(b2World* world, cocos2d::Node* container)
{
    if (g_cameraValid && container) container->setPosition(g_cameraCurrent);
    g_cameraPrevious = g_cameraCurrent;

    g_previous.clear();
    for (b2Body* body = world->GetBodyList(); body; body = body->GetNext())
    {
        g_previous[body] = Pose{body->GetPosition(), body->GetAngle(), body->GetUserData()};
    }
}

void afterStep(cocos2d::Node* container)
{
    if (!container) return;
    const cocos2d::Vec2 position = container->getPosition();
    if (!g_cameraValid) g_cameraPrevious = position;
    g_cameraCurrent = position;
    g_cameraValid = true;
}

void beginDraw(b2World* world, float alpha)
{
    g_moved.clear();
    g_drawing = true;
    if (alpha >= 1.0f) return;
    if (alpha < 0.0f) alpha = 0.0f;
    const float step = LevelItem::s_timeStep;
    for (b2Body* body = world->GetBodyList(); body; body = body->GetNext())
    {
        auto found = g_previous.find(body);
        if (found == g_previous.end()) continue;
        const Pose& previous = found->second;
        if (previous.userData != body->GetUserData()) continue;
        const b2Vec2 position = body->GetPosition();
        const float angle = body->GetAngle();
        if (position.x == previous.position.x && position.y == previous.position.y &&
            angle == previous.angle)
        {
            continue;
        }
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(angle) ||
            teleported(body, previous, step))
        {
            continue;
        }
        const b2Sweep& sweep = body->GetSweepForDrawing();
        g_moved.push_back(Moved{body, body->GetTransform(), sweep});
        const float drawnAngle = previous.angle + alpha * (angle - previous.angle);
        b2Transform xf;
        xf.Set(previous.position + alpha * (position - previous.position), drawnAngle);
        b2Sweep drawn = sweep;  // GetAngle / GetWorldCenter read the sweep
        drawn.a = drawnAngle;
        drawn.c = b2Mul(xf, sweep.localCenter);
        body->SetStateForDrawing(xf, drawn);
    }
}

void endDraw(b2World* world)
{
    (void)world;
    for (const Moved& m : g_moved) m.body->SetStateForDrawing(m.xf, m.sweep);
    g_moved.clear();
    g_drawing = false;
}

void drawCamera(cocos2d::Node* container, float alpha)
{
    if (!g_cameraValid || !container) return;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    container->setPosition(g_cameraPrevious + (g_cameraCurrent - g_cameraPrevious) * alpha);
}

bool drawing() { return g_drawing; }

}  // namespace interp
}  // namespace online
