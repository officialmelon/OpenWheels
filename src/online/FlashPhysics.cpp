// ONLINE (PC addition): see FlashPhysics.h.
#include "online/FlashPhysics.h"

#include <Box2D/Box2D.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include "tinyxml2/tinyxml2.h"
#include <cmath>

#include "cocos2d.h"
#include "LevelItem.h"
#include "Session.h"
#include "online/FlashRuntime.h"
#include "online/RenderInterpolation.h"

// Box2D 2.3.1's switch for the 2-point block solver (b2ContactSolver.cpp, not in its headers).
extern bool g_blockSolve;
// OpenWheels' Box2D (thirdparty/box2d): the browser game's Box2D 2.0 solver rules (b2Settings.h).
extern bool g_flash20Solver;

namespace online {
namespace {

const char* const kOptionKey = "qol_browser_physics";
const float kMobileTimeStep = 1.0f / 60.0f;
const float kFlashTimeStep = 1.0f / 30.0f;
const int kFlashIterations = 10;

bool g_active = false;  // the running level uses the browser profile

// flashCharacterBegin / End: the lists' heads before the character, then its bodies and joints.
b2Body* g_bodyHeadBefore = nullptr;
b2Joint* g_jointHeadBefore = nullptr;
std::vector<b2Body*> g_characterBodies;
std::vector<b2Joint*> g_characterJoints;
bool g_offlineLevel = false;  // the level being loaded is an offline one (restored campaign, editor)

// LevelItem keeps the previous step for CharacterB2D::timeStepChanged's ratio; setting the same
// step twice leaves current == previous, i.e. "no change pending".
void settle(Session* session, float timeStep)
{
    if (session) {
        session->setTimeStep(timeStep);
        session->setTimeStep(timeStep);
    } else {
        LevelItem::setTimeStep(timeStep);
        LevelItem::setTimeStep(timeStep);
    }
}

}  // namespace

bool browserPhysicsOption()
{
    return cocos2d::UserDefault::getInstance()->getBoolForKey(kOptionKey, true);
}

void setBrowserPhysicsOption(bool on)
{
    cocos2d::UserDefault::getInstance()->setBoolForKey(kOptionKey, on);
}

void setOfflineLevel(bool offline) { g_offlineLevel = offline; }

std::string markOfflineLevel(const std::string& mobileXml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(mobileXml.c_str(), mobileXml.size()) != tinyxml2::XML_SUCCESS) return mobileXml;
    tinyxml2::XMLElement* root = doc.RootElement();
    tinyxml2::XMLElement* info = root ? root->FirstChildElement("info") : nullptr;
    if (!info) return mobileXml;
    info->SetAttribute("offline", true);
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}

bool browserPhysics() { return g_active && flashLevel(); }

int stepsPerFlashFrame() { return browserPhysics() ? 1 : 2; }

namespace {
// Current step / (1/60): exactly 1 at 1/60 (so callers return their argument unchanged).
float stepsRatio60()
{
    const float ratio = LevelItem::s_timeStep * 60.0f;
    return std::fabs(ratio - 1.0f) < 1e-4f ? 1.0f : ratio;
}
}  // namespace

int stepsFor60HzFrames(int frames60)
{
    const float ratio = stepsRatio60();
    if (ratio == 1.0f || frames60 <= 0) return frames60;
    // ratio is 2 (+ float error) at 1/30: halves round up (the 1e-3 absorbs the error).
    return std::max(1, (int)std::floor(frames60 / ratio + 0.5f + 1e-3f));
}

int stepsForFlashFrames(int frames30) { return stepsFor60HzFrames(frames30 * 2); }

float perStep(float per60HzStepValue)
{
    const float ratio = stepsRatio60();
    return ratio == 1.0f ? per60HzStepValue : per60HzStepValue * ratio;
}

void resetLevelTimeStep(Session* session)
{
    interp::reset();
    if (!g_active) return;
    g_active = false;
    settle(session, kMobileTimeStep);
}

void beginLevelTimeStep(Session* session)
{
    if (!flashLevel() || g_offlineLevel || !browserPhysicsOption()) return;
    g_active = true;
    settle(session, kFlashTimeStep);
}

