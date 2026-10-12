// RESTORED (PC addition): Helicopter Man's helicopter, ported from the browser game's
// HelicopterMan.as and heli/BladeShard.as, VPoint.as, VSpring.as (Flash v1.87) onto the mobile
// Vehicle framework. See Helicopter.h.

#include "Helicopter.h"

#include <algorithm>
#include <cmath>

#include "BurstEmitter.h"
#include "CharacterB2D.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "FlashParticles.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "online/FlashPhysics.h"  // RESTORED (PC addition): per-step constants at 1/30 too

USING_NS_CC;

namespace {

const float kSym = 1.0f / 125.0f;   // metres per Flash symbol px (character_scale 125)
const float kPx = 1.0f / 62.5f;     // metres per Flash world px (Flash m_physScale 62.5)
const float kDeg = 0.017453292f;
const float kPi = 3.14159265f;

// Rider parts counted in the hover mass (Flash COMArray).
enum ComSlot
{
    ComHead, ComChest, ComPelvis, ComUpperArm1, ComLowerArm1, ComUpperArm2, ComLowerArm2,
    ComUpperLeg1, ComLowerLeg1, ComUpperLeg2, ComLowerLeg2, ComUpperArm3, ComUpperArm4,
    ComUpperLeg3, ComUpperLeg4,
};

void limitJoint(b2RevoluteJoint* joint, b2Body* a, b2Body* b, float lowerDeg, float upperDeg)
{
    if (!joint || !a || !b) {
        return;
    }
    float angle = b->GetAngle() - a->GetAngle();
    joint->SetLimits(-upperDeg * kDeg - angle, -lowerDeg * kDeg - angle);
}

float wrapAngle(float a)
{
    while (a > kPi) {
        a -= 2.0f * kPi;
    }
    while (a < -kPi) {
        a += 2.0f * kPi;
    }
    return a;
}

b2Vec2 contactPoint(b2Contact* contact)
{
    b2WorldManifold wm;
    contact->GetWorldManifold(&wm);
    int n = contact->GetManifold()->pointCount;
    if (n == 2) {
        return 0.5f * (wm.points[0] + wm.points[1]);
    }
    if (n == 1) {
        return wm.points[0];
    }
    return contact->GetFixtureA()->GetBody()->GetWorldCenter();
}

// Sutherland-Hodgman: the part of `poly` with y >= c (keepAbove) or y <= c.
std::vector<b2Vec2> clipY(const std::vector<b2Vec2>& poly, float c, bool keepAbove)
{
    std::vector<b2Vec2> out;
    auto inside = [&](const b2Vec2& p) { return keepAbove ? p.y >= c : p.y <= c; };
    for (size_t i = 0; i < poly.size(); i++) {
        const b2Vec2& a = poly[i];
        const b2Vec2& b = poly[(i + 1) % poly.size()];
        bool ia = inside(a);
        bool ib = inside(b);
        if (ia) {
            out.push_back(a);
        }
        if (ia != ib && b.y != a.y) {
            float t = (c - a.y) / (b.y - a.y);
            out.push_back(b2Vec2(a.x + t * (b.x - a.x), c));
        }
    }
    return out;
}

}  // namespace

Helicopter* Helicopter::create(Vec2 position, std::string name, int groupID)
{
    Helicopter* heli = new (std::nothrow) Helicopter();
    if (heli && heli->init(position, name, groupID)) {
        heli->autorelease();
        return heli;
    }
    delete heli;
    return nullptr;
}

Helicopter::Helicopter()
    : _impulseLeft(1.0f),
      _impulseRight(1.0f),
      _impulseOffset(1.0f),
      _maxSpinAV(3.5f),
      _copterSmashLimit(40.0f),
      _bladeSmashLimit(30.0f),
      _ejectImpulse(2.0f),
      _accelStep(0.025f / 4.0f),
      _decelStep(0.02f / 4.0f),
      _maxStep(0.1f / 2.0f),
      _ropeMaxLength(3.0f),
      _ropeSpeed(0.05f / 2.0f),
      _numRopeSegments(20),
      _rider(nullptr),
      _riderEjected(false),
      _hoverState(0),
      _isLoud(false),
      _targetAng(0.0f),
      _spinAcceleration(0.0f),
      _copterSmashed(false),
      _bladeSmashed(false),
      _legOn{true, true},
      _magnetized(false),
      _spaceOff(false),
      _soundDelay(20),
      _soundDelayCount(0),
      _bladeImpactSound(false),
      _frameCounter(0),
      _copterBody(nullptr),
      _magnetBody(nullptr),
      _bladeFixture(nullptr),
      _stemFixture(nullptr),
      _baseFixture(nullptr),
      _backFixture(nullptr),
      _legFixture{nullptr, nullptr},
      _wheelFixture{nullptr, nullptr},
      _magnetFixture(nullptr),
      _ropeJoint(nullptr),
      _ropeMinLength(0.0f),
      _bladeResult(0.0f),
      _copterResult(false),
      _legResult{false, false},
      _copterSprite(nullptr),
      _propellerSprite(nullptr),
      _brokenPropellerSprite(nullptr),
      _frontSprite(nullptr),
      _legSprite{nullptr, nullptr},
      _magnetSprite(nullptr),
      _ropeNode(nullptr),
      _heliLoop(nullptr),
      _magnetLoop(nullptr)
{
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
}

Helicopter::~Helicopter()
{
    for (Sound* s : {_heliLoop, _magnetLoop}) {
        if (s) {
            s->setFinishCallback(nullptr);
        }
    }
    for (Shard& shard : _shards) {
        if (shard.fleshSound) {
            shard.fleshSound->setFinishCallback(nullptr);
        }
    }
}

bool Helicopter::gameplay()
{
    return getSession()->getMode() == SessionModeGameplay;
}

bool Helicopter::init(Vec2 position, std::string name, int groupID)
{
    _maxTorque = 100000.0f;
    bool ok = Vehicle::init(position, name, groupID);
    if (ok) {
        getLevel()->addToPaintItem(this);
    }
    return ok;
}

// ---- helpers ---------------------------------------------------------------------------------

ValueMap Helicopter::shape(const std::string& name)
{
    return _bodiesDict.at("bodies").asValueMap().at(name).asValueMap();
}

b2Vec2 Helicopter::guide(const std::string& name)
{
    Vec2 p = PointFromString(_bodiesDict.at("joints").asValueMap().at(name).asString());
    return b2Vec2(p.x, p.y);
}

// The copter body sits on the guide origin with angle 0: guide positions are body-local.
b2Fixture* Helicopter::addBox(b2Body* body, b2FixtureDef def, const std::string& name)
{
    ValueMap data = shape(name);
    return createFixture(body, def, &data, true, true);
}

b2Fixture* Helicopter::addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix, int count)
{
    b2Vec2 vertices[b2_maxPolygonVertices];
    for (int i = 0; i < count; i++) {
        vertices[i] = guide(prefix + patch::to_string(i));
    }
    b2PolygonShape polygon;
    polygon.Set(vertices, count);
    def.shape = &polygon;
    return body->CreateFixture(&def);
}

Sprite* Helicopter::sprite(const std::string& frame, Node* parent, int z)
{
    Sprite* s = Sprite::createWithSpriteFrameName(_name + "_" + frame + ".png");
    if (s && parent) {
        parent->addChild(s, z);
    }
    return s;
}

void Helicopter::watch(b2Fixture* fixture)
{
    if (_watched.insert(fixture).second) {
        getSession()->getDestructionListener()->addFixtureListener(fixture, this);
    }
}

void Helicopter::unwatchAll()
{
    DestructionListener* listener = getSession()->getDestructionListener();
    for (b2Fixture* f : _watched) {
        listener->removeFixtureListener(this, f);
    }
    _watched.clear();
}

void Helicopter::startLoop(Sound*& sound, const std::string& name, b2Body* body, float volume, float fade)
{
    if (sound || !gameplay() || !body) {
        return;
    }
    sound = createBodySound(name, body, 1.0f, true);
    if (sound) {
        Sound** slot = &sound;
        sound->setFinishCallback([slot](int&) { *slot = nullptr; });
        sound->setMaxVolume(0.0f);
        sound->fadeTo(volume, fade, false);
    }
}

