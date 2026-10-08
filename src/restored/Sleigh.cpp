// RESTORED (PC addition): Santa Claus's sleigh and elves, ported from the browser game's
// SantaClaus.as / SleighElf.as (Flash v1.87) onto the mobile Vehicle framework. See Sleigh.h.

#include "Sleigh.h"

#include <algorithm>
#include <cmath>

#include "CharacterB2D.h"
#include "DestructionListener.h"
#include "FlashParticles.h"
#include "GameplayControls.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "platform/compat/Box2DFloat.h"
#include "online/FlashPhysics.h"  // RESTORED (PC addition): per-step constants at 1/30 too

USING_NS_CC;

namespace {

const float kPx = 1.0f / 62.5f;         // metres per Flash world px (Flash m_physScale 62.5)
const float kSym = 1.0f / 125.0f;       // metres per Flash symbol px (character_scale 125)
const float kDeg = 0.017453292f;

void limitJoint(b2RevoluteJoint* joint, b2Body* a, b2Body* b, float lowerDeg, float upperDeg)
{
    if (!joint || !a || !b) {
        return;
    }
    float angle = b->GetAngle() - a->GetAngle();
    joint->SetLimits(-upperDeg * kDeg - angle, -lowerDeg * kDeg - angle);
}

}  // namespace

Sleigh* Sleigh::create(Vec2 position, std::string name, int groupID)
{
    Sleigh* sleigh = new (std::nothrow) Sleigh();
    if (sleigh && sleigh->init(position, name, groupID)) {
        sleigh->autorelease();
        return sleigh;
    }
    delete sleigh;
    return nullptr;
}

Sleigh::Sleigh()
    : _wheelMaxSpeed(30.0f),
      _accelStep(0.625f),
      _impulseLeft(1.3f),
      _impulseRight(1.7f),
      _impulseOffset(1.0f),
      _maxSpinAV(3.5f),
      _sleighSmashLimit(200.0f),
      _boostMax(50.0f),
      _boostStep(0.25f),
      _strapBreakImpulse(0.4f),
      _santa(nullptr),
      _santaEjected(false),
      _ejectImpulse(0.0f),
      _boostVal(0.0f),
      _boosting(false),
      _pumpCounter(0.0f),
      _frontContacts(0),
      _backContacts(0),
      _frame(0),
      _sleighBody(nullptr),
      _reinsBody(nullptr),
      _rollers{nullptr, nullptr, nullptr, nullptr},
      _rollerJoints{nullptr, nullptr, nullptr, nullptr},
      _reinsJoint(nullptr),
      _rearFixture(nullptr),
      _skiFixture(nullptr),
      _backFixture(nullptr),
      _seatFixture(nullptr),
      _frontFixture(nullptr),
      _bumperFixture(nullptr),
      _stemFixtures{nullptr, nullptr, nullptr},
      _sleighSmashed(false),
      _skiSmashed(false),
      _stemSmashed(false),
      _santaWheelRadius(1.0f),
      _sleighSprite(nullptr),
      _stemSprite(nullptr),
      _skiSprite(nullptr),
      _strapNode(nullptr),
      _skiLoop(nullptr),
      _bellLoop(nullptr)
{
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
}

Sleigh::~Sleigh()
{
    for (Sound* s : {_skiLoop, _bellLoop}) {
        if (s) {
            s->setFinishCallback(nullptr);
        }
    }
}

bool Sleigh::gameplay()
{
    return getSession()->getMode() == SessionModeGameplay;
}

bool Sleigh::init(Vec2 position, std::string name, int groupID)
{
    _maxTorque = 100000.0f;
    _maxSpeed = 30.0f;
    bool ok = Vehicle::init(position, name, groupID);
    if (ok) {
        getLevel()->addToPaintItem(this);
    }
    return ok;
}

// ---- helpers ---------------------------------------------------------------------------------

ValueMap Sleigh::shape(const std::string& name)
{
    return _bodiesDict.at("bodies").asValueMap().at(name).asValueMap();
}

b2Vec2 Sleigh::point(const std::string& name, float dx)
{
    // Guide points are under "joints"; a few markers drawn as boxes (sprayStart / sprayEnd) are
    // under "bodies".
    ValueMap joints = _bodiesDict.at("joints").asValueMap();
    auto it = joints.find(name);
    Vec2 p = it != joints.end() ? PointFromString(it->second.asString())
                                : PointFromString(shape(name).at("pos").asString());
    p += _origin;
    return b2Vec2(p.x + dx, p.y);
}

// Circle guides (100 px symbols) come out as squares in the plist: half the side is the radius.
float Sleigh::radiusOf(const std::string& name)
{
    ValueMap data = shape(name);
    if (data.find("radius") != data.end()) {
        return data.at("radius").asFloat();
    }
    return SizeFromString(data.at("size").asString()).width;
}

std::vector<b2Vec2> Sleigh::guidePolygon(const std::string& prefix, int maxCount)
{
    std::vector<b2Vec2> vertices;
    ValueMap joints = _bodiesDict.at("joints").asValueMap();
    for (int i = 1; i <= maxCount; i++) {
        auto it = joints.find(prefix + patch::to_string(i));
        if (it == joints.end()) {
            continue;
        }
        Vec2 p = PointFromString(it->second.asString());
        vertices.push_back(b2Vec2(p.x, p.y));  // body-local: the bodies start on the guide origin
    }
    return vertices;
}

b2Fixture* Sleigh::addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix, int count)
{
    std::vector<b2Vec2> vertices = guidePolygon(prefix, count);
    b2PolygonShape polygon;
    polygon.Set(vertices.data(), (int)vertices.size());
    def.shape = &polygon;
    return body->CreateFixture(&def);
}

void Sleigh::paintBody(b2Body* body, const std::string& frame, bool atCentre, int z)
{
    Sprite* sprite = Sprite::createWithSpriteFrameName(_name + "_" + frame + ".png");
    if (!sprite) {
        return;
    }
    getSession()->getVehicleForeground()->addChild(sprite, z);
    _painted.push_back({body, sprite, atCentre});
}

Sleigh::Elf* Sleigh::elfOf(CharacterB2D* character)
{
    for (Elf& e : _elves) {
        if (e.character && e.character == character) {
            return &e;
        }
    }
    return nullptr;
}

void Sleigh::startLoop(Sound*& sound, const std::string& name, b2Body* body, float fade)
{
    if (sound || !gameplay() || !body) {
        return;
    }
    sound = createBodySound(name, body, 1.0f, true);
    if (sound) {
        Sound** slot = &sound;
        sound->setFinishCallback([slot](int&) { *slot = nullptr; });
        sound->setMaxVolume(0.0f);
        sound->fadeTo(1.0f, fade, false);
    }
}

void Sleigh::stopLoop(Sound*& sound, float fade)
{
    if (sound) {
        sound->fadeTo(0.0f, fade, true);
        sound->setFinishCallback(nullptr);
        sound = nullptr;
    }
}

// ---- creation --------------------------------------------------------------------------------

void Sleigh::createSprites()
{
    // Flash adds the sleigh on top of everything, the presents just below it, the straps above.
    Node* foreground = getSession()->getVehicleForeground();
    _sleighSprite = Sprite::createWithSpriteFrameName(_name + "_sleigh.png");
    foreground->addChild(_sleighSprite, 10);
    _stemSprite = Sprite::createWithSpriteFrameName(_name + "_sleighStem.png");
    foreground->addChild(_stemSprite, 10);
    _skiSprite = Sprite::createWithSpriteFrameName(_name + "_sleighSki.png");
    foreground->addChild(_skiSprite, 10);
    _strapNode = DrawNode::create();
    foreground->addChild(_strapNode, 11);
}

