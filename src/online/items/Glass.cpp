// ONLINE (PC addition): see Glass.h. Port of com.totaljerkface.game.level.userspecials.Glass,
// GlassShard and GlassShard2.
#include "online/items/Glass.h"

#include <cmath>

#include "BurstEmitter.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"
#include "Settings.h"
#include "Trigger.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(18, [] { return (LevelItem*)new (std::nothrow) Glass(); });

const Color4F kGlassFill(0x66 / 255.0f, 0xcc / 255.0f, 1.0f, 0.35f);  // 6737151, alpha 0.35

void suffixFor(float mass, std::string* suffix, int* particles, int light, int mid, int heavy)
{
    if (mass < 0.75f) {
        *suffix = "Light";
        *particles = light;
    } else if (mass < 4.0f) {
        *suffix = "Mid";
        *particles = mid;
    } else {
        *suffix = "Heavy";
        *particles = heavy;
    }
}

}  // namespace

// Flash ParticleController.createRectBurst("glass", ...): glass shards spread over the body.
Node* misc::glassBurst(b2Body* body, int count, float rangePx)
{
    Session* session = Settings::getInstance()->getCurrentSession();
    count = std::max(1, std::min(count, 120));
    if (!session || !session->canAddEmitter(count)) return nullptr;
    auto* emitter = new (std::nothrow) BurstEmitter();
    if (!emitter) return nullptr;
    emitter->setTotalParticles(count);
    __Array* textures = __Array::createWithCapacity(1);
    Sprite* sprite = Sprite::create("images/glassShard.png");
    const float w = sprite ? sprite->getTextureRect().size.width : 4.0f;
    const float range = rangePx * pointsPerFlashPx();
    if (sprite && emitter->init(textures, session->getPtmRatio(), session->getTimeStep(), session->_gravity,
                                w * 0.25f, w * 0.7f, 1.5707964f, 12.566371f, range, 20.0f, body, b2Vec2_zero,
                                Vec2::ZERO, count)) {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F(Color4B(140, 210, 255, 170)));
        emitter->autorelease();
        if (EmitterNode* particles = session->getParticlesForeground()) particles->addChild(emitter);
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Glass

Glass::~Glass()
{
    if (_mc) _mc->release();
    for (GlassShard* s : _shards) s->release();
}

bool Glass::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupBody;
    (void)groupOffset;
    const float x = num(element, "p0", 0.0f);
    const float y = num(element, "p1", 0.0f);
    // GlassRef scaleX 0.05..0.5, scaleY 0.5..5 of a 100 px square.
    const float w = std::max(5.0f, std::min(50.0f, num(element, "p2", 10.0f)));
    const float h = std::max(50.0f, std::min(500.0f, num(element, "p3", 100.0f)));
    const float angle = num(element, "p4", 0.0f);
    const bool sleeping = flag(element, "p5", false);
    int strength = inum(element, "p6", 10);
    if (strength == 0) strength = 10;
    strength = std::max(1, std::min(10, strength));
    _stabbing = flag(element, "p7", true);

    _halfW = w * 0.5f / kFlashPtm;
    _halfH = h * 0.5f / kFlashPtm;

    auto* draw = DrawNode::create();
    const float s = pointsPerFlashPx();
    draw->drawSolidRect(Vec2(-w * 0.5f * s, -h * 0.5f * s), Vec2(w * 0.5f * s, h * 0.5f * s), kGlassFill);
    _mc = draw;
    _mc->retain();
    if (Node* layer = flashBackgroundLayer()) layer->addChild(_mc);
    misc::placeFlash(_mc, x, y, angle);

    b2BodyDef bd;
    bd.type = b2_dynamicBody;
    bd.position = flashToWorld(x, y);
    bd.angle = flashAngle(angle);
    bd.awake = !sleeping;
    bd.userData = _mc;
    _body = getWorld()->CreateBody(&bd);
    b2PolygonShape box;
    box.SetAsBox(_halfW, _halfH);
    b2FixtureDef fd;
    fd.shape = &box;
    fd.density = 2.0f;
    fd.friction = 0.3f;
    fd.restitution = 0.1f;
    fd.filter.categoryBits = 8;
    _shape = _body->CreateFixture(&fd);
    getLevel()->addToPaintBody(_body);
    addToBeginContact(_shape);
    addToPostSolve(_shape);

    const float mass = _body->GetMass();
    suffixFor(mass, &_soundSuffix, &_glassParticles, 100, 150, 200);
    _smashImpulse = (int)std::lround(mass * strength);
    return true;
}

std::vector<b2Body*> Glass::getBodyList()
{
    std::vector<b2Body*> v;
    if (_body) v.push_back(_body);
    return v;
}