void Helicopter::stopLoop(Sound*& sound, float fade)
{
    if (sound) {
        sound->fadeTo(0.0f, fade, true);
        sound->setFinishCallback(nullptr);
        sound = nullptr;
    }
}

// ---- creation --------------------------------------------------------------------------------

// Flash stacking: magnet, rope and copter just behind the head (the vehicle background, between
// the far limbs and the torso), copterFront between the near leg and the near arm.
void Helicopter::createSprites()
{
    Node* background = getSession()->getVehicleBackground();
    _magnetSprite = sprite("magnet", background, 1);
    _ropeNode = DrawNode::create();
    background->addChild(_ropeNode, 2);
    _copterSprite = sprite("copter", background, 3);
    _propellerSprite = sprite("propeller_1", background, 4);
    if (!gameplay()) {
        return;
    }
    _brokenPropellerSprite = sprite("brokenPropeller", background, 4);
    if (_brokenPropellerSprite) {
        _brokenPropellerSprite->setVisible(false);
    }
}

void Helicopter::createBodies()
{
    b2World* world = getWorld();
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.allowSleep = false;
    bodyDef.position.Set(_origin.x, _origin.y);
    bodyDef.linearDamping = 0.4f;
    _copterBody = world->CreateBody(&bodyDef);

    // Flash: density 3, friction 0.3, restitution 0.1; blade, stem, base and back with the zero
    // filter, the legs with the rider's default filter (the handle box is never created: its
    // polygon def is overwritten by the base before CreateShape).
    b2FixtureDef fixture;
    fixture.density = 3.0f;
    fixture.friction = 0.3f;
    fixture.restitution = 0.1f;
    fixture.filter = _zeroFilter;
    _bladeFixture = addBox(_copterBody, fixture, "bladeShape");
    _stemFixture = addBox(_copterBody, fixture, "stemShape");
    _baseFixture = addPolygon(_copterBody, fixture, "baseVert", 4);
    _backFixture = addPolygon(_copterBody, fixture, "backVert", 5);
    fixture.filter = _defaultFilter;
    _legFixture[0] = addBox(_copterBody, fixture, "leg1Shape");
    _legFixture[1] = addBox(_copterBody, fixture, "leg2Shape");
    fixture.friction = 0.0f;
    fixture.restitution = 0.3f;
    fixture.filter = _zeroFilter;
    for (int i = 0; i < 2; i++) {
        ValueMap data = shape(i == 0 ? "wheel1Shape" : "wheel2Shape");
        _wheelFixture[i] = createFixture(_copterBody, fixture, &data, true, false);
    }
    _copterBody->ResetMassData();
    Vec2 blade = PointFromString(shape("bladeShape").at("pos").asString());
    _bladeLocalCenter = b2Vec2(blade.x, blade.y);

    // The magnet: angular damping 1, a box with the zero filter. Its range (Flash
    // magnetRangeSensor) is only used for its bounding box.
    ValueMap magnet = shape("magnetShape");
    Vec2 magnetPos = PointFromString(magnet.at("pos").asString());
    Size magnetSize = SizeFromString(magnet.at("size").asString());
    bodyDef.linearDamping = 0.0f;
    bodyDef.angularDamping = 1.0f;
    bodyDef.position.Set(_origin.x + magnetPos.x, _origin.y + magnetPos.y);
    _magnetBody = world->CreateBody(&bodyDef);
    b2PolygonShape box;
    box.SetAsBox(magnetSize.width, magnetSize.height);
    fixture.shape = &box;
    fixture.density = 3.0f;
    fixture.friction = 0.3f;
    fixture.restitution = 0.1f;
    _magnetFixture = _magnetBody->CreateFixture(&fixture);
    _magnetBody->ResetMassData();
    for (int i = 0; i < 4; i++) {
        _magnetRange[i] = guide("magnetVert" + patch::to_string(i));
    }

    for (b2Fixture* f : {_bladeFixture, _baseFixture, _backFixture, _wheelFixture[0], _wheelFixture[1],
                         _magnetFixture}) {
        addToPostSolve(f);
    }
    for (b2Fixture* f : {_baseFixture, _backFixture, _wheelFixture[0], _wheelFixture[1]}) {
        addToBeginContact(f);
    }
    _painted.push_back({_magnetBody, _magnetSprite, true});

    if (gameplay()) {
        // Flash: HeliLoop at volume 0, faded to 0.5 over a second.
        startLoop(_heliLoop, "HeliLoop", _copterBody, 0.5f, 1.0f);
    }
}

void Helicopter::createDictionaries()
{
    _contactAddSounds[_wheelFixture[0]] = "CarTire1";
    _contactAddSounds[_wheelFixture[1]] = "CarTire1";
    _contactAddSounds[_backFixture] = "BikeHit3";
    _contactAddSounds[_baseFixture] = "BikeHit1";
}

// Flash HelicopterMan.createJoints / createMovieClips: seat and both hands on the copter, the
// magnet under the centre of mass on its rope, the front of the copter over the near leg.
void Helicopter::addCharacter(CharacterB2D* character)
{
    Vehicle::addCharacter(character);
    attachRider(character);
    b2World* world = getWorld();

    // The magnet goes under the centre of mass of copter + rider; the rope hangs from the copter
    // at that x (the magnetAnchor's height) to 20 symbol px above the magnet's centre.
    float mass = 0.0f;
    std::vector<b2Body*> bodies = comBodies();
    bodies.erase(std::remove(bodies.begin(), bodies.end(), _magnetBody), bodies.end());
    b2Vec2 com(0.0f, 0.0f);
    for (b2Body* b : bodies) {
        com += b->GetMass() * b->GetWorldCenter();
        mass += b->GetMass();
    }
    if (mass > 0.0f) {
        com *= 1.0f / mass;
    }
    _magnetBody->SetTransform(b2Vec2(com.x, _magnetBody->GetWorldCenter().y), 0.0f);
    b2Vec2 copterAnchor(com.x, _origin.y + guide("magnetAnchor").y);
    b2Vec2 magnetAnchor = _magnetBody->GetWorldCenter() + b2Vec2(0.0f, 20.0f * kSym);
    _copterAnchor = _copterBody->GetLocalPoint(copterAnchor);
    _magnetAnchor = _magnetBody->GetLocalPoint(magnetAnchor);
    b2RopeJointDef rope;
    rope.bodyA = _copterBody;
    rope.bodyB = _magnetBody;
    rope.localAnchorA = _copterAnchor;
    rope.localAnchorB = _magnetAnchor;
    rope.maxLength = (magnetAnchor - copterAnchor).Length();
    rope.collideConnected = true;
    _ropeJoint = static_cast<b2RopeJoint*>(world->CreateJoint(&rope));
    _ropeMinLength = rope.maxLength;
    // RESTORED (PC addition): the rope reels between its starting length (fully in) and
    // _ropeMaxLength. A rig whose starting rope is already about that long (scaled art) left
    // ctrl clamping it shorter and shift with nothing to reel in: keep at least 2 m to let out.
    if (_ropeMaxLength < _ropeMinLength + 2.0f) {
        _ropeMaxLength = _ropeMinLength + 2.0f;
    }

    _ropePoints.clear();
    _ropeLengths.clear();
    for (int i = 0; i <= _numRopeSegments; i++) {
        b2Vec2 p = copterAnchor + (float(i) / _numRopeSegments) * (magnetAnchor - copterAnchor);
        _ropePoints.push_back({p, p, i == 0 || i == _numRopeSegments});
        if (i > 0) {
            _ropeLengths.push_back(0.0f);
        }
    }
    resizeRope();

    Node* foreground = getSession()->getCharacterForeground();
    // Between the near lower leg (z 8) and upper arm (z 9) of the pelvis-under-chest stacking.
    _frontSprite = sprite("copterFront", foreground, 8);
    _legSprite[0] = sprite("leg1", foreground, 8);
    _legSprite[1] = sprite("leg2", foreground, 8);
    for (Sprite* s : {_copterSprite, _propellerSprite, _brokenPropellerSprite, _frontSprite, _legSprite[0],
                      _legSprite[1]}) {
        if (s) {
            _painted.push_back({_copterBody, s, false});
        }
    }
}