namespace {

// Hands the world's contacts to Box2D 2.0's broad-phase with the browser game's world box, in its
// metres (y down): the stage (320 x 160 m) with 4 m on the sides and below and 84 m above. The
// mobile world is that one flipped at 160 m.
const b2Vec2 kFlashWorldLower(-4.0f, -84.0f);
const b2Vec2 kFlashWorldUpper(324.0f, 164.0f);
const float kFlashBorder = 4.0f;  // UserLevel's borderThickness, 120 px

void flash20Begin(b2World* world, const b2World::Flash20BuildStep* steps, int count)
{
    world->Flash20Begin(kFlashWorldLower, kFlashWorldUpper, true, kFlashStageHeightPx / kFlashPtm,
                        steps, count);
}

// The mobile game walls the stage in with two edges (Session::createWorld); the browser game's
// UserLevel gives its level body three boxes, left, right and top, filling the broad-phase box's
// border (createStaticShapes). They are the level body's first shapes.
void useFlashBorders(b2World* world)
{
    b2Body* levelBody = nullptr;
    std::vector<b2Fixture*> walls;
    for (b2Body* b = world->GetBodyList(); b; b = b->GetNext()) {
        for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext()) {
            if (b->GetType() != b2_staticBody || f->GetType() != b2Shape::e_edge) continue;
            const b2EdgeShape* edge = static_cast<const b2EdgeShape*>(f->GetShape());
            if (edge->m_vertex1.x == edge->m_vertex2.x && f->GetFilterData().categoryBits == 4) {
                levelBody = b;
                walls.push_back(f);
            }
        }
    }
    if (!levelBody) return;
    for (b2Fixture* f : walls) levelBody->DestroyFixture(f);

    // In the browser game's frame (y down), then mirrored at mirrorY.
    const float mirrorY = kFlashStageHeightPx / kFlashPtm;
    const b2Vec2 lo = kFlashWorldLower, hi = kFlashWorldUpper;
    const b2AABB boxes[3] = {
        {b2Vec2(lo.x, lo.y), b2Vec2(lo.x + kFlashBorder, hi.y)},
        {b2Vec2(hi.x - kFlashBorder, lo.y), b2Vec2(hi.x, hi.y)},
        {b2Vec2(lo.x, lo.y), b2Vec2(hi.x, lo.y + kFlashBorder)},
    };
    b2Fixture* created[3];
    for (int i = 0; i < 3; ++i) {
        const b2AABB& box = boxes[i];
        b2PolygonShape shape;
        const b2Vec2 center(0.5f * (box.lowerBound.x + box.upperBound.x),
                            mirrorY - 0.5f * (box.lowerBound.y + box.upperBound.y));
        shape.SetAsBox(0.5f * (box.upperBound.x - box.lowerBound.x),
                       0.5f * (box.upperBound.y - box.lowerBound.y), center, 0.0f);
        b2FixtureDef def;
        def.shape = &shape;
        def.friction = 1.0f;
        def.restitution = 0.1f;
        def.filter.categoryBits = 8;
        def.filter.groupIndex = -10;
        created[i] = levelBody->CreateFixture(&def);
    }
    for (int i = 2; i >= 0; --i) levelBody->MoveFixtureToBack(created[i]);
}

}  // namespace

std::function<void()> g_afterWorldStep;

void flashWorldStep(b2World* world, float timeStep)
{
    if (!browserPhysics()) {
        world->Step(timeStep, 8, 3);
        return;
    }
    const bool blockSolve = g_blockSolve;
    g_blockSolve = false;
    if (!world->IsFlash20()) flash20Begin(world, nullptr, 0);
    g_flash20Solver = true;
    world->Step(timeStep, kFlashIterations, kFlashIterations);
    g_flash20Solver = false;
    g_blockSolve = blockSolve;
    if (g_afterWorldStep) g_afterWorldStep();
}

void setAfterWorldStepObserver(std::function<void()> observer) { g_afterWorldStep = std::move(observer); }

