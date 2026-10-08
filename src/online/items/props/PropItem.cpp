// ONLINE (PC addition): see PropItem.h.
#include "online/items/props/PropItem.h"

#include <algorithm>
#include <cmath>

#include "cocos2d.h"

#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"
#include "Sound.h"
#include "StageCamera.h"
#include "online/FlashPhysics.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

// Flash ParticleController.maxParticles is 2000 for everything; these sprites are a separate
// budget next to the mobile emitters, so keep it smaller.
int s_liveParticles = 0;
const int kMaxParticles = 1200;

float rnd() { return CCRANDOM_0_1(); }

}  // namespace

PropItem::~PropItem()
{
    if (_root) _root->release();
}

Vec2 PropItem::pt(float xPx, float yPx)
{
    const float ppf = pointsPerFlashPx();
    return Vec2(xPx * ppf, -yPx * ppf);
}

Node* PropItem::art(const std::string& name, float w, float h, float cx, float cy, const Color4F& color)
{
    if (Sprite* s = createFlashSprite(name)) return s;
    // Art not extracted: a plain box of the shape's size.
    DrawNode* d = DrawNode::create();
    const Vec2 c = pt(cx, cy);
    const float ppf = pointsPerFlashPx();
    d->drawSolidRect(c - Vec2(w, h) * (0.5f * ppf), c + Vec2(w, h) * (0.5f * ppf), color);
    return d;
}

void PropItem::placeRoot(LevelDataElement* element, b2Body* groupBody, bool foreground)
{
    _inGroup = groupBody != nullptr;
    _foreground = foreground;
    const float x = num(element, "p0", 0.0f);
    const float y = num(element, "p1", 0.0f);
    const float angle = num(element, "p2", 0.0f);
    _root = Node::create();
    _root->retain();
    _root->setCascadeOpacityEnabled(true);
    Node* layer = (foreground || element->stringAttribute("fg")) ? flashForegroundLayer()
                                                                 : flashBackgroundLayer();
    if (layer) layer->addChild(_root);
    const b2Vec2 m = flashToWorld(x, y);
    _root->setPosition(Vec2(m.x, m.y) * getPtm());
    _root->setRotation(angle);  // both clockwise degrees
}

void PropItem::paintWithOffsetPoints(Vec2 offset, float rotation)
{
    if (!_root) return;
    _root->setPosition(offset);
    _root->setRotation(rotation);
}

void PropItem::setOpacity(float opacity)
{
    if (_root) _root->setOpacity((GLubyte)std::lround(std::max(0.0f, std::min(1.0f, opacity)) * 255.0f));
}

// ---- bodies ----------------------------------------------------------------------------------

b2Body* PropItem::createBody(const b2Vec2& position, float angle, bool sleeping)
{
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = position;
    def.angle = angle;
    def.awake = !sleeping;
    return getWorld()->CreateBody(&def);
}

static b2Fixture* makeFixture(b2Body* body, const b2Shape& shape, const PropFixture& d)
{
    b2FixtureDef def;
    def.shape = &shape;
    def.density = d.density;
    def.friction = d.friction;
    def.restitution = d.restitution;
    def.filter.categoryBits = d.category;
    def.filter.maskBits = d.mask;
    return body->CreateFixture(&def);
}

b2Fixture* PropItem::addBox(b2Body* body, float hwPx, float hhPx, float cxPx, float cyPx,
                            const PropFixture& def)
{
    b2PolygonShape shape;
    shape.SetAsBox(hwPx / 62.5f, hhPx / 62.5f, propLocal(cxPx, cyPx), 0.0f);
    return makeFixture(body, shape, def);
}

b2Fixture* PropItem::addPolygon(b2Body* body, const std::vector<b2Vec2>& flashPx, const PropFixture& def)
{
    b2Vec2 v[b2_maxPolygonVertices];
    const int n = std::min((int)flashPx.size(), (int)b2_maxPolygonVertices);
    for (int i = 0; i < n; i++) v[i] = propLocal(flashPx[i].x, flashPx[i].y);
    b2PolygonShape shape;
    shape.Set(v, n);  // convex hull: Flash's clockwise (y down) order needs no flipping
    return makeFixture(body, shape, def);
}

b2Fixture* PropItem::addCircle(b2Body* body, float radiusPx, const PropFixture& def)
{
    b2CircleShape shape;
    shape.m_radius = radiusPx / 62.5f;
    return makeFixture(body, shape, def);
}