void Helicopter::attachRider(CharacterB2D* character)
{
    _rider = character;
    b2World* world = getWorld();
    CharacterB2D* c = character;

    limitJoint(c->getNeckJoint(), c->getChestBody(), c->getHeadBody(), -10.0f, 10.0f);

    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    def.enableLimit = false;
    b2Vec2 origin(_origin.x, _origin.y);
    def.Initialize(_copterBody, c->getPelvisBody(), origin + guide("seatAnchor"));
    addBodyVehicleJoint(c->getPelvisBody(), world->CreateJoint(&def));
    def.Initialize(_copterBody, c->getLowerArm1Body(), origin + guide("handleAnchor"));
    addBodyVehicleJoint(c->getLowerArm1Body(), world->CreateJoint(&def));
    def.Initialize(_copterBody, c->getLowerArm2Body(), origin + guide("handleAnchor"));
    addBodyVehicleJoint(c->getLowerArm2Body(), world->CreateJoint(&def));
}

// ---- rider -------------------------------------------------------------------------------------

std::vector<b2Body*> Helicopter::comBodies()
{
    std::vector<b2Body*> bodies;
    if (_copterBody) {
        bodies.push_back(_copterBody);
    }
    if (riderOn()) {
        CharacterB2D* c = _rider;
        b2Body* parts[] = {c->getHeadBody(), c->getChestBody(), c->getPelvisBody(), c->getUpperArm1Body(),
                           c->getLowerArm1Body(), c->getUpperArm2Body(), c->getLowerArm2Body(),
                           c->getUpperLeg1Body(), c->getLowerLeg1Body(), c->getUpperLeg2Body(),
                           c->getLowerLeg2Body(), c->getUpperArm3Body(), c->getUpperArm4Body(),
                           c->getUpperLeg3Body(), c->getUpperLeg4Body()};
        for (int i = 0; i <= ComUpperLeg4; i++) {
            bool on = i < ComUpperArm3 ? !_comOff.count(i) : _comOn.count(i) > 0;
            if (on && parts[i]) {
                bodies.push_back(parts[i]);
            }
        }
    }
    if (_magnetBody) {
        bodies.push_back(_magnetBody);
    }
    return bodies;
}

b2Vec2 Helicopter::centerOfMass(float* totalMass)
{
    b2Vec2 com(0.0f, 0.0f);
    float mass = 0.0f;
    for (b2Body* b : comBodies()) {
        com += b->GetMass() * b->GetWorldCenter();
        mass += b->GetMass();
    }
    if (mass > 0.0f) {
        com *= 1.0f / mass;
    }
    *totalMass = mass;
    return com;
}

// Flash: chest / pelvis smashes and a broken torso throw him out; a broken arm lets go of the
// handle (out once both are gone); broken limbs stop counting in the hover mass. Death keeps him
// in his seat (Flash HelicopterMan.dead does not eject).
void Helicopter::handleInjury(CharacterInjury injury, CharacterB2D* character)
{
    if (!riderOn() || character != _rider) {
        return;
    }
    auto held = [this](b2Body* body) {
        auto it = _bodyVehicleJointDict.find(body);
        return body && it != _bodyVehicleJointDict.end() && it->second != nullptr;
    };
    auto letGo = [&](b2Body* hand) {
        destroyJointsForBody(hand);
        if (!held(character->getLowerArm1Body()) && !held(character->getLowerArm2Body())) {
            ejectCharacter(character);
        }
    };
    switch (injury) {
    case CharacterInjuryChestSmash:
    case CharacterInjuryPelvisSmash:
    case CharacterInjuryTorsoBreak:
        ejectCharacter(character);
        break;
    case CharacterInjuryHeadSmash:
        _comOff.insert(ComHead);
        break;
    case CharacterInjuryShoulder1Break:
        _comOff.insert(ComUpperArm1);
        _comOff.insert(ComLowerArm1);
        _comOn.insert(ComUpperArm3);
        letGo(character->getLowerArm1Body());
        break;
    case CharacterInjuryShoulder2Break:
        _comOff.insert(ComUpperArm2);
        _comOff.insert(ComLowerArm2);
        _comOn.insert(ComUpperArm4);
        letGo(character->getLowerArm2Body());
        break;
    case CharacterInjuryElbow1Break:
        _comOff.insert(ComLowerArm1);
        letGo(character->getLowerArm1Body());
        break;
    case CharacterInjuryElbow2Break:
        _comOff.insert(ComLowerArm2);
        letGo(character->getLowerArm2Body());
        break;
    case CharacterInjuryHip1Break:
        _comOff.insert(ComUpperLeg1);
        _comOff.insert(ComLowerLeg1);
        _comOn.insert(ComUpperLeg3);
        break;
    case CharacterInjuryHip2Break:
        _comOff.insert(ComUpperLeg2);
        _comOff.insert(ComLowerLeg2);
        _comOn.insert(ComUpperLeg4);
        break;
    case CharacterInjuryKnee1Break:
        _comOff.insert(ComLowerLeg1);
        break;
    case CharacterInjuryKnee2Break:
        _comOff.insert(ComLowerLeg2);
        break;
    default:
        break;
    }
}

// Flash eject: the copter's shapes (but the blade) get the zero filter, the man is pushed
// sideways along the copter (impulse 2), the copter levels out.
bool Helicopter::ejectCharacter(CharacterB2D* character)
{
    if (!Vehicle::ejectCharacter(character)) {
        return false;
    }
    _riderEjected = true;
    _ejected = true;
    setCurrentPose(VehiclePoseNone);
    if (_copterBody) {
        for (b2Fixture* f = _copterBody->GetFixtureList(); f; f = f->GetNext()) {
            if (f != _bladeFixture) {
                f->SetFilterData(_zeroFilter);
            }
        }
        float a = _copterBody->GetAngle();
        b2Vec2 impulse(cosf(a) * _ejectImpulse, sinf(a) * _ejectImpulse);
        for (b2Body* b : {character->getChestBody(), character->getPelvisBody()}) {
            if (b) {
                b->ApplyLinearImpulse(impulse, b->GetWorldCenter(), true);
            }
        }
    }
    _targetAng = 0.0f;
    return true;
}

// ---- controls ----------------------------------------------------------------------------------

void Helicopter::forwardButtonPressed()
{
    if (!riderOn() || _bladeSmashed) {
        return;
    }
    _hoverState = 1;
    if (!_isLoud) {
        _isLoud = true;
        if (_heliLoop) {
            _heliLoop->fadeTo(0.75f, 0.25f, false);
        }
    }
}

void Helicopter::backButtonPressed()
{
    if (!riderOn() || _bladeSmashed) {
        return;
    }
    _hoverState = -1;
    if (!_isLoud) {
        _isLoud = true;
        if (_heliLoop) {
            _heliLoop->fadeTo(0.75f, 0.25f, false);
        }
    }
}

void Helicopter::forwardBackButtonsNull()
{
    if (!riderOn() || _bladeSmashed) {
        return;
    }
    _hoverState = 0;
    if (_isLoud) {
        _isLoud = false;
        if (_heliLoop) {
            _heliLoop->fadeTo(0.5f, 0.25f, false);
        }
    }
}