void Sleigh::createBodies()
{
    b2World* world = getWorld();
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(_origin.x, _origin.y);

    // Flash: polygons density 3, friction 0.3, restitution 0.3, the rider's default filter
    // (category 0x104, mask 0x10e) in his group, except the shafts (group -2: the first elf's).
    b2FixtureDef poly;
    poly.density = 3.0f;
    poly.friction = 0.3f;
    poly.restitution = 0.3f;
    poly.filter = _defaultFilter;
    int16 group = _defaultFilter.groupIndex;
    _sleighBody = world->CreateBody(&bodyDef);
    _rearFixture = addPolygon(_sleighBody, poly, "rear_", 5);
    _skiFixture = addPolygon(_sleighBody, poly, "base_", 6);
    poly.filter.groupIndex = -1;
    _backFixture = addPolygon(_sleighBody, poly, "back_", 4);
    _seatFixture = addPolygon(_sleighBody, poly, "seat_", 5);
    poly.filter.groupIndex = group;
    _frontFixture = addPolygon(_sleighBody, poly, "front_", 5);
    poly.filter.groupIndex = -2;
    for (int i = 0; i < 3; i++) {
        _stemFixtures[i] = addPolygon(_sleighBody, poly, "stem" + patch::to_string(i + 1) + "_", 4);
    }
    b2FixtureDef circle;
    circle.density = 3.0f;
    circle.friction = 1.0f;
    circle.restitution = 0.1f;
    circle.filter = _defaultFilter;
    b2CircleShape bumper;
    bumper.m_radius = radiusOf("bumper");
    Vec2 bp = PointFromString(shape("bumper").at("pos").asString());
    bumper.m_p.Set(bp.x, bp.y);
    circle.shape = &bumper;
    _bumperFixture = _sleighBody->CreateFixture(&circle);
    _sleighBody->ResetMassData();

    // The reins: a small sensor box on the hands, sliding up and down on the sleigh.
    b2BodyDef reinsDef;
    reinsDef.type = b2_dynamicBody;
    reinsDef.position = point("handAnchor");
    _reinsBody = world->CreateBody(&reinsDef);
    b2PolygonShape box;
    box.SetAsBox(0.1f, 0.1f);
    b2FixtureDef sensor;
    sensor.shape = &box;
    sensor.isSensor = true;
    sensor.density = 3.0f;
    _reinsBody->CreateFixture(&sensor);
    _reinsBody->ResetMassData();

    // Presents: density 0.25, the default filter outside any group.
    b2FixtureDef present;
    present.density = 0.25f;
    present.friction = 0.3f;
    present.restitution = 0.3f;
    present.filter = _defaultFilter;
    present.filter.groupIndex = 0;
    for (int i = 1; i <= 8; i++) {
        ValueMap data = shape("box" + patch::to_string(i));
        Vec2 p = PointFromString(data.at("pos").asString()) + _origin;
        Size half = SizeFromString(data.at("size").asString());
        b2BodyDef def;
        def.type = b2_dynamicBody;
        def.position.Set(p.x, p.y);
        def.angle = data.at("rot").asFloat();
        b2Body* body = world->CreateBody(&def);
        b2PolygonShape b;
        b.SetAsBox(half.width, half.height);
        present.shape = &b;
        body->CreateFixture(&present);
        body->ResetMassData();
        _boxes.push_back(body);
        _floating.push_back(body);
        paintBody(body, "box" + patch::to_string(i), false, 9);
    }

    // Rollers under the ski (not drawn): density 3, friction 1; mid wheel 2 in the rider's group.
    const char* names[4] = {"backWheel", "midWheel1", "midWheel2", "frontWheel"};
    circle.filter = _defaultFilter;
    for (int i = 0; i < 4; i++) {
        circle.filter.groupIndex = i == 2 ? -1 : 0;
        b2CircleShape c;
        c.m_radius = radiusOf(names[i]);
        circle.shape = &c;
        Vec2 p = PointFromString(shape(names[i]).at("pos").asString()) + _origin;
        b2BodyDef def;
        def.type = b2_dynamicBody;
        def.position.Set(p.x, p.y);
        _rollers[i] = world->CreateBody(&def);
        b2Fixture* f = _rollers[i]->CreateFixture(&circle);
        if (i == 0) {
            _backWheelFixture = f;
        } else if (i == 3) {
            _frontWheelFixture = f;
        }
    }
    _santaWheelRadius = radiusOf("backWheel");
    _sprayStart = point("sprayStart") - b2Vec2(_origin.x, _origin.y);
    _sprayEnd = point("sprayEnd") - b2Vec2(_origin.x, _origin.y);

    for (b2Fixture* f : {_rearFixture, _frontFixture, _bumperFixture, _skiFixture, _stemFixtures[0],
                         _stemFixtures[1], _stemFixtures[2]}) {
        addToPostSolve(f);
    }
    for (b2Fixture* f : {_rearFixture, _frontFixture, _bumperFixture, _stemFixtures[0], _stemFixtures[1],
                         _stemFixtures[2]}) {
        addToBeginContact(f);
    }

    _floating.push_back(_sleighBody);
    _floating.push_back(_reinsBody);
    for (b2Body* r : _rollers) {
        _floating.push_back(r);
    }
}

void Sleigh::createJoints()
{
    b2World* world = getWorld();
    b2PrismaticJointDef reins;
    // Flash axis (0, 1) = down; here (0, -1) is down: same translations and speeds.
    reins.Initialize(_sleighBody, _reinsBody, point("handAnchor"), b2Vec2(0.0f, -1.0f));
    reins.enableLimit = true;
    reins.maxMotorForce = 1000.0f;
    reins.upperTranslation = 15.0f * kPx;
    reins.lowerTranslation = -100.0f * kPx;
    _reinsJoint = static_cast<b2PrismaticJoint*>(world->CreateJoint(&reins));

    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    for (int i = 0; i < 4; i++) {
        def.Initialize(_sleighBody, _rollers[i], _rollers[i]->GetPosition());
        _rollerJoints[i] = static_cast<b2RevoluteJoint*>(world->CreateJoint(&def));
        addWheelJoint(_rollerJoints[i], _rollers[i]);
    }
}

void Sleigh::createDictionaries()
{
    _contactImpulseDict[_frontFixture] = _sleighSmashLimit;
    _contactImpulseDict[_skiFixture] = _sleighSmashLimit;
    _contactImpulseDict[_stemFixtures[2]] = _sleighSmashLimit;
    _contactAddSounds[_backWheelFixture] = "SkiImpact1";
    _contactAddSounds[_frontWheelFixture] = "SkiImpact2";
    _contactAddSounds[_frontFixture] = "SleighImpact1";
    _contactAddSounds[_bumperFixture] = "SleighImpact2";
    _contactAddSounds[_stemFixtures[0]] = "SleighImpact2";
    _contactAddSounds[_stemFixtures[1]] = "SleighImpact1";
    _contactAddSounds[_stemFixtures[2]] = "SleighImpact1";
    _contactAddSounds[_rearFixture] = "SleighImpact3";
}

void Sleigh::lockWheels()
{
    for (b2RevoluteJoint* j : _rollerJoints) {
        if (j) {
            j->EnableLimit(true);
            j->SetLimits(0.0f, 0.0f);
        }
    }
}

// Flash SantaClaus.createJoints: hands on the reins, pelvis welded to the seat, feet on the floor.
void Sleigh::addSanta(CharacterB2D* santa)
{
    _santa = santa;
    Vehicle::addCharacter(santa);
    attachSanta(santa);
}

// QOL (PC addition): split from addSanta for a re-mount (re-grab vehicle). His eject took the
// reins away (checkReins): a re-mounted Santa holds on to the sleigh at the same point.
void Sleigh::attachSanta(CharacterB2D* santa)
{
    b2World* world = getWorld();
    limitJoint(santa->getNeckJoint(), santa->getChestBody(), santa->getHeadBody(), -5.0f, 5.0f);

    b2Body* reins = _reinsBody ? _reinsBody : _sleighBody;
    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    def.Initialize(reins, santa->getLowerArm1Body(), point("handAnchor"));
    addBodyVehicleJoint(santa->getLowerArm1Body(), world->CreateJoint(&def));
    def.Initialize(reins, santa->getLowerArm2Body(), point("handAnchor"));
    addBodyVehicleJoint(santa->getLowerArm2Body(), world->CreateJoint(&def));
    def.Initialize(_sleighBody, santa->getPelvisBody(), santa->getPelvisBody()->GetWorldCenter());
    def.enableLimit = true;
    def.lowerAngle = def.upperAngle = 0.0f;
    addBodyVehicleJoint(santa->getPelvisBody(), world->CreateJoint(&def));
    def.enableLimit = false;
    def.Initialize(_sleighBody, santa->getLowerLeg1Body(), point("footAnchor"));
    addBodyVehicleJoint(santa->getLowerLeg1Body(), world->CreateJoint(&def));
    def.Initialize(_sleighBody, santa->getLowerLeg2Body(), point("footAnchor"));
    addBodyVehicleJoint(santa->getLowerLeg2Body(), world->CreateJoint(&def));
}