// The browser profile's condition (beginLevelTimeStep), known before the character exists.
bool flashOrderWanted()
{
    return flashLevel() && !g_offlineLevel && browserPhysicsOption();
}

bool browserPhysicsWanted() { return flashOrderWanted(); }

void flashCharacterBegin(b2World* world)
{
    if (!flashOrderWanted()) return;
    g_bodyHeadBefore = world->GetBodyList();
    g_jointHeadBefore = world->GetJointList();
    g_characterBodies.clear();
    g_characterJoints.clear();
}

void flashCharacterEnd(b2World* world)
{
    if (!flashOrderWanted()) return;
    for (b2Body* b = world->GetBodyList(); b && b != g_bodyHeadBefore; b = b->GetNext()) {
        g_characterBodies.push_back(b);
    }
    for (b2Joint* j = world->GetJointList(); j && j != g_jointHeadBefore; j = j->GetNext()) {
        g_characterJoints.push_back(j);
    }
}

void flashLevelBuilt(b2World* world)
{
    if (!flashOrderWanted()) return;
    useFlashBorders(world);
    for (auto it = g_characterBodies.rbegin(); it != g_characterBodies.rend(); ++it) {
        world->MoveBodyToFront(*it);
    }
    for (auto it = g_characterJoints.rbegin(); it != g_characterJoints.rend(); ++it) {
        world->MoveJointToFront(*it);
    }

    // The browser game builds the level, then the character (all its bodies, then its joints).
    // The lists are now in its order (newest first); bodies and joints of the level interleave as
    // they were made here.
    std::set<const void*> character(g_characterBodies.begin(), g_characterBodies.end());
    character.insert(g_characterJoints.begin(), g_characterJoints.end());
    std::vector<b2Body*> levelBodies, characterBodies;
    std::vector<b2Joint*> levelJoints, characterJoints;
    for (b2Body* b = world->GetBodyList(); b; b = b->GetNext()) {
        (character.count(b) ? characterBodies : levelBodies).insert(
            (character.count(b) ? characterBodies : levelBodies).begin(), b);
    }
    for (b2Joint* j = world->GetJointList(); j; j = j->GetNext()) {
        (character.count(j) ? characterJoints : levelJoints).insert(
            (character.count(j) ? characterJoints : levelJoints).begin(), j);
    }
    std::vector<std::pair<uint32, bool>> levelSlots;  // (serial, is a joint)
    for (b2Body* b : levelBodies) levelSlots.push_back({b2World::GetCreationSerial(b), false});
    for (b2Joint* j : levelJoints) levelSlots.push_back({b2World::GetCreationSerial(j), true});
    std::sort(levelSlots.begin(), levelSlots.end());
    std::vector<b2World::Flash20BuildStep> steps;
    size_t nextBody = 0, nextJoint = 0;
    for (const auto& slot : levelSlots) {
        if (slot.second) steps.push_back({nullptr, levelJoints[nextJoint++]});
        else steps.push_back({levelBodies[nextBody++], nullptr});
    }
    for (b2Body* b : characterBodies) steps.push_back({b, nullptr});
    for (b2Joint* j : characterJoints) steps.push_back({nullptr, j});
    flash20Begin(world, steps.data(), static_cast<int>(steps.size()));

    g_characterBodies.clear();
    g_characterJoints.clear();
}

void flashQueryAABB(b2World* world, b2QueryCallback* callback, const b2AABB& aabb)
{
    if (!world->IsFlash20()) {
        world->QueryAABB(callback, aabb);
        return;
    }
    b2Fixture* fixtures[30];
    const int32 count = world->Flash20Query(aabb, fixtures, 30);
    for (int32 i = 0; i < count; ++i) {
        if (!callback->ReportFixture(fixtures[i])) break;
    }
}

bool flashPersists(b2Body* body, b2Fixture* sensor)
{
    for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
        b2Contact* c = edge->contact;
        if ((c->GetFixtureA() == sensor || c->GetFixtureB() == sensor) && c->GetFlash20PersistCount() > 0) return true;
    }
    return false;
}

}  // namespace online