// Flash left / rightPressedActions: the target angle turns ever faster (Flash sense, clockwise
// positive); without the blade the lean impulses of the bikes spin the copter instead.
void Helicopter::spin(bool right)
{
    // RESTORED (PC addition): the steps are per 60 Hz step (^2); online::perStep converts them to
    // the current step (Flash's own values at the browser physics profile's 1/30).
    const float accelStep = online::perStep(online::perStep(_accelStep));
    const float maxStep = online::perStep(_maxStep);
    if (right) {
        if (_spinAcceleration < 0.0f) {
            _spinAcceleration = -_spinAcceleration;
        }
        _spinAcceleration = std::min(_spinAcceleration + accelStep, maxStep);
    } else {
        if (_spinAcceleration > 0.0f) {
            _spinAcceleration = -_spinAcceleration;
        }
        _spinAcceleration = std::max(_spinAcceleration - accelStep, -maxStep);
    }
    _targetAng += _spinAcceleration;
    if (!_bladeSmashed || !_copterBody) {
        return;
    }
    // Flash: impulses (sin, -cos) * impulse at local centre +- offset, faded out near maxSpinAV.
    // Mobile angular velocity is the negated Flash one.
    float angle = -_copterBody->GetAngle();
    float av = -_copterBody->GetAngularVelocity();
    float factor = right ? (av - _maxSpinAV) / -_maxSpinAV : (av + _maxSpinAV) / _maxSpinAV;
    factor = std::min(std::max(factor, 0.0f), 1.0f);
    float magnitude = (right ? _impulseRight : _impulseLeft) * s_timeStepOverFlashTimeStep * factor;
    float c = cosf(angle) * magnitude;
    float s = sinf(angle) * magnitude;
    // Flash (x, y down) -> mobile (x, -y)
    b2Vec2 first = right ? b2Vec2(-s, -c) : b2Vec2(s, c);
    b2Vec2 local = _copterBody->GetLocalCenter();
    _copterBody->ApplyLinearImpulse(first, _copterBody->GetWorldPoint(b2Vec2(local.x + _impulseOffset, local.y)), true);
    _copterBody->ApplyLinearImpulse(-first, _copterBody->GetWorldPoint(b2Vec2(local.x - _impulseOffset, local.y)), true);
}

void Helicopter::leanForwardButtonPressed()
{
    if (!riderOn()) {
        return;
    }
    spin(true);
    setCurrentPose(VehiclePoseLeanForward);
}

void Helicopter::leanBackButtonPressed()
{
    if (!riderOn()) {
        return;
    }
    spin(false);
    setCurrentPose(VehiclePoseLeanBack);
}

// Flash leftAndRightActions: the target angle eases back to level.
void Helicopter::leanButtonsNull()
{
    if (!riderOn()) {
        return;
    }
    // RESTORED (PC addition): per current step, as in spin().
    const float decelStep = online::perStep(online::perStep(_decelStep));
    const float maxStep = online::perStep(_maxStep);
    if (_targetAng > 0.0f) {
        _spinAcceleration = std::max(_spinAcceleration - decelStep, -maxStep);
        _targetAng = std::max(_targetAng + _spinAcceleration, 0.0f);
    } else if (_targetAng < 0.0f) {
        _spinAcceleration = std::min(_spinAcceleration + decelStep, maxStep);
        _targetAng = std::min(_targetAng + _spinAcceleration, 0.0f);
    }
    if (_currentPose == VehiclePoseLeanForward || _currentPose == VehiclePoseLeanBack) {
        setCurrentPose(VehiclePoseNone);
    }
}

// Flash spacePressedActions: toggles the magnet once per press.
void Helicopter::special1ButtonPressed()
{
    if (!riderOn() || !_spaceOff) {
        return;
    }
    _spaceOff = false;
    _magnetized = !_magnetized;
    if (_magnetized) {
        startLoop(_magnetLoop, "MagnetBuzz", _magnetBody, 1.0f, 0.25f);
    } else {
        stopLoop(_magnetLoop, 0.25f);
        if (_magnetSprite) {
            _magnetSprite->setSpriteFrame(_name + "_magnet.png");
        }
    }
}

void Helicopter::special1ButtonNull()
{
    _spaceOff = true;
}

// Flash shift / ctrlPressedActions: reel the rope in / out.
void Helicopter::extraControls(unsigned char state)
{
    if (!riderOn() || !_ropeJoint) {
        return;
    }
    float length = _ropeJoint->GetMaxLength();
    float next = length;
    const float ropeSpeed = online::perStep(_ropeSpeed);  // RESTORED (PC addition): per current step
    if (state & 0x20) {
        next = std::max(length - ropeSpeed, _ropeMinLength);
    } else if (state & 0x40) {
        next = std::min(length + ropeSpeed, _ropeMaxLength);
    }
    if (next != length) {
        _ropeJoint->SetMaxLength(next);
        _magnetBody->SetAwake(true);
        resizeRope();
    }
}

// Flash leanForwardPose / leanBackPose. setJoint(j, a, g) -> mobile setJoint(j, -a, g, 20).
void Helicopter::leanForwardPose()
{
    if (!riderOn()) {
        return;
    }
    CharacterB2D* c = _rider;
    if (c->getNeckJoint()) {
        c->setJoint(c->getNeckJoint(), -0.4f, 10.0f, 20.0f);
    }
    for (b2RevoluteJoint* j : {c->getElbowJoint1(), c->getElbowJoint2()}) {
        if (j) {
            c->setJoint(j, -0.5f, 5.0f, 20.0f);
        }
    }
}

void Helicopter::leanBackPose()
{
    if (!riderOn()) {
        return;
    }
    CharacterB2D* c = _rider;
    if (c->getNeckJoint()) {
        c->setJoint(c->getNeckJoint(), 0.0f, 10.0f, 20.0f);
    }
    for (b2RevoluteJoint* j : {c->getElbowJoint1(), c->getElbowJoint2()}) {
        if (j) {
            c->setJoint(j, -2.0f, 5.0f, 20.0f);
        }
    }
}

// ---- flight ------------------------------------------------------------------------------------

// Flash balanceCopter: the angular velocity is set to pull the copter to the target angle.
void Helicopter::balanceCopter()
{
    _targetAng = wrapAngle(_targetAng);
    float angle = wrapAngle(_copterBody->GetAngle());
    float target = -_targetAng;
    _copterBody->SetAngularVelocity(-sinf(angle - target) * 3.0f);
}

// Flash hoverCopter: cancels gravity for the whole mass at its centre (mass / 3 per Flash frame
// = g / 30 s), climbs with up to 0.31 mass more and sinks with 0.5 + 0.31 mass, both easing off
// towards 10 m/s along the copter's up axis.
void Helicopter::hoverCopter()
{
    float totalMass = 0.0f;
    b2Vec2 com = centerOfMass(&totalMass);
    float angle = _copterBody->GetAngle();
    b2Vec2 up(-sinf(angle), cosf(angle));
    float speed = b2Dot(_copterBody->GetLinearVelocityFromWorldPoint(com), up);
    const float maxSpeed = 10.0f;
    float hold = totalMass / 3.0f;
    float extra = totalMass * 0.31f;
    speed = std::min(std::max(speed, -maxSpeed), maxSpeed);
    float k = s_timeStepOverFlashTimeStep;
    float magnitude;
    if (_hoverState >= 0) {
        _copterBody->ApplyLinearImpulse(hold * k * up, com, true);
        if (_hoverState == 0) {
            return;
        }
        magnitude = extra - extra * speed / maxSpeed;
    } else {
        magnitude = -0.5f - (extra + extra * speed / maxSpeed);
    }
    _copterBody->ApplyLinearImpulse(magnitude * k * up, com, true);
}

// ---- rope (Flash VPoint / VSpring) -------------------------------------------------------------

void Helicopter::resizeRope()
{
    if (!_ropeJoint) {
        return;
    }
    float length = 0.8f * _ropeJoint->GetMaxLength() / _numRopeSegments;
    for (float& l : _ropeLengths) {
        l = length;
    }
}