void Glass::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                      const b2ContactImpulse* impulse)
{
    (void)fixture;
    (void)otherFixture;
    if (_queued || _hasImpact || !_body) return;
    if (misc::maxNormalImpulse(contact, impulse) > _smashImpulse) {
        const b2Vec2 local = _body->GetLocalPoint(misc::contactPoint(contact));
        _impact = b2Vec2(local.x, -local.y);
        _hasImpact = true;
        _queued = true;
        getLevel()->addToSingleActions(this);
    }
}

void Glass::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    (void)fixture;
    if (!_body || _queued || otherFixture->IsSensor()) return;
    const float otherMass = otherFixture->GetBody()->GetMass();
    if (otherMass != 0.0f && otherMass < _body->GetMass()) return;
    if (misc::normalSpeed(contact) > 4.0f) {
        const int n = std::max(1, (int)std::ceil(misc::rand01() * 2.0f));
        createBodySound("GlassImpact" + std::to_string(std::min(2, n)), _body, 1.0f, false);
    }
}

void Glass::removeListeners()
{
    if (!_shape) return;
    removeBeginContact(_shape);
    removePostSolve(_shape);
}

void Glass::singleAction()
{
    if (_body && _hasImpact) shatter();
}

void Glass::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (action == 1) {
        if (_body) _body->SetAwake(true);
    } else if (action == 2) {
        if (_body) misc::applyFlashImpulse(_body, properties, true, _body->GetMass());
    } else if (action == 0) {
        if (_hasImpact || !_body) return;
        _body->SetAwake(true);
        // Flash: the trigger's position seen from the pane; the impact point is where the line
        // from the centre towards it leaves the rectangle.
        b2Vec2 from = _body->GetWorldCenter() + b2Vec2(0.0f, 1.0f);
        if (auto* t = dynamic_cast<Trigger*>(trigger)) from = b2Vec2(t->getXMeters(), t->getYMeters());
        const b2Vec2 wl = _body->GetLocalPoint(from);
        const b2Vec2 l(wl.x, -wl.y);  // Flash local (y down)
        const float slope = l.y / l.x;
        const float cx = l.x < 0 ? -_halfW : _halfW;
        const float cy = l.y < 0 ? -_halfH : _halfH;
        b2Vec2 perp;
        if ((cx == -_halfW && cy == -_halfH) || (cx == _halfW && cy == _halfH)) {
            perp = b2Vec2(-cy, cx);
        } else {
            perp = b2Vec2(cy, -cx);
        }
        if (b2Dot(l, perp) > 0.0f) {
            _impact = b2Vec2(cy / slope, cy);
        } else {
            _impact = b2Vec2(cx, cx * slope);
        }
        if (!std::isfinite(_impact.x) || !std::isfinite(_impact.y)) _impact = b2Vec2(0.0f, cy);
        _hasImpact = true;
        shatter();
    }
}