void PropItem::paintBody(b2Body* body, Node* node)
{
    _painted.push_back({body, node});
    if (!_paintRegistered) {
        _paintRegistered = true;
        getLevel()->addToPaintItem(this);
    }
    paint();
}

void PropItem::unpaintBody(b2Body* body)
{
    _painted.erase(std::remove_if(_painted.begin(), _painted.end(),
                                  [body](const Painted& p) { return p.body == body; }),
                   _painted.end());
}

void PropItem::paint()
{
    const float ptm = getPtm();
    for (const Painted& p : _painted) {
        const b2Vec2 c = p.body->GetWorldCenter();
        p.node->setPosition(Vec2(c.x * ptm, c.y * ptm));
        p.node->setRotation(p.body->GetAngle() * -57.29578f);
    }
}

void PropItem::destroyFixture(b2Fixture* fixture)
{
    if (!fixture) return;
    forgetImpulse(fixture);
    forgetHits(fixture);
    getLevel()->removeFixtureMaterial(fixture);
    _contactAddBufferDict.erase(fixture);
    fixture->GetBody()->DestroyFixture(fixture);
}

void PropItem::destroyBody(b2Body* body)
{
    if (!body) return;
    for (b2Fixture* f = body->GetFixtureList(); f; f = f->GetNext()) {
        forgetImpulse(f);
        forgetHits(f);
        getLevel()->removeFixtureMaterial(f);
        _contactAddBufferDict.erase(f);
    }
    unpaintBody(body);
    for (Spray& s : _sprays) {
        if (s.body == body) s.finished = true, s.body = nullptr;
    }
    getWorld()->DestroyBody(body);
}

void PropItem::listenImpulse(b2Fixture* fixture)
{
    if (std::find(_impulseFixtures.begin(), _impulseFixtures.end(), fixture) == _impulseFixtures.end())
        _impulseFixtures.push_back(fixture);
    addToPostSolve(fixture);
}

void PropItem::forgetImpulse(b2Fixture* fixture)
{
    auto it = std::find(_impulseFixtures.begin(), _impulseFixtures.end(), fixture);
    if (it == _impulseFixtures.end()) return;
    _impulseFixtures.erase(it);
    removePostSolve(fixture);
}

void PropItem::listenHits(b2Fixture* fixture)
{
    if (std::find(_hitFixtures.begin(), _hitFixtures.end(), fixture) == _hitFixtures.end())
        _hitFixtures.push_back(fixture);
    addToBeginContact(fixture);
}

void PropItem::forgetHits(b2Fixture* fixture)
{
    auto it = std::find(_hitFixtures.begin(), _hitFixtures.end(), fixture);
    if (it == _hitFixtures.end()) return;
    _hitFixtures.erase(it);
    removeBeginContact(fixture);
    _contactAddBufferDict.erase(fixture);
}

// ---- contacts --------------------------------------------------------------------------------

void PropItem::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (std::find(_hitFixtures.begin(), _hitFixtures.end(), fixture) == _hitFixtures.end()) return;
    contactSoundHandler(fixture, otherFixture, contact, nullptr);
}

void PropItem::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                         const b2ContactImpulse* impulse)
{
    if (std::find(_impulseFixtures.begin(), _impulseFixtures.end(), fixture) == _impulseFixtures.end())
        return;
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2)
        maxImpulse = std::max(maxImpulse, impulse->normalImpulses[1]);
    onImpulse(fixture, otherFixture, contact, maxImpulse);
}

void PropItem::actions()
{
    if (!_contactAddBufferDict.empty()) {
        std::vector<b2Fixture*> hits;
        for (auto& e : _contactAddBufferDict) hits.push_back(e.first);
        _contactAddBufferDict.clear();
        for (b2Fixture* f : hits) {
            if (std::find(_hitFixtures.begin(), _hitFixtures.end(), f) != _hitFixtures.end()) onHit(f);
        }
    }
    stepParticles();
}

// ---- triggers --------------------------------------------------------------------------------

std::vector<b2Body*> PropItem::getBodyList()
{
    std::vector<b2Body*> bodies;
    if (_body) bodies.push_back(_body);
    return bodies;
}