// Flash paint: the ends follow the anchors, the points fall (10 / 30 / 30 m per Flash frame^2,
// a quarter per 60 Hz step^2), 20 relaxation passes.
void Helicopter::stepRope()
{
    if (_ropePoints.empty()) {
        return;
    }
    if (!_copterSmashed && _copterBody) {
        b2Vec2 p = _copterBody->GetWorldPoint(_copterAnchor);
        _ropePoints.front().curr = _ropePoints.front().prev = p;
    }
    b2Vec2 m = _magnetBody->GetWorldPoint(_magnetAnchor);
    _ropePoints.back().curr = _ropePoints.back().prev = m;
    // RESTORED (PC addition): per current step^2 (a quarter of Flash's only at 1/60).
    const float gravity = online::perStep(online::perStep(-(10.0f / 30.0f) / 30.0f / 4.0f));
    for (RopePoint& p : _ropePoints) {
        if (!p.fixed) {
            b2Vec2 v = p.curr - p.prev;
            p.prev = p.curr;
            p.curr += v;
            p.curr.y += gravity;
        }
    }
    for (int pass = 0; pass < 20; pass++) {
        for (int i = 0; i < _numRopeSegments; i++) {
            RopePoint& a = _ropePoints[i];
            RopePoint& b = _ropePoints[i + 1];
            b2Vec2 d = b.curr - a.curr;
            float dist = d.Length();
            if (dist < 1e-6f) {
                continue;
            }
            float wa = a.fixed ? 0.0f : 1.0f;
            float wb = b.fixed ? 0.0f : 1.0f;
            if (wa + wb == 0.0f) {
                continue;
            }
            b2Vec2 corr = ((dist - _ropeLengths[i]) / dist) * d;
            a.curr += (wa / (wa + wb)) * corr;
            b.curr -= (wb / (wa + wb)) * corr;
        }
    }
}

// ---- contacts ----------------------------------------------------------------------------------

void Helicopter::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    for (Shard& shard : _shards) {
        if (fixture != shard.sensor || !shard.sensor) {
            continue;
        }
        // Flash BladeShard.checkAdd: flesh touching the stabbing shard gets pinned next step.
        if (getLevel()->getFixtureMaterial(otherFixture) & 2) {
            b2Body* body = otherFixture->GetBody();
            b2Vec2 normal = body->GetWorldCenter() - shard.body->GetWorldCenter();
            if (normal.Normalize() < 1e-6f) {
                normal.Set(0.0f, 1.0f);
            }
            shard.toAdd.push_back({body, normal});
            watch(otherFixture);
        }
        return;
    }
    Vehicle::beginContact(fixture, otherFixture, contact);
}

void Helicopter::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    for (Shard& shard : _shards) {
        if (fixture != shard.sensor || !shard.sensor) {
            continue;
        }
        // Flash BladeShard.checkRemove
        if (getLevel()->getFixtureMaterial(otherFixture) & 2) {
            b2Body* body = otherFixture->GetBody();
            auto pending = std::find_if(shard.toAdd.begin(), shard.toAdd.end(),
                                        [body](const std::pair<b2Body*, b2Vec2>& e) { return e.first == body; });
            if (shard.joints.count(body) || pending != shard.toAdd.end()) {
                shard.toRemove.push_back(body);
            }
        }
        return;
    }
    Vehicle::endContact(fixture, otherFixture, contact);
}

void Helicopter::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                           const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }
    if (fixture == _bladeFixture && _bladeFixture) {
        // Flash bladeContactResultHandler: every hit is pushed away; above 30 the blade breaks.
        b2Vec2 p = contactPoint(contact);
        _bladeHits.push_back({otherFixture, p, normalImpulse});
        watch(otherFixture);
        if (normalImpulse > _bladeSmashLimit && normalImpulse > _bladeResult) {
            _bladeResult = normalImpulse;
            _bladeResultPoint = p;
        }
        return;
    }
    if (fixture == _backFixture || fixture == _baseFixture) {
        if (normalImpulse > _copterSmashLimit && fixture) {
            if (!_copterResult) {
                _bladeResultPoint = contactPoint(contact);
            }
            _copterResult = true;
        }
        return;
    }
    for (int i = 0; i < 2; i++) {
        if (fixture == _wheelFixture[i] && fixture) {
            if (normalImpulse > _copterSmashLimit) {
                _legResult[i] = true;
            }
            return;
        }
    }
    if (fixture == _magnetFixture) {
        // Flash magnetContactResult: the strongest contact per touching shape.
        auto it = _magnetHits.find(otherFixture);
        if (it == _magnetHits.end() || normalImpulse > it->second.impulse) {
            _magnetHits[otherFixture] = {otherFixture, contactPoint(contact), normalImpulse};
            watch(otherFixture);
        }
        return;
    }
    for (Shard& shard : _shards) {
        if (fixture != shard.fixture) {
            continue;
        }
        // Flash BladeShard.checkContact: a hard hit on flesh makes the shard stab.
        if (normalImpulse > shard.stabImpulse && !shard.sensor &&
            (getLevel()->getFixtureMaterial(otherFixture) & 2)) {
            shard.stab = true;
            shard.active = true;
        }
        return;
    }
}

void Helicopter::fixtureWillBeDestroyed(b2Fixture* fixture)
{
    Vehicle::fixtureWillBeDestroyed(fixture);
    _watched.erase(fixture);
    _bladeHits.erase(std::remove_if(_bladeHits.begin(), _bladeHits.end(),
                                    [fixture](const Hit& h) { return h.other == fixture; }),
                     _bladeHits.end());
    _magnetHits.erase(fixture);
    b2Body* body = fixture->GetBody();
    for (Shard& shard : _shards) {
        shard.toAdd.erase(std::remove_if(shard.toAdd.begin(), shard.toAdd.end(),
                                         [body](const std::pair<b2Body*, b2Vec2>& e) { return e.first == body; }),
                          shard.toAdd.end());
    }
}

void Helicopter::jointWillBeDestroyed(b2Joint* joint)
{
    for (Shard& shard : _shards) {
        for (auto it = shard.joints.begin(); it != shard.joints.end(); ++it) {
            if (it->second == joint) {
                shard.counts.erase(it->first);
                shard.joints.erase(it);
                return;
            }
        }
    }
}

// Flash handleBladeContacts: everything the turning blade touches is shoved away from its hub
// (impulse 3 split by inverse mass, the copter takes the rest) with flesh or ricochet sounds.
void Helicopter::handleBladeContacts()
{
    std::vector<Hit> hits;
    hits.swap(_bladeHits);
    if (_bladeSmashed || !_copterBody) {
        return;
    }
    const float total = 3.0f * s_timeStepOverFlashTimeStep;
    restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession());
    for (const Hit& hit : hits) {
        b2Body* other = hit.other->GetBody();
        b2Vec2 local = _copterBody->GetLocalPoint(hit.point) - _bladeLocalCenter;
        local.Normalize();
        b2Vec2 dir = b2Mul(_copterBody->GetTransform().q, local);
        if (other->GetMass() > 0.0f) {
            float otherInv = 1.0f / other->GetMass();
            float sum = otherInv + 1.0f / _copterBody->GetMass();
            float toOther = total * otherInv / sum;
            float toCopter = total * (1.0f / _copterBody->GetMass()) / sum;
            b2Vec2 push = toOther * dir;
            other->ApplyLinearImpulse(push, hit.point, true);
            _copterBody->ApplyLinearImpulse(-toCopter * dir, hit.point, true);
            int material = getLevel()->getFixtureMaterial(hit.other);
            // Flash picks flesh or metal by the last digit of the contact's x (70 % flesh).
            bool flesh = (material & 7) && CCRANDOM_0_1() < 0.7f;
            if (flesh) {
                if (gameplay()) {
                    createBodySound("BladeFlesh" + patch::to_string((int)ceilf(CCRANDOM_0_1() * 3.0f)), other,
                                    1.0f, false);
                }
            } else if (!_bladeImpactSound) {
                _bladeImpactSound = true;
                if (gameplay()) {
                    createBodySound("MetalRicochet" + patch::to_string((int)ceilf(CCRANDOM_0_1() * 3.0f)), other,
                                    1.0f, false);
                }
                if (particles) {
                    b2Vec2 v = (5.0f / s_timeStepOverFlashTimeStep) * push;
                    particles->sparks(hit.point, b2Vec2(v.x, -v.y), 5.0f, 50.0f, 20);
                }
            }
        } else {
            b2Vec2 push = total * dir;
            _copterBody->ApplyLinearImpulse(-push, hit.point, true);
            if (!_bladeImpactSound) {
                _bladeImpactSound = true;
                if (gameplay()) {
                    createBodySound("MetalRicochet" + patch::to_string((int)ceilf(CCRANDOM_0_1() * 3.0f)),
                                    _copterBody, 0.4f, false);
                }
                if (particles) {
                    b2Vec2 v = (5.0f / s_timeStepOverFlashTimeStep) * push;
                    particles->sparks(hit.point, b2Vec2(v.x, -v.y), 5.0f, 50.0f, 20);
                }
            }
        }
    }
}