// Flash SleighElf.createBodies / createJoints.
void Sleigh::addElf(CharacterB2D* elfCharacter, int index)
{
    Elf& elf = _elves[index];
    elf.character = elfCharacter;
    elf.offsetX = index == 0 ? 0.0f : 110.0f * kSym;
    elf.verticalOffset = (index == 0 ? 35.0f : 70.0f) * kPx;
    if (std::find(_characters.begin(), _characters.end(), elfCharacter) == _characters.end()) {
        Vehicle::addCharacter(elfCharacter);
    }
    b2World* world = getWorld();
    CharacterB2D* c = elfCharacter;
    limitJoint(c->getElbowJoint1(), c->getUpperArm1Body(), c->getLowerArm1Body(), -90.0f, 0.0f);
    limitJoint(c->getElbowJoint2(), c->getUpperArm2Body(), c->getLowerArm2Body(), -90.0f, 0.0f);
    limitJoint(c->getShoulderJoint1(), c->getChestBody(), c->getUpperArm1Body(), -170.0f, 0.0f);
    limitJoint(c->getShoulderJoint2(), c->getChestBody(), c->getUpperArm2Body(), -170.0f, 0.0f);
    limitJoint(c->getNeckJoint(), c->getChestBody(), c->getHeadBody(), -10.0f, 0.0f);

    // Stem: no collisions, density 4. Wheel: density 6, friction 1, category 0xf400, mask 0x208,
    // in the elf's group.
    ValueMap stem = shape("elf_stem");
    Vec2 sp = PointFromString(stem.at("pos").asString()) + _origin;
    Size half = SizeFromString(stem.at("size").asString());
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position.Set(sp.x + elf.offsetX, sp.y);
    def.angle = stem.at("rot").asFloat();
    elf.stem = world->CreateBody(&def);
    b2PolygonShape box;
    box.SetAsBox(half.width, half.height);
    b2FixtureDef stemFixture;
    stemFixture.shape = &box;
    stemFixture.density = 4.0f;
    stemFixture.friction = 0.3f;
    stemFixture.restitution = 0.3f;
    stemFixture.filter.categoryBits = 0;
    stemFixture.filter.maskBits = 0;
    elf.stem->CreateFixture(&stemFixture);
    elf.stem->ResetMassData();

    Vec2 wp = PointFromString(shape("elf_wheel").at("pos").asString()) + _origin;
    b2BodyDef wheelDef;
    wheelDef.type = b2_dynamicBody;
    wheelDef.position.Set(wp.x + elf.offsetX, wp.y);
    elf.wheel = world->CreateBody(&wheelDef);
    b2CircleShape wheelShape;
    wheelShape.m_radius = radiusOf("elf_wheel");
    b2FixtureDef wheelFixture;
    wheelFixture.shape = &wheelShape;
    wheelFixture.density = 6.0f;
    wheelFixture.friction = 1.0f;
    wheelFixture.restitution = 0.1f;
    wheelFixture.filter.categoryBits = 0xf400;
    wheelFixture.filter.maskBits = 0x208;
    wheelFixture.filter.groupIndex = (int16)c->getGroupIndex();
    elf.wheelFixture = elf.wheel->CreateFixture(&wheelFixture);
    addToBeginContact(elf.wheelFixture);
    addToEndContact(elf.wheelFixture);
    float multiplier = _santaWheelRadius / wheelShape.m_radius;
    elf.maxSpeed = _wheelMaxSpeed * multiplier;
    elf.accelStep = _accelStep * multiplier;

    b2PrismaticJointDef slide;
    slide.Initialize(_sleighBody, elf.stem, elf.stem->GetPosition(), b2Vec2(0.0f, -1.0f));
    slide.enableLimit = true;
    slide.upperTranslation = elf.verticalOffset;
    slide.lowerTranslation = -elf.verticalOffset;
    world->CreateJoint(&slide);
    b2RevoluteJointDef pin;
    pin.maxMotorTorque = _maxTorque;
    pin.Initialize(elf.stem, elf.wheel, elf.wheel->GetPosition());
    elf.wheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&pin));
    pin.maxMotorTorque = 0.0f;
    pin.Initialize(elf.wheel, c->getLowerLeg1Body(), point("elf_foot1Anchor", elf.offsetX));
    elf.foot1 = world->CreateJoint(&pin);
    addBodyVehicleJoint(c->getLowerLeg1Body(), elf.foot1);
    pin.Initialize(elf.wheel, c->getLowerLeg2Body(), point("elf_foot2Anchor", elf.offsetX));
    elf.foot2 = world->CreateJoint(&pin);
    addBodyVehicleJoint(c->getLowerLeg2Body(), elf.foot2);
    pin.Initialize(elf.stem, c->getChestBody(), c->getChestBody()->GetPosition());
    pin.enableLimit = true;
    pin.lowerAngle = -20.0f * kDeg;  // Flash 0..20 degrees (y down)
    pin.upperAngle = 0.0f;
    addBodyVehicleJoint(c->getChestBody(), world->CreateJoint(&pin));

    elf.headPoint = c->getHeadBody()->GetWorldPoint(b2Vec2(-18.0f * kSym, -4.0f * kSym));
    elf.chestPoint = c->getChestBody()->GetWorldPoint(b2Vec2(0.0f, -20.0f * kSym));

    if (_elves[0].character && _elves[1].character) {
        buildStraps();
    }
}

// ---- straps ----------------------------------------------------------------------------------

b2Body* Sleigh::strapNode(b2Vec2 at, b2Vec2 velocity)
{
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = at;
    b2Body* body = getWorld()->CreateBody(&def);
    b2CircleShape c;
    c.m_radius = 3.0f * kPx;
    b2FixtureDef f;
    f.shape = &c;
    f.density = 0.25f;
    f.friction = 1.0f;
    f.restitution = 0.1f;
    f.filter.categoryBits = 0;
    f.filter.maskBits = 0;
    body->CreateFixture(&f);
    body->ResetMassData();
    body->SetLinearVelocity(velocity);
    _floating.push_back(body);
    return body;
}

void Sleigh::link(Strap& strap, size_t i)
{
    b2Body* a = strap.bodies[i];
    b2Body* b = strap.bodies[i + 1];
    if (!a || !b) {
        strap.joints[i] = nullptr;
        return;
    }
    b2DistanceJointDef def;
    def.Initialize(a, b, a->GetWorldPoint(strap.locals[i]), b->GetWorldPoint(strap.locals[i + 1]));
    def.length = strap.lengths[i];
    strap.joints[i] = static_cast<b2DistanceJoint*>(getWorld()->CreateJoint(&def));
    getSession()->getDestructionListener()->addJointListener(strap.joints[i], this);
}

void Sleigh::unlink(Strap& strap, size_t i)
{
    if (i < strap.joints.size() && strap.joints[i]) {
        b2Joint* j = strap.joints[i];
        strap.joints[i] = nullptr;
        getWorld()->DestroyJoint(j);
    }
}

// Flash createStraps: `segments` rigid links from the strap's last body to `to`, through free nodes.
void Sleigh::chain(Strap& strap, b2Body* to, b2Vec2 toPoint, int segments, bool bells)
{
    b2Vec2 from = strap.bodies.back()->GetWorldPoint(strap.locals.back());
    b2Vec2 step = (1.0f / segments) * (toPoint - from);
    for (int i = 0; i < segments; i++) {
        b2Body* next = to;
        b2Vec2 nextPoint = toPoint;
        if (i < segments - 1) {
            next = strapNode(from + (float)(i + 1) * step, b2Vec2_zero);
            nextPoint = next->GetPosition();
            if (bells) {
                // Flash ChristmasBellMC, the strap hooked 1 px above its centre.
                if (Sprite* bell = Sprite::createWithSpriteFrameName(_name + "_bell.png")) {
                    getSession()->getVehicleForeground()->addChild(bell, 12);
                    _painted.push_back({next, bell, false});
                }
                nextPoint = next->GetWorldPoint(b2Vec2(0.0f, 1.0f * kPx));
            }
        }
        b2Vec2 previous = strap.bodies.back()->GetWorldPoint(strap.locals.back());
        strap.bodies.push_back(next);
        strap.locals.push_back(next->GetLocalPoint(nextPoint));
        strap.lengths.push_back((nextPoint - previous).Length());
        strap.joints.push_back(nullptr);
        link(strap, strap.joints.size() - 1);
    }
}