void PropItem::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    (void)trigger;
    if (!_body) return;
    if (action == 0) {  // wake from sleep
        _body->SetAwake(true);
    } else if (action == 1) {  // apply impulse: impulseX, impulseY (per unit mass), spin
        // Missing properties are NaN in Flash (which would wreck the body); skip those parts.
        LevelB2D* level = getLevel();
        if (properties.size() >= 2 && std::isfinite(properties[0]) && std::isfinite(properties[1])) {
            float ix = properties[0];
            float iy = properties[1];
            level->convertDirectionIfNecessaryBasedOnRegistration(&iy);
            const float mass = _body->GetMass();
            _body->ApplyLinearImpulse(b2Vec2(ix * mass, iy * mass), _body->GetWorldCenter(), true);
        }
        if (properties.size() >= 3 && std::isfinite(properties[2])) {
            float spin = properties[2];
            level->convertDirectionIfNecessaryBasedOnRegistration(&spin);
            _body->SetAngularVelocity(_body->GetAngularVelocity() + spin);
        }
    }
}

// ---- sounds ----------------------------------------------------------------------------------

void PropItem::playSound(const std::string& name, b2Body* body, bool bodyStays)
{
    if (!body) return;
    if (bodyStays) {
        createBodySound(name, body, 1.0f, false);
    } else {
        const b2Vec2 p = body->GetPosition();
        createPositionSound(name, Vec2(p.x, p.y), 1.0f, false);
    }
}

void PropItem::playGatedSound(int slot, const std::string& name, b2Body* body, bool bodyStays)
{
    std::shared_ptr<bool>& busy = _soundBusy[slot & 1];
    if (busy && *busy) return;
    if (!body) return;
    Sound* sound;
    if (bodyStays) {
        sound = createBodySound(name, body, 1.0f, false);
    } else {
        const b2Vec2 p = body->GetPosition();
        sound = createPositionSound(name, Vec2(p.x, p.y), 1.0f, false);
    }
    if (!sound) return;
    busy = std::make_shared<bool>(true);
    std::shared_ptr<bool> flag = busy;
    sound->setFinishCallback([flag](int&) { *flag = false; });
}

// ---- particles -------------------------------------------------------------------------------

Sprite* PropItem::particleSprite(const std::string& prefix, int frames)
{
    const int frame = 1 + std::min(frames - 1, (int)std::floor(rnd() * frames));
    Sprite* s = createFlashSprite(prefix + std::to_string(frame));
    if (!s) {
        // No art: a small grey chip.
        s = Sprite::create();
        s->setTextureRect(Rect(0, 0, 3 * pointsPerFlashPx(), 3 * pointsPerFlashPx()));
        s->setColor(Color3B(150, 150, 150));
    }
    s->setAnchorPoint(Vec2(0.5f, 0.5f));  // Flash: bitmap at point - size / 2
    return s;
}

void PropItem::addParticle(Node* layer, Sprite* sprite, const b2Vec2& worldM, const b2Vec2& velM)
{
    const float ptm = getPtm();
    Particle p;
    p.sprite = sprite;
    p.pos = Vec2(worldM.x * ptm, worldM.y * ptm);
    p.vel = Vec2(velM.x * ptm, velM.y * ptm);
    sprite->setPosition(p.pos);
    layer->addChild(sprite);
    _particles.push_back(p);
    ++s_liveParticles;
    getLevel()->addToActions(this);
}

void PropItem::rectBurst(const std::string& prefix, int frames, float speedRange, b2Body* body, int total)
{
    if (!body || !body->GetFixtureList()) return;
    if (!_burstLayer) {
        Session* session = getSession();
        _burstLayer = session ? static_cast<Node*>(session->getParticlesForeground()) : nullptr;
        if (!_burstLayer) _burstLayer = flashForegroundLayer();
        if (!_burstLayer) return;
    }
    // Bounds of the first fixture (Flash: GetShapeList() as b2PolygonShape).
    b2Vec2 lo(1e4f, 1e4f), hi(-1e4f, -1e4f);
    const b2Shape* shape = body->GetFixtureList()->GetShape();
    if (shape->GetType() == b2Shape::e_polygon) {
        const b2PolygonShape* poly = static_cast<const b2PolygonShape*>(shape);
        for (int i = 0; i < poly->m_count; i++) {
            lo = b2Min(lo, poly->m_vertices[i]);
            hi = b2Max(hi, poly->m_vertices[i]);
        }
    } else {
        const b2CircleShape* c = static_cast<const b2CircleShape*>(shape);
        lo = c->m_p - b2Vec2(c->m_radius, c->m_radius);
        hi = c->m_p + b2Vec2(c->m_radius, c->m_radius);
    }
    const float half = speedRange * 0.5f;
    for (int i = 0; i < total; i++) {
        if (s_liveParticles >= kMaxParticles) return;
        const b2Vec2 offset(lo.x + rnd() * (hi.x - lo.x), lo.y + rnd() * (hi.y - lo.y));
        b2Vec2 v = body->GetLinearVelocityFromLocalPoint(offset);
        v.x += rnd() * speedRange - half;
        v.y += rnd() * speedRange - half;
        addParticle(_burstLayer, particleSprite(prefix, frames), body->GetWorldPoint(offset), v);
    }
}