void Glass::shatter()
{
    removeListeners();
    LevelB2D* level = getLevel();
    b2World* world = getWorld();
    misc::glassBurst(_body, _glassParticles, std::max(_halfW, _halfH) * 2.0f * kFlashPtm);
    const int n = std::max(1, std::min(2, (int)std::ceil(misc::rand01() * 2.0f)));
    createBodySound("Glass" + _soundSuffix + std::to_string(n), _body, 1.0f, false);
    level->removeFromPaintBody(_body);

    const bool oldVersion = flashVersion() < 1.36f;
    const float mass = _body->GetMass();
    if (oldVersion && mass < 0.75f) {
        world->DestroyBody(_body);
        _body = nullptr;
        _shape = nullptr;
        _mc->removeFromParent();
        return;
    }

    const b2Vec2 center = _body->GetWorldCenter();
    const float angle = _body->GetAngle();
    const float angVel = _body->GetAngularVelocity();
    // Flash sides (local, y down): top, right, bottom, left - three points each.
    const float L = -_halfW, R = _halfW, T = -_halfH, B = _halfH;
    std::vector<std::vector<b2Vec2>> sides = {{{L, T}, {0, T}, {R, T}},
                                             {{R, T}, {R, 0}, {R, B}},
                                             {{R, B}, {0, B}, {L, B}},
                                             {{L, B}, {L, 0}, {L, T}}};
    bool keep[4] = {true, true, true, true};
    const float minDistEdge = 0.05f;
    if (_impact.x - (L + minDistEdge) <= 0) {
        _impact.x = L;
        keep[3] = false;
    } else if (_impact.x - (R - minDistEdge) >= 0) {
        _impact.x = R;
        keep[1] = false;
    }
    if (_impact.y - (T + minDistEdge) <= 0) {
        _impact.y = T;
        keep[0] = false;
    } else if (_impact.y - (B - minDistEdge) >= 0) {
        _impact.y = B;
        keep[2] = false;
    }

    // Velocities of the pane at each shard's centre, before the pane goes.
    struct ShardDef {
        b2Vec2 v[3];  // world-local (y up), metres
    };
    std::vector<ShardDef> defs;
    for (int side = 0; side < 4; side++) {
        if (!keep[side]) continue;
        for (int k = 0; k < 2; k++) {
            ShardDef d;
            const b2Vec2 p0 = _impact, p1 = sides[side][k], p2 = sides[side][k + 1];
            d.v[0] = b2Vec2(p0.x, -p0.y);
            d.v[1] = b2Vec2(p1.x, -p1.y);
            d.v[2] = b2Vec2(p2.x, -p2.y);
            const float area = 0.5f * std::fabs(b2Cross(d.v[1] - d.v[0], d.v[2] - d.v[0]));
            if (area < 2.0e-5f || (d.v[1] - d.v[0]).Length() < 0.01f || (d.v[2] - d.v[0]).Length() < 0.01f ||
                (d.v[2] - d.v[1]).Length() < 0.01f) {
                continue;  // Box2D 2.3 cannot make degenerate polygons
            }
            defs.push_back(d);
        }
    }
    std::vector<b2Vec2> velocities;
    for (const ShardDef& d : defs) {
        const b2Vec2 c = (1.0f / 3.0f) * (d.v[0] + d.v[1] + d.v[2]);
        velocities.push_back(_body->GetLinearVelocityFromWorldPoint(_body->GetWorldPoint(c)));
    }
    world->DestroyBody(_body);
    _body = nullptr;
    _shape = nullptr;

    Node* parent = _mc->getParent();
    const int z = _mc->getLocalZOrder();
    const float ptm = misc::ptm();
    for (size_t i = 0; i < defs.size(); i++) {
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position = center;
        bd.angle = angle;
        b2Body* body = world->CreateBody(&bd);
        b2PolygonShape tri;
        tri.Set(defs[i].v, 3);
        b2FixtureDef fd;
        fd.shape = &tri;
        fd.density = 2.0f;
        fd.friction = 0.3f;
        fd.restitution = 0.1f;
        fd.filter.categoryBits = 8;
        body->CreateFixture(&fd);
        body->SetAngularVelocity(angVel);
        body->SetLinearVelocity(velocities[i]);

        std::vector<Vec2> pts;
        for (int k = 0; k < 3; k++) pts.push_back(Vec2(defs[i].v[k].x * ptm, defs[i].v[k].y * ptm));
        Node* root = Node::create();
        auto* fill = DrawNode::create();
        fill->drawSolidPoly(pts.data(), 3, kGlassFill);
        root->addChild(fill, 0);
        if (parent) parent->addChild(root, z);
        body->SetUserData(root);
        misc::paintFromBody(root, body, body->GetPosition());
        getLevel()->addToPaintBody(body);
        _shards.push_back(new GlassShard(body, root, pts, !oldVersion, _stabbing));
    }
    _mc->removeFromParent();
}

// ---------------------------------------------------------------------------------------------
// GlassShard

GlassShard::GlassShard(b2Body* body, Node* root, const std::vector<Vec2>& triPoints, bool version2,
                       bool stabbing)
    : _body(body), _shape(body->GetFixtureList()), _root(root), _tri(triPoints), _v2(version2),
      _stabbing(stabbing)
{
    _root->retain();
    const float mass = _body->GetMass();
    _shatterImpulse = mass * 10.0f;
    if (!_v2) {
        _stabImpulse = mass * 2.0f;
        if (mass >= 0.15f) _fatal = true;
        suffixFor(mass, &_soundSuffix, &_glassParticles, 50, 100, 200);
    } else {
        if (_stabbing) {
            _stabImpulse = std::min(mass * 5.0f, 2.5f);
            if (mass < 0.1f) {
                _bloodParticles = 15;
                _stabImpulse = 0.0f;
            }
            float minX = 1e9f, maxX = -1e9f;
            for (const Vec2& p : _tri) {
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
            }
            const float widthPx = (maxX - minX) / pointsPerFlashPx();  // sprite.width (unrotated)
            if (mass >= 0.25f && widthPx >= 15.0f) _fatal = true;
        } else {
            _stabImpulse = _shatterImpulse;
        }
        suffixFor(mass, &_soundSuffix, &_glassParticles, 50, 100, 200);
    }
    addToPostSolve(_shape);
}