void Sleigh::buildStraps()
{
    CharacterB2D* elf1 = _elves[0].character;
    CharacterB2D* elf2 = _elves[1].character;
    b2Vec2 headLocal(-18.0f * kSym, -4.0f * kSym);
    b2Vec2 chestLocal(0.0f, -20.0f * kSym);
    _headStrap.bodies = {_reinsBody};
    _headStrap.locals = {b2Vec2_zero};
    chain(_headStrap, elf1->getHeadBody(), elf1->getHeadBody()->GetWorldPoint(headLocal), 8, false);
    chain(_headStrap, elf2->getHeadBody(), elf2->getHeadBody()->GetWorldPoint(headLocal), 4, false);
    _chestStrap1.bodies = {_sleighBody};
    _chestStrap1.locals = {_sleighBody->GetLocalPoint(point("strap1Anchor"))};
    chain(_chestStrap1, elf1->getChestBody(), elf1->getChestBody()->GetWorldPoint(chestLocal), 3, false);
    _chestStrap2.bodies = {_sleighBody};
    _chestStrap2.locals = {_sleighBody->GetLocalPoint(point("strap2Anchor"))};
    chain(_chestStrap2, elf2->getChestBody(), elf2->getChestBody()->GetWorldPoint(chestLocal), 5, true);
}

void Sleigh::jointWillBeDestroyed(b2Joint* joint)
{
    for (Strap* strap : {&_headStrap, &_chestStrap1, &_chestStrap2}) {
        for (b2DistanceJoint*& j : strap->joints) {
            if (j == joint) {
                j = nullptr;
            }
        }
    }
}

void Sleigh::replaceStrapBody(Strap& strap, size_t k, b2Body* body, b2Vec2 world)
{
    if (k >= strap.bodies.size()) {
        return;
    }
    if (k > 0) {
        unlink(strap, k - 1);
    }
    if (k < strap.joints.size()) {
        unlink(strap, k);
    }
    strap.bodies[k] = body;
    strap.locals[k] = body->GetLocalPoint(world);
    if (k > 0) {
        link(strap, k - 1);
    }
    if (k < strap.joints.size()) {
        link(strap, k);
    }
}

// Flash elfHeadRemove: the head strap now runs through a free node where the elf's head was.
void Sleigh::elfHeadRemove(Elf& elf, bool smashed)
{
    if (!elf.headAttached || _headStrap.bodies.size() < 13) {
        return;
    }
    elf.headAttached = false;
    size_t k = &elf == &_elves[0] ? 8 : _headStrap.bodies.size() - 1;
    b2Body* node = strapNode(elf.headPoint, elf.headVelocity);
    replaceStrapBody(_headStrap, k, node, elf.headPoint);
    (void)smashed;
}

// Flash elfChestRemove: the chest strap ends on a free node.
void Sleigh::elfChestRemove(Elf& elf, bool smashed)
{
    if (!elf.chestAttached) {
        return;
    }
    elf.chestAttached = false;
    Strap& strap = &elf == &_elves[0] ? _chestStrap1 : _chestStrap2;
    if (strap.bodies.size() < 2) {
        return;
    }
    b2Body* node = strapNode(elf.chestPoint, elf.chestVelocity);
    replaceStrapBody(strap, strap.bodies.size() - 1, node, elf.chestPoint);
    (void)smashed;
}

// Flash breakStrap: the chest strap comes off the shafts.
void Sleigh::breakStrap(Elf& elf)
{
    elf.strappedIn = false;
    Strap& strap = &elf == &_elves[0] ? _chestStrap1 : _chestStrap2;
    if (strap.bodies.size() < 2 || !strap.bodies[0]) {
        return;
    }
    b2Vec2 at = strap.bodies[0]->GetWorldPoint(strap.locals[0]);
    b2Body* node = strapNode(at, strap.bodies[0]->GetLinearVelocityFromWorldPoint(at));
    replaceStrapBody(strap, 0, node, at);
    if (gameplay()) {
        createBodySound(&elf == &_elves[0] ? "StrapSnap1" : "StrapSnap2", node, 1.0f, false);
    }
}

// Flash checkReigns: with both hands off, the reins come loose (their strap end becomes a node).
void Sleigh::checkReins()
{
    if (!_reinsBody) {
        return;
    }
    auto held = [this](b2Body* body) {
        auto it = _bodyVehicleJointDict.find(body);
        return it != _bodyVehicleJointDict.end() && it->second != nullptr;
    };
    if (_santa && (held(_santa->getLowerArm1Body()) || held(_santa->getLowerArm2Body()))) {
        return;
    }
    b2World* world = getWorld();
    if (_reinsJoint) {
        world->DestroyJoint(_reinsJoint);
        _reinsJoint = nullptr;
    }
    if (!_headStrap.bodies.empty() && _headStrap.bodies[0] == _reinsBody) {
        b2Vec2 at = _reinsBody->GetPosition();
        b2Body* node = strapNode(at, _reinsBody->GetLinearVelocity());
        replaceStrapBody(_headStrap, 0, node, at);
    }
    _floating.erase(std::remove(_floating.begin(), _floating.end(), _reinsBody), _floating.end());
    world->DestroyBody(_reinsBody);
    _reinsBody = nullptr;
}

// ---- riders --------------------------------------------------------------------------------------

void Sleigh::handleInjury(CharacterInjury injury, CharacterB2D* character)
{
    if (std::find(_characters.begin(), _characters.end(), character) == _characters.end()) {
        return;
    }
    if (character == _santa) {
        switch (injury) {
        case CharacterInjuryHeadSmash:
        case CharacterInjuryNeckBreak:
        case CharacterInjuryTorsoBreak:
            handleFatalWound(character);
            return;
        case CharacterInjuryChestSmash:   // (Flash: Santa stays in the sleigh)
        case CharacterInjuryPelvisSmash:
        case CharacterInjuryDeath:
        case CharacterInjuryFoot1Smash:
        case CharacterInjuryFoot2Smash:
            return;
        default:
            Vehicle::handleInjury(injury, character);
            return;
        }
    }
    Elf* elf = elfOf(character);
    if (!elf) {
        return;
    }
    switch (injury) {
    case CharacterInjuryHeadSmash:
        elfHeadRemove(*elf, true);
        return;
    case CharacterInjuryChestSmash:
        elfChestRemove(*elf, true);
        return;
    case CharacterInjuryTorsoBreak:
    case CharacterInjuryDeath:
        disableRunning(*elf);
        return;
    case CharacterInjuryKnee1Break:
    case CharacterInjuryHip1Break:
        destroyJointsForBody(character->getLowerLeg1Body());
        elf->foot1 = nullptr;
        checkLegs(*elf);
        return;
    case CharacterInjuryKnee2Break:
    case CharacterInjuryHip2Break:
        destroyJointsForBody(character->getLowerLeg2Body());
        elf->foot2 = nullptr;
        checkLegs(*elf);
        return;
    default:
        return;  // (Flash: arms, feet and the pelvis do not let go of anything)
    }
}

void Sleigh::checkStateOfCharacter(CharacterB2D* character)
{
    if (character != _santa) {
        return;
    }
    checkReins();
    auto held = [this](b2Body* body) {
        auto it = _bodyVehicleJointDict.find(body);
        return it != _bodyVehicleJointDict.end() && it->second != nullptr;
    };
    if (!held(_santa->getLowerArm1Body()) && !held(_santa->getLowerArm2Body()) &&
        !held(_santa->getLowerLeg1Body()) && !held(_santa->getLowerLeg2Body())) {
        ejectCharacter(_santa);
    }
}