// Flash handleMagnetContacts: while on, whatever touches the magnet's underside inside its range
// is held by a stiff revolute joint; off, every held thing is let go.
void Helicopter::handleMagnetContacts()
{
    std::map<b2Fixture*, Hit> hits;
    hits.swap(_magnetHits);
    if (!_magnetBody) {
        return;
    }
    b2World* world = getWorld();
    if (!_magnetized) {
        std::vector<b2Joint*> held;
        for (b2JointEdge* e = _magnetBody->GetJointList(); e; e = e->next) {
            if (e->joint != _ropeJoint) {
                held.push_back(e->joint);
            }
        }
        for (b2Joint* j : held) {
            world->DestroyJoint(j);
        }
        return;
    }
    if (hits.empty()) {
        return;
    }
    std::set<b2Body*> holding;
    for (b2JointEdge* e = _magnetBody->GetJointList(); e; e = e->next) {
        if (e->joint->GetType() == e_revoluteJoint) {
            holding.insert(e->other);
        }
    }
    b2AABB range;
    range.lowerBound = range.upperBound = _magnetBody->GetWorldPoint(_magnetRange[0]);
    for (int i = 1; i < 4; i++) {
        b2Vec2 p = _magnetBody->GetWorldPoint(_magnetRange[i]);
        range.lowerBound = b2Min(range.lowerBound, p);
        range.upperBound = b2Max(range.upperBound, p);
    }
    std::vector<b2Body*> com = comBodies();
    for (auto& entry : hits) {
        const Hit& hit = entry.second;
        b2Body* body = hit.other->GetBody();
        if (!body || hit.other->IsSensor()) {
            continue;
        }
        b2AABB box;
        hit.other->GetShape()->ComputeAABB(&box, body->GetTransform(), 0);
        if (box.lowerBound.x > range.upperBound.x || box.upperBound.x < range.lowerBound.x ||
            box.lowerBound.y > range.upperBound.y || box.upperBound.y < range.lowerBound.y) {
            continue;
        }
        b2Vec2 local = _magnetBody->GetLocalPoint(hit.point);
        bool below = local.y < 0.0f;  // Flash: local y > 0 (y down)
        if (!below || body->GetMass() <= 0.0f || holding.count(body) ||
            std::find(com.begin(), com.end(), body) != com.end()) {
            continue;
        }
        b2RevoluteJointDef def;
        def.Initialize(_magnetBody, body, hit.point);
        def.collideConnected = true;
        def.enableLimit = true;
        def.lowerAngle = -3.0f * kDeg;
        def.upperAngle = 3.0f * kDeg;
        world->CreateJoint(&def);
        holding.insert(body);
    }
}

// ---- blade shards (Flash heli.BladeShard) ------------------------------------------------------

void Helicopter::shardActions(Shard& shard)
{
    if (shard.stab) {
        // The shard stops hitting things (mask 8) and grows a sensor that pins flesh.
        shard.stab = false;
        b2Filter filter = shard.fixture->GetFilterData();
        filter.maskBits = 8;
        shard.fixture->SetFilterData(filter);
        b2FixtureDef def;
        def.shape = shard.fixture->GetShape();
        def.isSensor = true;
        def.density = 0.0f;
        def.filter.categoryBits = 8;
        def.filter.maskBits = 4;
        def.filter.groupIndex = 0;
        shard.sensor = shard.body->CreateFixture(&def);
        addToBeginContact(shard.sensor);
        addToEndContact(shard.sensor);
        return;
    }
    std::vector<std::pair<b2Body*, b2Vec2>> adds;
    adds.swap(shard.toAdd);
    for (auto& add : adds) {
        createPrisJoint(shard, add.first, add.second);
    }
    std::vector<b2Body*> removes;
    removes.swap(shard.toRemove);
    if (!removes.empty()) {
        for (b2Body* body : removes) {
            removeShardJoint(shard, body);
        }
        checkZeroJoints(shard);
    }
}

void Helicopter::createPrisJoint(Shard& shard, b2Body* body, b2Vec2 normal)
{
    auto existing = shard.joints.find(body);
    if (existing != shard.joints.end()) {
        shard.counts[body]++;
        return;
    }
    b2Vec2 anchor = body->GetWorldCenter();
    b2PrismaticJointDef def;
    def.Initialize(shard.body, body, anchor, normal);
    def.collideConnected = true;
    def.enableMotor = true;
    def.maxMotorForce = 30.0f;
    def.motorSpeed = 0.0f;
    b2PrismaticJoint* joint = static_cast<b2PrismaticJoint*>(getWorld()->CreateJoint(&def));
    shard.joints[body] = joint;
    shard.counts[body] = 1;
    getSession()->getDestructionListener()->addJointListener(joint, this);

    b2Fixture* fixture = body->GetFixtureList();
    LevelItem* item = fixture ? static_cast<LevelItem*>(fixture->GetUserData()) : nullptr;
    if (item) {
        item->shapeImpale(fixture, true, b2Vec2(INFINITY, INFINITY), 0.0f);
    }
    if (body == shard.previousBody) {
        return;
    }
    shard.previousBody = body;
    if (EmitterNode* particles = getSession()->getParticlesForeground()) {
        float ptm = getPtm();
        if (BurstEmitter* blood = BurstEmitter::createBloodBurst(5.0f, 15.0f, Vec2(anchor.x * ptm, anchor.y * ptm), 50)) {
            particles->addChild(blood);
        }
    }
    if (!shard.fleshSound && gameplay()) {
        shard.fleshSound = createBodySound("ImpaleSpikes" + patch::to_string((int)ceilf(CCRANDOM_0_1() * 3.0f)),
                                           shard.body, 1.0f, false);
        if (shard.fleshSound) {
            Sound** slot = &shard.fleshSound;
            shard.fleshSound->setFinishCallback([slot](int&) { *slot = nullptr; });
        }
    }
}

void Helicopter::removeShardJoint(Shard& shard, b2Body* body)
{
    auto it = shard.joints.find(body);
    if (it == shard.joints.end()) {
        return;
    }
    if (--shard.counts[body] > 0) {
        return;
    }
    getSession()->getDestructionListener()->removeJointListener(this, it->second);
    getWorld()->DestroyJoint(it->second);
    shard.joints.erase(it);
    shard.counts.erase(body);
}

// Flash checkZeroJoints: nothing pinned any more - a plain shard again (category 8).
void Helicopter::checkZeroJoints(Shard& shard)
{
    if (!shard.joints.empty()) {
        return;
    }
    b2Filter filter;
    filter.categoryBits = 8;
    filter.maskBits = 0xffff;
    filter.groupIndex = 0;
    shard.fixture->SetFilterData(filter);
    if (shard.sensor) {
        b2Fixture* sensor = shard.sensor;
        removeBeginContact(sensor);
        removeEndContact(sensor);
        shard.body->DestroyFixture(sensor);
        shard.sensor = nullptr;
    }
    shard.active = false;
}