GlassShard::~GlassShard()
{
    if (_root) _root->release();
}

void GlassShard::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                           const b2ContactImpulse* impulse)
{
    if (_dead || !_resultListening) return;
    const float imp = misc::maxNormalImpulse(contact, impulse);
    _lastNormal[otherFixture->GetBody()] = misc::normalFrom(contact, fixture);
    if (imp <= _stabImpulse) return;
    if (imp > _shatterImpulse) {
        _stab = false;
        _resultListening = false;
        if (!_inSAA) getLevel()->addToSingleActions(this);
        _inSAA = true;
        return;
    }
    if ((getLevel()->getFixtureMaterial(otherFixture) & 2) && !_sensor) {
        _stab = true;
        if (!_inSAA) getLevel()->addToSingleActions(this);
        _inSAA = true;
    }
}

void GlassShard::singleAction()
{
    if (_dead) return;
    _inSAA = false;
    if (_stab) {
        _stab = false;
        b2Filter f = _shape->GetFilterData();
        f.maskBits = 8;
        _shape->SetFilterData(f);
        auto* poly = static_cast<b2PolygonShape*>(_shape->GetShape());
        b2PolygonShape copy = *poly;
        b2FixtureDef fd;
        fd.shape = &copy;
        fd.isSensor = true;
        fd.density = 0.0f;
        fd.filter.categoryBits = 8;
        fd.filter.maskBits = 4;
        _sensor = _body->CreateFixture(&fd);
        _body->ResetMassData();
        if (!_inActions) getLevel()->addToActions(this);
        _inActions = true;
        _resultListening = true;
        addToBeginContact(_sensor);
        addToEndContact(_sensor);
        return;
    }
    // Shatter.
    _dead = true;
    LevelB2D* level = getLevel();
    level->removeFromPaintBody(_body);
    if (_inActions) level->removeFromActions(this);
    _inActions = false;
    misc::glassBurst(_body, _glassParticles, 10.0f);
    const int n = std::max(1, std::min(2, (int)std::ceil(misc::rand01() * 2.0f)));
    createBodySound("Glass" + _soundSuffix + std::to_string(n), _body, 1.0f, false);
    removePostSolve(_shape);
    if (_sensor) {
        removeBeginContact(_sensor);
        removeEndContact(_sensor);
    }
    for (auto& e : _bj) getSession()->getDestructionListener()->removeJointListener(this, e.second);
    for (auto& e : _bj) getSession()->getDestructionListener()->removeFixtureListener(this, e.first->GetFixtureList());
    _bj.clear();
    _count.clear();
    _bodiesToAdd.clear();
    _bodiesToRemove.clear();
    getWorld()->DestroyBody(_body);  // its joints go with it
    _body = nullptr;
    _shape = nullptr;
    _sensor = nullptr;
    _root->removeFromParent();
}

void GlassShard::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    (void)contact;
    if (_dead || fixture != _sensor) return;
    if (!(getLevel()->getFixtureMaterial(otherFixture) & 2)) return;
    b2Body* other = otherFixture->GetBody();
    b2Vec2 normal;
    auto it = _lastNormal.find(other);
    if (it != _lastNormal.end()) {
        normal = it->second;
    } else {
        normal = other->GetWorldCenter() - _body->GetWorldCenter();
        if (normal.Normalize() < 1e-6f) normal = b2Vec2(0.0f, 1.0f);
    }
    _bodiesToAdd.push_back(std::make_pair(other, normal));
}

void GlassShard::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    (void)contact;
    if (_dead || fixture != _sensor) return;
    if (!(getLevel()->getFixtureMaterial(otherFixture) & 2)) return;
    b2Body* other = otherFixture->GetBody();
    bool pending = false;
    for (auto& p : _bodiesToAdd) pending = pending || p.first == other;
    if (_bj.count(other) || pending) _bodiesToRemove.push_back(other);
}

void GlassShard::actions()
{
    if (_dead) return;
    if (_fleshSoundFrames > 0) {
        _fleshSoundFrames -= _clock.advance(getTimeStepOverFlashTimeStep());
    }
    if (!_bodiesToAdd.empty()) {
        auto adds = _bodiesToAdd;
        _bodiesToAdd.clear();
        for (auto& p : adds) createPrisJoint(p.first, p.second);
    }
    if (!_bodiesToRemove.empty()) {
        auto removes = _bodiesToRemove;
        _bodiesToRemove.clear();
        for (b2Body* b : removes) removeJoint(b);
        if (_v2) checkZeroJoints();
    }
}