bool Sleigh::ejectCharacter(CharacterB2D* character)
{
    if (!Vehicle::ejectCharacter(character)) {
        return false;
    }
    if (character == _santa) {
        santaEject();
    } else if (Elf* elf = elfOf(character)) {
        elfEject(*elf);
    }
    return true;
}

void Sleigh::ejectAllCharacters()
{
    if (_santa && !_santaEjected) {
        _ejectImpulse = 5.0f;
        ejectCharacter(_santa);
    }
}

// Flash SantaClaus.eject.
void Sleigh::santaEject()
{
    _santaEjected = true;
    _ejected = true;
    checkReins();
    if (_sleighBody) {
        for (b2Fixture* f = _sleighBody->GetFixtureList(); f; f = f->GetNext()) {
            b2Filter filter = f->GetFilterData();
            if (filter.groupIndex == -1) {
                filter.groupIndex = 0;
                f->SetFilterData(filter);
            }
        }
    }
    if (_rollers[2] && _rollers[2]->GetFixtureList()) {
        b2Filter filter = _rollers[2]->GetFixtureList()->GetFilterData();
        filter.groupIndex = 0;
        _rollers[2]->GetFixtureList()->SetFilterData(filter);
    }
    b2Body* base = _sleighBody;
    if (base && _santa) {
        float angle = base->GetAngle() + (float)M_PI_2;
        b2Vec2 impulse(cosf(angle) * _ejectImpulse, sinf(angle) * _ejectImpulse);
        for (b2Body* b : {_santa->getChestBody(), _santa->getPelvisBody()}) {
            if (b) {
                b->ApplyLinearImpulse(impulse, b->GetWorldCenter(), true);
            }
        }
    }
    for (b2RevoluteJoint* j : _rollerJoints) {
        if (j) {
            j->EnableMotor(false);
        }
    }
    stopFlight();
}

// Flash SleighElf.eject: off the wheel, the straps keep only free nodes.
void Sleigh::elfEject(Elf& elf)
{
    elf.ejected = true;
    elf.foot1 = elf.foot2 = nullptr;  // (Vehicle::ejectCharacter destroyed them)
    checkLegs(elf);
    elfHeadRemove(elf, false);
    elfChestRemove(elf, false);
}

void Sleigh::disableRunning(Elf& elf)
{
    if (!elf.character) {
        return;
    }
    destroyJointsForBody(elf.character->getLowerLeg1Body());
    destroyJointsForBody(elf.character->getLowerLeg2Body());
    elf.foot1 = elf.foot2 = nullptr;
    checkLegs(elf);
}

// Flash checkLegsOk: with both feet off the wheel, the stem and wheel go.
void Sleigh::checkLegs(Elf& elf)
{
    if (!elf.legsOk || elf.foot1 || elf.foot2) {
        elf.runPose = 0;
        return;
    }
    elf.legsOk = false;
    elf.runPose = 0;
    if (elf.character) {
        destroyJointsForBody(elf.character->getChestBody());  // the stem's chest joint
    }
    b2World* world = getWorld();
    if (elf.wheel) {
        removeBeginContact(elf.wheelFixture);
        removeEndContact(elf.wheelFixture);
        fixtureWillBeDestroyed(elf.wheelFixture);
        world->DestroyBody(elf.wheel);
        elf.wheel = nullptr;
        elf.wheelFixture = nullptr;
        elf.wheelJoint = nullptr;
        elf.wheelContacts = 0;
    }
    if (elf.stem) {
        world->DestroyBody(elf.stem);
        elf.stem = nullptr;
    }
}

// ---- controls ------------------------------------------------------------------------------------

void Sleigh::forwardButtonPressed()
{
    if (_santaEjected || !_rollerJoints[1]) {
        return;
    }
    if (!_rollerJoints[0]->IsMotorEnabled()) {
        for (b2RevoluteJoint* j : _rollerJoints) {
            j->EnableMotor(true);
        }
        if (_reinsJoint) {
            _reinsJoint->EnableMotor(true);
            _pumpCounter = 0.0f;
        }
    }
    // Flash upPressedActions (clockwise = forward = mobile negative).
    float speed = owb2::jointSpeed(_rollerJoints[1]);
    // RESTORED (PC addition): _accelStep is per 60 Hz step; online::perStep converts it to the
    // current step (x2 at the browser physics profile's 1/30, online/FlashPhysics.h).
    const float accelStep = online::perStep(_accelStep);
    float next = speed > 0.0f ? 0.0f : (speed > -_wheelMaxSpeed ? speed - accelStep : speed);
    for (b2RevoluteJoint* j : _rollerJoints) {
        j->SetMotorSpeed(next);
    }
    if (_reinsJoint) {
        _reinsJoint->SetMotorSpeed(sinf(_pumpCounter) * 6.0f);
        _pumpCounter += online::perStep(0.3f);  // RESTORED (PC addition): 0.3 per 60 Hz step
    }
}

void Sleigh::backButtonPressed()
{
    if (_santaEjected || !_rollerJoints[2]) {
        return;
    }
    if (!_rollerJoints[0]->IsMotorEnabled()) {
        for (b2RevoluteJoint* j : _rollerJoints) {
            j->EnableMotor(true);
        }
        if (_reinsJoint) {
            _reinsJoint->EnableMotor(true);
            _pumpCounter = 0.0f;
        }
    }
    float speed = owb2::jointSpeed(_rollerJoints[2]);
    const float accelStep = online::perStep(_accelStep);  // RESTORED (PC addition), as above
    float next = speed < 0.0f ? 0.0f : (speed < _wheelMaxSpeed ? speed + accelStep : speed);
    for (b2RevoluteJoint* j : _rollerJoints) {
        j->SetMotorSpeed(next);
    }
    if (_reinsJoint) {
        _reinsJoint->SetMotorSpeed(sinf(_pumpCounter) * 6.0f);
        _pumpCounter += online::perStep(0.3f);  // RESTORED (PC addition): 0.3 per 60 Hz step
    }
}

void Sleigh::forwardBackButtonsNull()
{
    if (_rollerJoints[0] && _rollerJoints[0]->IsMotorEnabled()) {
        for (b2RevoluteJoint* j : _rollerJoints) {
            if (j) {
                j->EnableMotor(false);
            }
        }
        if (_reinsJoint) {
            _reinsJoint->EnableMotor(false);
        }
    }
    if (_currentPose == VehiclePoseForward || _currentPose == VehiclePoseBack) {
        setCurrentPose(VehiclePoseNone);
    }
}

// Flash left/rightPressedActions: a torque couple at +- impulseOffset.
void Sleigh::leanBackButtonPressed()
{
    if (_santaEjected || !_sleighBody) {
        return;
    }
    float angle = _sleighBody->GetAngle();
    float av = _sleighBody->GetAngularVelocity();
    float factor = std::min(std::max((av - _maxSpinAV) / -_maxSpinAV, 0.0f), 1.0f);
    float magnitude = _impulseLeft * s_timeStepOverFlashTimeStep * factor;
    b2Vec2 up(-sinf(angle) * magnitude, cosf(angle) * magnitude);
    b2Vec2 c = _sleighBody->GetLocalCenter();
    _sleighBody->ApplyLinearImpulse(-up, _sleighBody->GetWorldPoint(b2Vec2(c.x + _impulseOffset, c.y)), true);
    _sleighBody->ApplyLinearImpulse(up, _sleighBody->GetWorldPoint(b2Vec2(c.x - _impulseOffset, c.y)), true);
}

void Sleigh::leanForwardButtonPressed()
{
    if (_santaEjected || !_sleighBody) {
        return;
    }
    float angle = _sleighBody->GetAngle();
    float av = _sleighBody->GetAngularVelocity();
    float factor = std::min(std::max((av + _maxSpinAV) / _maxSpinAV, 0.0f), 1.0f);
    float magnitude = _impulseRight * s_timeStepOverFlashTimeStep * factor;
    b2Vec2 up(-sinf(angle) * magnitude, cosf(angle) * magnitude);
    b2Vec2 c = _sleighBody->GetLocalCenter();
    _sleighBody->ApplyLinearImpulse(up, _sleighBody->GetWorldPoint(b2Vec2(c.x + _impulseOffset, c.y)), true);
    _sleighBody->ApplyLinearImpulse(-up, _sleighBody->GetWorldPoint(b2Vec2(c.x - _impulseOffset, c.y)), true);
}