// Flash BladeShard.createSprites: the shard in the blade's colour (dark for one half, light for
// the other) with a lighter edge strip 1 px wide along its outer side.
void Helicopter::drawShard(Shard& shard)
{
    float ptm = getPtm();
    b2PolygonShape* polygon = static_cast<b2PolygonShape*>(shard.fixture->GetShape());
    std::vector<b2Vec2> poly(polygon->m_vertices, polygon->m_vertices + polygon->m_count);
    auto draw = [&](const std::vector<b2Vec2>& p, const Color4F& color) {
        if (p.size() < 3) {
            return;
        }
        std::vector<Vec2> pts;
        for (const b2Vec2& v : p) {
            pts.push_back(Vec2(v.x * ptm, v.y * ptm));
        }
        shard.node->drawSolidPoly(pts.data(), (unsigned int)pts.size(), color);
    };
    draw(poly, Color4F(Color3B(shard.rightSide ? 0x55 : 0xcc, shard.rightSide ? 0x55 : 0xcc,
                               shard.rightSide ? 0x5b : 0xcc)));
    // Flash: the part beyond 1.5 px from the blade's centre line on the outer side (mobile y up).
    float edge = 1.5f * kPx;
    draw(clipY(poly, shard.rightSide ? edge : -edge, shard.rightSide), Color4F(Color3B(0x8f, 0x90, 0x98)));
}

// ---- smashes -----------------------------------------------------------------------------------

b2Body* Helicopter::brokenPiece(int index)
{
    b2World* world = getWorld();
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = _copterBody->GetPosition();
    def.angle = _copterBody->GetAngle();
    b2FixtureDef fixture;
    fixture.density = 3.0f;
    fixture.friction = 0.3f;
    fixture.restitution = 0.3f;
    fixture.filter = _zeroFilter;
    b2Body* body = world->CreateBody(&def);
    addPolygon(body, fixture, "broken" + patch::to_string(index + 1) + "Vert", 4);
    body->ResetMassData();
    body->SetAngularVelocity(_copterBody->GetAngularVelocity());
    body->SetLinearVelocity(_copterBody->GetLinearVelocityFromLocalPoint(body->GetLocalCenter()));
    if (Sprite* s = sprite("broken" + patch::to_string(index + 1), getSession()->getVehicleForeground(), 0)) {
        _painted.push_back({body, s, true});
    }
    return body;
}

// Flash bladeSmash: the blade (but its hub) breaks at the hit into a quad and a triangle per
// side, flung outwards (50 x their offset from the hub) - the copter falls from now on.
void Helicopter::bladeSmash(b2Vec2 worldPoint)
{
    _bladeSmashed = true;
    b2World* world = getWorld();
    // Flash blade-local coordinates (y down) of the hit.
    b2Vec2 hit = _copterBody->GetLocalPoint(worldPoint) - _bladeLocalCenter;
    hit.y = -hit.y;
    const float trim = 20.0f * kSym;
    const float top = -5.0f * kSym;
    const float bottom = 5.0f * kSym;
    float shift = 235.0f * kSym;
    bool rightSide;
    float x0, x1;
    if (hit.x < 0.0f) {
        rightSide = false;
        x0 = -200.0f * kSym;
        x1 = -35.0f * kSym;
    } else {
        rightSide = true;
        x0 = 35.0f * kSym;
        x1 = 200.0f * kSym;
        shift = -shift;
    }
    b2Vec2 v7(x0, top), v8(x1, top), v9(x1, bottom), v10(x0, bottom), v11(0.0f, 0.0f);
    int quad[4];
    int tri[3];
    // vertex indices: 7, 8, 9, 10, 11 -> 0..4
    if (hit.x < x0 + trim) {
        v11.x = x1 - trim;
        int t[3] = {4, 3, 0};
        std::copy(t, t + 3, tri);
        if (hit.y > 0.0f) {
            v11.y = top;
            int q[4] = {4, 1, 2, 3};
            std::copy(q, q + 4, quad);
        } else {
            v11.y = bottom;
            int q[4] = {4, 0, 1, 2};
            std::copy(q, q + 4, quad);
        }
    } else if (hit.x > x1 - trim) {
        v11.x = x0 + trim;
        int t[3] = {4, 1, 2};
        std::copy(t, t + 3, tri);
        if (hit.y > 0.0f) {
            v11.y = top;
            int q[4] = {4, 2, 3, 0};
            std::copy(q, q + 4, quad);
        } else {
            v11.y = bottom;
            int q[4] = {4, 3, 0, 1};
            std::copy(q, q + 4, quad);
        }
    } else {
        v11.x = hit.x;
        int t[3] = {4, 1, 2};
        std::copy(t, t + 3, tri);
        if (hit.y > 0.0f) {
            v11.y = bottom;
            int q[4] = {4, 3, 0, 1};
            std::copy(q, q + 4, quad);
        } else {
            v11.y = top;
            int q[4] = {4, 2, 3, 0};
            std::copy(q, q + 4, quad);
        }
    }

    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = _copterBody->GetWorldPoint(_bladeLocalCenter);
    def.angle = _copterBody->GetAngle();
    b2FixtureDef fixture;
    fixture.density = 3.0f;
    fixture.friction = 0.3f;
    fixture.restitution = 0.1f;
    fixture.filter = _zeroFilter;
    Node* background = getSession()->getVehicleBackground();
    _shards.reserve(_shards.size() + 4);
    for (int half = 0; half < 2; half++) {
        if (half == 1) {
            for (b2Vec2* v : {&v7, &v8, &v9, &v10, &v11}) {
                v->x += shift;
            }
            rightSide = !rightSide;
        }
        b2Vec2 verts[5] = {v7, v8, v9, v10, v11};
        b2Vec2 quadCentre;
        for (int piece = 0; piece < 2; piece++) {
            int count = piece == 0 ? 4 : 3;
            b2Vec2 pts[4];
            for (int i = 0; i < count; i++) {
                b2Vec2 v = verts[piece == 0 ? quad[i] : tri[i]];
                pts[i] = b2Vec2(v.x, -v.y);  // back to mobile (y up)
            }
            b2PolygonShape polygon;
            polygon.Set(pts, count);
            fixture.shape = &polygon;
            Shard shard;
            shard.body = world->CreateBody(&def);
            shard.fixture = shard.body->CreateFixture(&fixture);
            shard.body->ResetMassData();
            shard.rightSide = rightSide;
            shard.stabImpulse = shard.body->GetMass() * 3.0f;
            b2Vec2 centre = shard.body->GetWorldCenter();
            if (piece == 0) {
                quadCentre = centre;
            }
            // Flash flings both pieces by the quad's offset from the hub (sic for the triangle).
            b2Vec2 velocity = _copterBody->GetLinearVelocityFromWorldPoint(centre) +
                              50.0f * (quadCentre - shard.body->GetPosition());
            shard.body->SetLinearVelocity(velocity);
            shard.node = DrawNode::create();
            background->addChild(shard.node, -1);
            _shards.push_back(shard);
            addToPostSolve(_shards.back().fixture);
            drawShard(_shards.back());
        }
    }

    b2Vec2 hub = _copterBody->GetWorldPoint(_bladeLocalCenter);
    std::vector<std::string> frames;
    for (int i = 1; i <= 16; i++) {
        frames.push_back(_name + "_shard_" + patch::to_string(i) + ".png");
    }
    if (restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession())) {
        particles->burst(frames, hub, b2Vec2(0.0f, 0.0f), 30.0f, 60.0f, 70);
    }
    removePostSolve(_bladeFixture);
    Vehicle::fixtureWillBeDestroyed(_bladeFixture);
    _copterBody->DestroyFixture(_bladeFixture);
    _bladeFixture = nullptr;
    _bladeHits.clear();
    if (_propellerSprite) {
        _propellerSprite->setVisible(false);
    }
    if (_brokenPropellerSprite) {
        _brokenPropellerSprite->setVisible(true);
    }
    stopLoop(_heliLoop, 0.05f);
    if (gameplay()) {
        createBodySound("MetalSmashHeavy4", _copterBody, 1.0f, false);
    }
    if (_rider) {
        _rider->addVocalsWithName("Help", static_cast<VocalPriority>(4));
    }
}