Node* PropItem::spray(const std::string& prefix, int frames, b2Body* body, float sx, float sy,
                      float ex, float ey, float minSpeed, float maxSpeed, float angleRangeDeg,
                      int perFrame, int total)
{
    Node* bg = flashBackgroundLayer();
    if (!bg || !body) return nullptr;
    Spray s;
    s.prefix = prefix;
    s.frames = frames;
    s.body = body;
    s.start = propLocal(sx, sy);
    s.range = propLocal(ex, ey) - s.start;
    s.angleRange = angleRangeDeg * 3.14159265f / 180.0f;
    s.rot = std::atan2(ey - sy, ex - sx) + (3.14159265f * 0.5f - s.angleRange * 0.5f);
    s.minSpeed = minSpeed;
    s.maxSpeed = maxSpeed;
    s.perFrame = perFrame;
    s.total = total;
    s.count = 0;
    s.layer = Node::create();
    bg->addChild(s.layer);
    s.finished = false;
    _sprays.push_back(s);
    getLevel()->addToActions(this);
    return s.layer;
}

void PropItem::bringToFront(Node* node)
{
    if (!node || !node->getParent()) return;
    Node* parent = node->getParent();
    node->retain();
    node->removeFromParentAndCleanup(false);
    parent->addChild(node);
    node->release();
}

void PropItem::stepParticles()
{
    if (_particles.empty() && _sprays.empty()) return;
    const float ptm = getPtm();
    float limit = -1e30f;
    if (Session* session = getSession()) {
        if (StageCamera* cam = session->getCamera()) limit = cam->getYParticleLimit();
    }
    // Flash Particle.step: move, then gravity (1/3 m/s per 30 Hz frame = 10 m/s^2).
    // One world step (1/60, or 1/30 with the browser physics profile, online/FlashPhysics.h).
    const float dt = LevelItem::s_timeStep;
    const float gravity = 10.0f * ptm * dt;
    for (size_t i = 0; i < _particles.size();) {
        Particle& p = _particles[i];
        p.pos += p.vel * dt;
        p.vel.y -= gravity;
        if (p.pos.y < limit) {
            p.sprite->removeFromParentAndCleanup(true);
            --s_liveParticles;
            _particles[i] = _particles.back();
            _particles.pop_back();
            continue;
        }
        p.sprite->setPosition(p.pos);
        ++i;
    }
    // Sprays emit perFrame particles per Flash frame (every second 60 Hz step; every step with
    // the browser physics profile).
    ++_stepCounter;
    if ((_stepCounter % stepsPerFlashFrame()) == 0) {
        for (Spray& s : _sprays) {
            if (s.finished || !s.body) continue;
            for (int k = 0; k < s.perFrame; k++) {
                if (s_liveParticles >= kMaxParticles) break;
                if (s.count > s.total) {
                    s.finished = true;
                    break;
                }
                ++s.count;
                const float r = rnd();
                const b2Vec2 local = s.start + r * s.range;
                const float speed = rnd() * (s.maxSpeed - s.minSpeed) + s.minSpeed;
                // Flash angle (y down): body angle + rot + random part of the range.
                const float a = -s.body->GetAngle() + s.rot + s.angleRange * rnd();
                b2Vec2 v = s.body->GetLinearVelocityFromLocalPoint(local);
                v.x += std::cos(a) * speed;
                v.y -= std::sin(a) * speed;
                addParticle(s.layer, particleSprite(s.prefix, s.frames), s.body->GetWorldPoint(local), v);
            }
        }
    }
}

}  // namespace online