// Flash spacePressedActions: flight while the boost lasts.
void Sleigh::special1ButtonPressed()
{
    if (_santaEjected) {
        return;
    }
    _boosting = true;
}

void Sleigh::special1ButtonNull()
{
    _boosting = false;
}

void Sleigh::stopFlight()
{
    _boosting = false;
    stopLoop(_bellLoop, 0.5f);
}

void Sleigh::updateMeter()
{
    if (!gameplay()) {
        return;
    }
    if (GameplayControls* controls = getSession()->getControls()) {
        controls->setMeterPercentage(1.0f - _boostVal / _boostMax);
    }
}

void Sleigh::antiGravity(bool elvesOnly)
{
    b2Vec2 lift = -getWorld()->GetGravity();
    lift *= LevelItem::s_timeStep;  // RESTORED (PC addition): one step (was 1/60)
    std::vector<b2Body*> bodies;
    if (!elvesOnly) {
        bodies = _floating;
        if (_santa) {
            for (b2Body* b : {_santa->getHeadBody(), _santa->getChestBody(), _santa->getPelvisBody(),
                              _santa->getUpperArm1Body(), _santa->getUpperArm2Body(), _santa->getLowerArm1Body(),
                              _santa->getLowerArm2Body(), _santa->getUpperLeg1Body(), _santa->getUpperLeg2Body(),
                              _santa->getLowerLeg1Body(), _santa->getLowerLeg2Body(), _santa->getUpperArm3Body(),
                              _santa->getUpperArm4Body(), _santa->getUpperLeg3Body(), _santa->getUpperLeg4Body()}) {
                bodies.push_back(b);
            }
        }
    }
    for (Elf& e : _elves) {
        CharacterB2D* c = e.character;
        if (!c) {
            continue;
        }
        for (b2Body* b : {c->getHeadBody(), c->getChestBody(), c->getPelvisBody(), c->getUpperArm1Body(),
                          c->getUpperArm2Body(), c->getLowerArm1Body(), c->getLowerArm2Body(),
                          c->getUpperLeg1Body(), c->getUpperLeg2Body(), c->getLowerLeg1Body(),
                          c->getLowerLeg2Body(), c->getUpperArm3Body(), c->getUpperArm4Body(),
                          c->getUpperLeg3Body(), c->getUpperLeg4Body(), e.stem, e.wheel}) {
            bodies.push_back(b);
        }
    }
    for (b2Body* b : bodies) {
        if (b && b->IsAwake()) {
            b->SetLinearVelocity(b->GetLinearVelocity() + lift);
        }
    }
}

// Flash SleighElf up/downPressedActions (the elves' wheels; Flash clockwise = mobile negative).
void Sleigh::elfRun(Elf& elf, int direction)
{
    if (elf.ejected || !elf.legsOk || !elf.wheelJoint || (elf.character && elf.character->getDead())) {
        return;
    }
    if (direction == 0) {
        if (elf.wheelJoint->IsMotorEnabled()) {
            elf.wheelJoint->EnableMotor(false);
        }
        elf.runPose = 0;
        return;
    }
    if (!elf.wheelJoint->IsMotorEnabled()) {
        elf.wheelJoint->EnableMotor(true);
    }
    float speed = -owb2::jointSpeed(elf.wheelJoint);  // Flash sense
    const float accelStep = online::perStep(elf.accelStep);  // RESTORED (PC addition): per 60 Hz step
    float next = direction > 0 ? (speed < elf.maxSpeed ? speed + accelStep : speed)
                               : (speed > -elf.maxSpeed ? speed - accelStep : speed);
    elf.wheelJoint->SetMotorSpeed(-next);
    elf.runPose = direction;
}

// Flash runForwardPose / runBackwardPose: the arms swing with the opposite leg. Flash measures the
// knee and hip from their lower limit; here that is the upper limit minus the angle.
void Sleigh::elfRunPose(Elf& elf)
{
    CharacterB2D* c = elf.character;
    if (!c || elf.runPose == 0 || elf.ejected) {
        return;
    }
    float elbowGain = elf.runPose > 0 ? 1.0f : 2.6f;
    float shoulderGain = elf.runPose > 0 ? 3.0f : 4.18f;
    struct Side
    {
        b2Joint* foot;
        b2RevoluteJoint* knee;
        b2RevoluteJoint* hip;
        b2RevoluteJoint* elbow;
        b2RevoluteJoint* shoulder;
    };
    Side sides[2] = {{elf.foot1, c->getKneeJoint1(), c->getHipJoint1(), c->getElbowJoint2(), c->getShoulderJoint2()},
                     {elf.foot2, c->getKneeJoint2(), c->getHipJoint2(), c->getElbowJoint1(), c->getShoulderJoint1()}};
    for (const Side& s : sides) {
        if (!s.foot || !s.knee || !s.hip) {
            continue;
        }
        float knee = (s.knee->GetUpperLimit() - owb2::jointAngle(s.knee)) / (150.0f * kDeg);
        float hip = (s.hip->GetUpperLimit() - owb2::jointAngle(s.hip)) / (160.0f * kDeg);
        if (s.elbow) {
            c->setJoint(s.elbow, -elbowGain * knee, 15.0f, 10.0f);
        }
        if (s.shoulder) {
            c->setJoint(s.shoulder, -shoulderGain * hip, 15.0f, 10.0f);
        }
    }
}

void Sleigh::extraControls(unsigned char state)
{
    int direction = (state & 0x01) ? 1 : (state & 0x02) ? -1 : 0;
    if ((state & 0x03) == 0x03) {
        direction = 0;
    }
    for (Elf& e : _elves) {
        elfRun(e, direction);
    }
    if (!_santaEjected && (state & 0x20)) {
        // Flash shiftPressedActions: let go of the elves that cannot run any more.
        for (Elf& e : _elves) {
            if (!e.legsOk && !e.ejected && e.character) {
                ejectCharacter(e.character);
            }
        }
    }
    if (_santaEjected && (state & 0x80)) {
        // Flash zPressedActions once Santa is off: release both elves.
        for (Elf& e : _elves) {
            if (!e.ejected && e.character) {
                ejectCharacter(e.character);
            }
        }
    }
}

// ---- per frame -----------------------------------------------------------------------------------