// Flash copterLegSmash: the leg (with its wheel) breaks off as piece 6 / 7.
void Helicopter::legSmash(int leg, bool sound)
{
    if (!_legOn[leg] || !_copterBody) {
        return;
    }
    _legOn[leg] = false;
    if (_legSprite[leg]) {
        _legSprite[leg]->setVisible(false);
    }
    brokenPiece(5 + leg);
    b2Fixture** slots[2] = {&_legFixture[leg], &_wheelFixture[leg]};
    for (b2Fixture** slot : slots) {
        b2Fixture* f = *slot;
        if (f) {
            removePostSolve(f);
            removeBeginContact(f);
            Vehicle::fixtureWillBeDestroyed(f);
            _copterBody->DestroyFixture(f);
            *slot = nullptr;
        }
    }
    if (sound && gameplay()) {
        createBodySound("StemSnap", _copterBody, 1.0f, false);
    }
}

// Flash copterSmash: blade and legs go, the man is thrown out and the copter breaks into five
// pieces with a burst of copter shards; the rope now hangs from the magnet alone.
void Helicopter::copterSmash()
{
    _copterSmashed = true;
    if (!_bladeSmashed) {
        bladeSmash(_bladeResultPoint);
    }
    legSmash(0, false);
    legSmash(1, false);
    if (_rider && !_riderEjected) {
        ejectCharacter(_rider);
    }
    _vehicleSmashed = true;
    b2Vec2 centre = _copterBody->GetWorldCenter();
    for (int i = 0; i < 5; i++) {
        brokenPiece(i);
    }
    std::vector<std::string> frames;
    for (int i = 1; i <= 16; i++) {
        frames.push_back(_name + "_shard_" + patch::to_string(i) + ".png");
    }
    if (restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession())) {
        particles->burst(frames, centre, b2Vec2(0.0f, 0.0f), 30.0f, 30.0f, 70);
    }
    if (gameplay()) {
        // (on the magnet: the copter body goes now)
        createPositionSound("MetalSmashHeavy2", Vec2(centre.x, centre.y), 1.0f, false);
    }
    for (b2Fixture* f = _copterBody->GetFixtureList(); f; f = f->GetNext()) {
        removePostSolve(f);
        removeBeginContact(f);
        Vehicle::fixtureWillBeDestroyed(f);
    }
    _painted.erase(std::remove_if(_painted.begin(), _painted.end(),
                                  [this](const Painted& p) { return p.body == _copterBody; }),
                   _painted.end());
    for (Sprite* s : {_copterSprite, _propellerSprite, _brokenPropellerSprite, _frontSprite, _legSprite[0],
                      _legSprite[1]}) {
        if (s) {
            s->setVisible(false);
        }
    }
    stopLoop(_heliLoop, 0.05f);
    stopSoundsForBody(_copterBody);
    getWorld()->DestroyBody(_copterBody);  // takes the rope joint
    _copterBody = nullptr;
    _ropeJoint = nullptr;
    _baseFixture = _backFixture = _stemFixture = nullptr;
    if (!_ropePoints.empty()) {
        _ropePoints.front().fixed = false;
    }
}

void Helicopter::debugFunction(int value)
{
    if (value == 1) {
        if (!_bladeSmashed && _copterBody) {
            bladeSmash(_copterBody->GetWorldPoint(_bladeLocalCenter + b2Vec2(1.0f, 0.0f)));
        }
        return;
    }
    if (!_copterSmashed && _copterBody) {
        _bladeResultPoint = _copterBody->GetWorldCenter();
        copterSmash();
    }
}

// ---- per step ----------------------------------------------------------------------------------

void Helicopter::actions()
{
    _frameCounter++;
    if (!_bladeSmashed && _copterBody) {
        balanceCopter();
        hoverCopter();
    }
    if (_bladeImpactSound) {
        // RESTORED (PC addition): _soundDelay counts 60 Hz steps; converted to the current step.
        if (++_soundDelayCount >= online::stepsFor60HzFrames(_soundDelay)) {
            _bladeImpactSound = false;
            _soundDelayCount = 0;
            _soundDelay = ((int)roundf(CCRANDOM_0_1() * 20.0f) + 5) * 2;  // Flash frames -> steps
        }
    }

    checkPose();
    handleContactAdds();

    // Flash handleContactResults (after the rider's own).
    handleBladeContacts();
    if (_copterResult && !_copterSmashed && _copterBody) {
        copterSmash();
    }
    if (_bladeResult > 0.0f && !_bladeSmashed && _copterBody) {
        bladeSmash(_bladeResultPoint);
    }
    for (int i = 0; i < 2; i++) {
        if (_legResult[i] && _copterBody) {
            legSmash(i, true);
        }
        _legResult[i] = false;
    }
    _copterResult = false;
    _bladeResult = 0.0f;
    handleMagnetContacts();
    for (Shard& shard : _shards) {
        if (shard.active) {
            shardActions(shard);
        }
    }
    unwatchAll();

    stepRope();

    // The propeller turns (9 frames at 30 fps), the magnet's lights blink while it is on.
    // RESTORED (PC addition): one art frame per Flash frame (2 steps at 1/60, 1 at 1/30).
    const int spf = online::stepsPerFlashFrame();
    if (gameplay() && _frameCounter % spf == 0) {
        int frame = (_frameCounter / spf) % 9 + 1;
        if (_propellerSprite && _propellerSprite->isVisible()) {
            _propellerSprite->setSpriteFrame(_name + "_propeller_" + patch::to_string(frame) + ".png");
        }
        if (_magnetized && _magnetSprite) {
            int m = (_frameCounter / spf) % 6 + 1;
            _magnetSprite->setSpriteFrame(_name + "_magnetOn_" + patch::to_string(m) + ".png");
        }
    }
}

void Helicopter::paint()
{
    float ptm = getPtm();
    for (const Painted& p : _painted) {
        b2Vec2 c = p.atCentre ? p.body->GetWorldCenter() : p.body->GetPosition();
        p.sprite->setPosition(Vec2(c.x * ptm, c.y * ptm));
        p.sprite->setRotation(-CC_RADIANS_TO_DEGREES(p.body->GetAngle()));
    }
    for (Shard& shard : _shards) {
        b2Vec2 c = shard.body->GetPosition();
        shard.node->setPosition(Vec2(c.x * ptm, c.y * ptm));
        shard.node->setRotation(-CC_RADIANS_TO_DEGREES(shard.body->GetAngle()));
    }
    if (_ropeNode) {
        _ropeNode->clear();
        // Flash lineStyle(1, 0x333333): one Flash px wide.
        float radius = 0.5f * kPx * ptm;
        Color4F color(Color3B(0x33, 0x33, 0x33));
        for (int i = 0; i + 1 < (int)_ropePoints.size(); i++) {
            b2Vec2 a = _ropePoints[i].curr;
            b2Vec2 b = _ropePoints[i + 1].curr;
            _ropeNode->drawSegment(Vec2(a.x * ptm, a.y * ptm), Vec2(b.x * ptm, b.y * ptm), radius, color);
        }
    }
}

// ---- QOL (PC addition): re-grab vehicle (src/game/vehicles/Vehicle.h) ---------------------------

b2Body* Helicopter::qolFrameBody()
{
    return _copterBody;
}

// handleInjury: off once both hands have let go (chest / pelvis smashes and a broken torso are
// never re-mounted, Vehicle::qolRiderFit). Without the blade he can still hang on to the wreck.
bool Helicopter::qolCanRemount(CharacterB2D* character)
{
    return character == _rider && !_copterSmashed && _copterBody &&
           !(character->qolLostLowerArm(1) && character->qolLostLowerArm(2));
}

// ejectCharacter: _riderEjected / _ejected, the copter's shapes zero-filtered (qolRestoreFilters),
// the target angle levelled (kept). The magnet, its rope and the sprites stay as they are.
void Helicopter::qolRemount(CharacterB2D* character)
{
    _riderEjected = false;
    _ejected = false;
    qolRestoreFilters(character);
    qolMount(character, [this, character]() {
        Vehicle::addCharacter(character);
        attachRider(character);
    });
    qolReplayInjuries(character);
}