void GlassShard::createPrisJoint(b2Body* other, b2Vec2 normal)
{
    auto found = _bj.find(other);
    if (found != _bj.end()) {
        _count[other]++;
        return;
    }
    b2Fixture* first = other->GetFixtureList();
    if (!first) return;
    if (auto* item = static_cast<LevelItem*>(first->GetUserData())) {
        item->shapeImpale(first, _fatal, b2Vec2(INFINITY, INFINITY), 0.0f);
    }
    b2PrismaticJointDef jd;
    const b2Vec2 anchor = other->GetWorldCenter();
    jd.Initialize(_body ? _body : getLevelBody(), other, anchor, normal);
    jd.collideConnected = true;
    jd.enableMotor = true;
    jd.maxMotorForce = 30.0f;
    jd.motorSpeed = 0.0f;
    auto* joint = (b2PrismaticJoint*)getWorld()->CreateJoint(&jd);
    _bj[other] = joint;
    _count[other] = 1;
    getSession()->getDestructionListener()->addJointListener(joint, this);
    getSession()->getDestructionListener()->addFixtureListener(first, this);
    if (other == _previousBody) return;
    _previousBody = other;
    if (EmitterNode* particles = getSession()->getParticlesForeground()) {
        const float ptm = misc::ptm();
        if (Emitter* blood = BurstEmitter::createBloodBurst(5.0f, 15.0f, Vec2(anchor.x * ptm, anchor.y * ptm),
                                                            _bloodParticles)) {
            particles->addChild(blood);
        }
    }
    paintBlood(anchor);
    if (_fleshSoundFrames <= 0) {
        const int n = std::max(1, std::min(3, (int)std::ceil(misc::rand01() * 3.0f)));
        createBodySound("ImpaleSpikes" + std::to_string(n), _body ? _body : other, 1.0f, false);
        _fleshSoundFrames = 30;
    }
}

void GlassShard::removeJoint(b2Body* other)
{
    auto it = _count.find(other);
    if (it == _count.end()) return;
    if (--it->second > 0) return;
    auto j = _bj.find(other);
    if (j != _bj.end()) {
        getSession()->getDestructionListener()->removeJointListener(this, j->second);
        getWorld()->DestroyJoint(j->second);
        _bj.erase(j);
    }
    _count.erase(it);
}

void GlassShard::checkZeroJoints()
{
    if (!_bj.empty() || !_sensor) return;
    b2Filter f;
    f.categoryBits = 8;
    _shape->SetFilterData(f);
    _resultListening = true;
    removeBeginContact(_sensor);
    removeEndContact(_sensor);
    _body->DestroyFixture(_sensor);
    _sensor = nullptr;
    getLevel()->removeFromActions(this);
    _inActions = false;
}

void GlassShard::forget(b2Body* other)
{
    _bj.erase(other);
    _count.erase(other);
    _lastNormal.erase(other);
    for (auto it = _bodiesToAdd.begin(); it != _bodiesToAdd.end();) {
        it = it->first == other ? _bodiesToAdd.erase(it) : it + 1;
    }
    _bodiesToRemove.erase(std::remove(_bodiesToRemove.begin(), _bodiesToRemove.end(), other),
                          _bodiesToRemove.end());
    if (_previousBody == other) _previousBody = nullptr;
}

void GlassShard::jointWillBeDestroyed(b2Joint* joint)
{
    for (auto it = _bj.begin(); it != _bj.end(); ++it) {
        if (it->second == joint) {
            _count.erase(it->first);
            _bj.erase(it);
            return;
        }
    }
}

void GlassShard::fixtureWillBeDestroyed(b2Fixture* fixture)
{
    // An impaled body part is going away (dismemberment etc.).
    b2Body* other = fixture->GetBody();
    if (other != _body) forget(other);
}

void GlassShard::paintBlood(const b2Vec2& worldPoint)
{
    if (!_body || !hasFlashArt("glass_blood")) return;
    if (!_bloodClip) {
        auto* stencil = DrawNode::create();
        stencil->drawSolidPoly(_tri.data(), 3, Color4F::WHITE);
        _bloodClip = ClippingNode::create(stencil);
        _root->addChild(_bloodClip, 1);
    }
    Sprite* blood = createFlashSprite("glass_blood");
    if (!blood) return;
    const b2Vec2 local = _body->GetLocalPoint(worldPoint);
    const float ptm = misc::ptm();
    blood->setPosition(Vec2(local.x * ptm, local.y * ptm));
    blood->setRotation(misc::rand01() * 360.0f);
    if (misc::rand01() > 0.5f) blood->setScaleX(-blood->getScaleX());
    _bloodClip->addChild(blood);
}

}  // namespace online