void Sleigh::actions()
{
    _frame++;
    bool select = getSession()->getMode() == SessionModeCharacterSelect;
    // Flash floatElves in the character menu: the elves hang in the air.
    if (select) {
        antiGravity(true);
    }

    // Flight (Flash spacePressedActions / spaceNullActions, per 30 Hz frame -> per step / 2).
    // RESTORED (PC addition): per step at the current rate (online::perStep: / 2 only at 1/60;
    // the snow spray once per Flash frame, online::stepsPerFlashFrame).
    const float boostStep = online::perStep(_boostStep);
    if (_boosting && !_santaEjected) {
        _boostVal = std::min(_boostMax, _boostVal + boostStep);
        if (_boostVal < _boostMax) {
            antiGravity(false);
            if (_sleighBody && (_frame % online::stepsPerFlashFrame()) == 0) {
                b2Vec2 a = _sprayStart;
                b2Vec2 b = _sprayEnd;
                std::vector<std::string> frames;
                for (int i = 1; i <= 7; i++) {
                    frames.push_back(_name + "_snowflake_" + patch::to_string(i) + ".png");
                }
                if (restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession())) {
                    for (int i = 0; i < 5; i++) {
                        float t = CCRANDOM_0_1();
                        b2Vec2 local = a + t * (b - a);
                        particles->snow(frames, _sleighBody->GetWorldPoint(local),
                                        _sleighBody->GetLinearVelocityFromLocalPoint(local));
                    }
                }
            }
            startLoop(_bellLoop, "SleighBellLoop", _sleighBody, 0.5f);
        } else {
            stopLoop(_bellLoop, 0.5f);
        }
    } else {
        _boostVal = std::max(0.0f, _boostVal - boostStep);
        stopLoop(_bellLoop, 0.5f);
    }
    updateMeter();

    // Ski loop (Flash: back roller faster than 12 rad/s while touching).
    if (_frontContacts + _backContacts > 0 && _rollers[0] && std::fabs(_rollers[0]->GetAngularVelocity()) > 12.0f) {
        startLoop(_skiLoop, "SkiLoop", _sleighBody, 0.2f);
    } else {
        stopLoop(_skiLoop, 0.2f);
    }

    // The elves: footsteps, run poses, the last known strap points, mourning.
    for (int i = 0; i < 2; i++) {
        Elf& e = _elves[i];
        CharacterB2D* c = e.character;
        if (!c) {
            continue;
        }
        if (c->getHeadBody()) {
            e.headPoint = c->getHeadBody()->GetWorldPoint(b2Vec2(-18.0f * kSym, -4.0f * kSym));
            e.headVelocity = c->getHeadBody()->GetLinearVelocity();
        }
        if (c->getChestBody()) {
            e.chestPoint = c->getChestBody()->GetWorldPoint(b2Vec2(0.0f, -20.0f * kSym));
            e.chestVelocity = c->getChestBody()->GetLinearVelocity();
        }
        if (c->getDead() && !e.wasDead) {
            e.wasDead = true;
            // Flash SantaClaus.mourn: "Mourn2" for the first elf, "Mourn1" for the second.
            if (_santa && !_santa->getDead()) {
                _santa->addVocalsWithName(i == 0 ? "Mourn2" : "Mourn1", VocalPriority3);
            }
        }
        elfRunPose(e);
        if (e.wheel && e.wheelContacts > 0 && std::fabs(e.wheel->GetAngularVelocity()) > 5.0f && gameplay()) {
            float cosine = cosf(e.wheel->GetAngle());
            if (!e.stepSign) {
                if (cosine > 0.9396f) {
                    e.stepSign = true;
                    if (e.foot2) {
                        createBodySound("Step" + patch::to_string((int)ceilf(CCRANDOM_0_1() * 5.0f) * 2), e.wheel, 1.0f, false);
                    }
                }
            } else if (cosine < -0.9396f) {
                e.stepSign = false;
                if (e.foot1) {
                    createBodySound("Step" + patch::to_string((int)ceilf(CCRANDOM_0_1() * 5.0f) * 2 - 1), e.wheel, 1.0f, false);
                }
            }
        }
    }

    checkPose();
    checkJoints();
    handleContactAdds();
    handleContactResults();
}

void Sleigh::paint()
{
    float ptm = getPtm();
    if (_sleighBody) {
        b2Vec2 p = _sleighBody->GetPosition();
        float rot = -CC_RADIANS_TO_DEGREES(_sleighBody->GetAngle());
        for (Sprite* s : {_sleighSprite, _stemSprite, _skiSprite}) {
            s->setPosition(Vec2(p.x * ptm, p.y * ptm));
            s->setRotation(rot);
        }
    }
    for (const Painted& p : _painted) {
        b2Vec2 c = p.atCentre ? p.body->GetWorldCenter() : p.body->GetPosition();
        p.sprite->setPosition(Vec2(c.x * ptm, c.y * ptm));
        p.sprite->setRotation(-CC_RADIANS_TO_DEGREES(p.body->GetAngle()));
    }
    // The straps (Flash: 1 px, chest straps 1.5 px, #333333).
    _strapNode->clear();
    Color4F colour(Color3B(0x33, 0x33, 0x33));
    float k = ptm * kPx;  // points per Flash px
    for (Strap* strap : {&_headStrap, &_chestStrap1, &_chestStrap2}) {
        float width = (strap == &_headStrap ? 1.0f : 1.5f) * k * 0.5f;
        for (b2DistanceJoint* j : strap->joints) {
            if (!j) {
                continue;
            }
            b2Vec2 a = j->GetAnchorA();
            b2Vec2 b = j->GetAnchorB();
            _strapNode->drawSegment(Vec2(a.x * ptm, a.y * ptm), Vec2(b.x * ptm, b.y * ptm), std::max(width, 0.5f), colour);
        }
    }
}

// Flash checkJoints: a chest strap snaps above an impulse of 0.4 on its first link.
void Sleigh::checkJoints()
{
    for (int i = 0; i < 2; i++) {
        Elf& e = _elves[i];
        Strap& strap = i == 0 ? _chestStrap1 : _chestStrap2;
        if (!e.strappedIn || strap.joints.empty() || !strap.joints[0]) {
            continue;
        }
        float impulse = strap.joints[0]->GetReactionForce(s_timeStepInverse).Length() / s_timeStepInverse;
        if (impulse > _strapBreakImpulse) {
            breakStrap(e);
        }
    }
}

void Sleigh::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (!otherFixture->IsSensor()) {
        if (fixture == _frontWheelFixture) {
            _frontContacts++;
        } else if (fixture == _backWheelFixture) {
            _backContacts++;
        }
        for (Elf& e : _elves) {
            if (fixture == e.wheelFixture) {
                e.wheelContacts++;
            }
        }
    }
    Vehicle::beginContact(fixture, otherFixture, contact);
}

void Sleigh::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (!otherFixture->IsSensor()) {
        if (fixture == _frontWheelFixture) {
            _frontContacts = std::max(0, _frontContacts - 1);
        } else if (fixture == _backWheelFixture) {
            _backContacts = std::max(0, _backContacts - 1);
        }
        for (Elf& e : _elves) {
            if (fixture == e.wheelFixture) {
                e.wheelContacts = std::max(0, e.wheelContacts - 1);
            }
        }
    }
    Vehicle::endContact(fixture, otherFixture, contact);
}

// Flash contactSleighResultHandler (rear, front, bumper, base -> the sleigh), skiShape (the ski) and
// contactStemResultHandler (the three shafts).
void Sleigh::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                       const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }
    b2Fixture* key = nullptr;
    if (fixture == _rearFixture || fixture == _frontFixture || fixture == _bumperFixture ||
        (_skiSmashed && fixture == _skiFixture)) {
        key = _frontFixture;
    } else if (fixture == _skiFixture) {
        key = _skiFixture;
    } else if (fixture == _stemFixtures[0] || fixture == _stemFixtures[1] || fixture == _stemFixtures[2]) {
        key = _stemFixtures[2];
    }
    if (!key) {
        return;
    }
    auto limit = _contactImpulseDict.find(key);
    if (limit != _contactImpulseDict.end() && normalImpulse > limit->second) {
        _results[key] = std::max(_results[key], normalImpulse);
    }
}

void Sleigh::handleContactResults()
{
    std::map<b2Fixture*, float> results;
    results.swap(_results);
    if (!_sleighSmashed && results.count(_frontFixture)) {
        sleighSmash();
        return;
    }
    if (!_skiSmashed && results.count(_skiFixture)) {
        skiSmash();
    }
    if (!_stemSmashed && results.count(_stemFixtures[2])) {
        stemSmash(true);
    }
}

void Sleigh::debugFunction(int value)
{
    if (!_sleighSmashed) {
        sleighSmash();
    }
}

// ---- breaking ------------------------------------------------------------------------------------

// Flash skiSmash: the rollers go, the ski breaks into three; the sleigh keeps a lower base.
void Sleigh::skiSmash()
{
    _skiSmashed = true;
    b2World* world = getWorld();
    stopLoop(_skiLoop, 0.0f);
    for (int i = 0; i < 4; i++) {
        if (_rollers[i]) {
            _floating.erase(std::remove(_floating.begin(), _floating.end(), _rollers[i]), _floating.end());
            world->DestroyBody(_rollers[i]);
            _rollers[i] = nullptr;
            _rollerJoints[i] = nullptr;
        }
    }
    removeBeginContact(_frontWheelFixture);
    removeBeginContact(_backWheelFixture);
    removeEndContact(_frontWheelFixture);
    removeEndContact(_backWheelFixture);
    _frontWheelFixture = _backWheelFixture = nullptr;
    _wheelJoints.clear();
    _wheelJointSpeedDict.clear();
    _frontContacts = _backContacts = 0;

    if (_sleighBody) {
        b2BodyDef def;
        def.type = b2_dynamicBody;
        def.position = _sleighBody->GetPosition();
        def.angle = _sleighBody->GetAngle();
        b2FixtureDef piece;
        piece.density = 3.0f;
        piece.friction = 0.3f;
        piece.restitution = 0.3f;
        piece.filter = _zeroFilter;
        for (int i = 1; i <= 3; i++) {
            std::vector<b2Vec2> vertices = guidePolygon("ski_" + patch::to_string(i) + "_", 6);
            b2Body* body = world->CreateBody(&def);
            b2PolygonShape polygon;
            polygon.Set(vertices.data(), (int)vertices.size());
            piece.shape = &polygon;
            body->CreateFixture(&piece);
            body->ResetMassData();
            body->SetAngularVelocity(_sleighBody->GetAngularVelocity());
            body->SetLinearVelocity(_sleighBody->GetLinearVelocityFromLocalPoint(body->GetLocalCenter()));
            paintBody(body, "ski" + patch::to_string(i), true, 8);
            _floating.push_back(body);
        }
        _skiSprite->setVisible(false);
        removePostSolve(_skiFixture);
        fixtureWillBeDestroyed(_skiFixture);
        _contactImpulseDict.erase(_skiFixture);
        _sleighBody->DestroyFixture(_skiFixture);
        b2FixtureDef base;
        base.density = 3.0f;
        base.friction = 0.3f;
        base.restitution = 0.3f;
        base.filter = _defaultFilter;
        if (_santaEjected) {
            base.filter.groupIndex = 0;
        }
        _skiFixture = addPolygon(_sleighBody, base, "newbase_", 5);
        _contactAddSounds[_skiFixture] = "SleighImpact1";
        addToPostSolve(_skiFixture);
        addToBeginContact(_skiFixture);
        _sleighBody->ResetMassData();
    }
    if (gameplay() && _sleighBody) {
        createBodySound("SkiSmash", _sleighBody, 1.0f, false);
    }
}

// Flash stemSmash: the shafts come off in one piece; the chest straps now pull on it.
void Sleigh::stemSmash(bool sound)
{
    _stemSmashed = true;
    if (!_sleighBody) {
        return;
    }
    b2World* world = getWorld();
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = _sleighBody->GetPosition();
    def.angle = _sleighBody->GetAngle();
    b2Body* stem = world->CreateBody(&def);
    b2FixtureDef piece;
    piece.density = 3.0f;
    piece.friction = 0.3f;
    piece.restitution = 0.3f;
    piece.filter = _zeroFilter;
    for (int i = 1; i <= 3; i++) {
        addPolygon(stem, piece, "stem" + patch::to_string(i) + "_", 4);
    }
    stem->ResetMassData();
    stem->SetAngularVelocity(_sleighBody->GetAngularVelocity());
    stem->SetLinearVelocity(_sleighBody->GetLinearVelocityFromLocalPoint(stem->GetLocalCenter()));
    paintBody(stem, "stem", true, 8);
    _floating.push_back(stem);

    _stemSprite->setVisible(false);
    for (b2Fixture*& f : _stemFixtures) {
        removePostSolve(f);
        removeBeginContact(f);
        fixtureWillBeDestroyed(f);
        _contactImpulseDict.erase(f);
        _sleighBody->DestroyFixture(f);
        f = nullptr;
    }
    _sleighBody->ResetMassData();
    // Re-hang the chest straps on the loose shafts.
    for (int i = 0; i < 2; i++) {
        Strap& strap = i == 0 ? _chestStrap1 : _chestStrap2;
        if (!_elves[i].strappedIn || strap.bodies.empty() || strap.bodies[0] != _sleighBody) {
            continue;
        }
        replaceStrapBody(strap, 0, stem, _sleighBody->GetWorldPoint(strap.locals[0]));
    }
    if (sound && gameplay()) {
        createBodySound("StemSnap", stem, 1.0f, false);
    }
}

// Flash sleighSmash: shards, Santa thrown out, the sleigh into seven pieces (with the ski and the
// shafts), the straps cut, both elves let go.
void Sleigh::sleighSmash()
{
    _sleighSmashed = true;
    _vehicleSmashed = true;
    b2World* world = getWorld();
    b2Vec2 centre = _sleighBody->GetWorldCenter();
    std::vector<std::string> shards;
    for (int i = 1; i <= 8; i++) {
        shards.push_back(_name + "_sleighShard_" + patch::to_string(i) + ".png");
    }
    // createPointBurst("sleighShards", ..., 100, 30, 20): Flash bursts at the sleigh's local centre
    // (a slip that puts the shards where the level started); here at the sleigh.
    if (restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession())) {
        particles->burst(shards, centre, b2Vec2_zero, 100.0f, 30.0f, 20);
    }
    if (_santa && !_santaEjected) {
        ejectCharacter(_santa);
    }
    stopFlight();
    if (!_skiSmashed) {
        skiSmash();
    }
    if (!_stemSmashed) {
        stemSmash(false);
    }
    for (Elf& e : _elves) {
        if (e.strappedIn) {
            breakStrap(e);
        }
    }

    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = _sleighBody->GetPosition();
    def.angle = _sleighBody->GetAngle();
    b2FixtureDef piece;
    piece.density = 3.0f;
    piece.friction = 0.3f;
    piece.restitution = 0.3f;
    piece.filter = _zeroFilter;
    b2Body* first = nullptr;
    for (int i = 1; i <= 7; i++) {
        std::vector<b2Vec2> vertices = guidePolygon("broken_" + patch::to_string(i) + "_", 6);
        if (vertices.size() < 3) {
            continue;
        }
        b2Body* body = world->CreateBody(&def);
        b2PolygonShape polygon;
        polygon.Set(vertices.data(), (int)vertices.size());
        piece.shape = &polygon;
        body->CreateFixture(&piece);
        body->ResetMassData();
        body->SetAngularVelocity(_sleighBody->GetAngularVelocity());
        body->SetLinearVelocity(_sleighBody->GetLinearVelocityFromLocalPoint(body->GetLocalCenter()));
        paintBody(body, "sleigh" + patch::to_string(i), true, 9);
        first = first ? first : body;
    }

    for (b2Fixture* f = _sleighBody->GetFixtureList(); f; f = f->GetNext()) {
        removePostSolve(f);
        removeBeginContact(f);
        fixtureWillBeDestroyed(f);
        _contactImpulseDict.erase(f);
    }
    stopSoundsForBody(_sleighBody);
    for (Elf& e : _elves) {
        // the elves' stems hang on the sleigh: let them go with it
        if (e.character && !e.ejected) {
            ejectCharacter(e.character);
        }
    }
    _floating.erase(std::remove(_floating.begin(), _floating.end(), _sleighBody), _floating.end());
    world->DestroyBody(_sleighBody);
    _sleighBody = nullptr;
    _reinsJoint = nullptr;
    _rearFixture = _frontFixture = _bumperFixture = _skiFixture = _backFixture = _seatFixture = nullptr;
    for (Sprite* s : {_sleighSprite, _stemSprite, _skiSprite}) {
        s->setVisible(false);
    }
    if (gameplay() && first) {
        createBodySound("SleighSmash", first, 1.0f, false);
    }
}

// ---- QOL (PC addition): re-grab vehicle (src/game/vehicles/Vehicle.h) ---------------------------
// Only Santa gets back on; an elf, once off, stays off.

b2Body* Sleigh::qolFrameBody()
{
    return _sleighBody;
}

// checkStateOfCharacter: Santa is off once both hands and both feet have let go (a smashed foot
// keeps holding, handleInjury).
bool Sleigh::qolCanRemount(CharacterB2D* character)
{
    return character == _santa && !_sleighSmashed && _sleighBody &&
           !(character->qolLostLowerArm(1) && character->qolLostLowerArm(2) &&
             character->qolLostLowerLeg(1, false) && character->qolLostLowerLeg(2, false));
}

// santaEject: _santaEjected / _ejected, the reins gone (attachSanta), the sleigh's group -1
// shapes and the third roller in group 0 (qolRestoreFilters), the roller motors off (the drive
// buttons switch them on), the flight stopped. He goes back to the front of _characters.
void Sleigh::qolRemount(CharacterB2D* character)
{
    _santaEjected = false;
    _ejected = false;
    qolRestoreFilters(character);
    qolMount(character, [this, character]() {
        _characters.insert(_characters.begin(), character);
        attachSanta(character);
    });
    qolReplayInjuries(character);
}
