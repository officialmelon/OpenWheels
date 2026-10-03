#include "Spikes.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <string>

#include "BurstEmitter.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "Session.h"
#include "Sound.h"

USING_NS_CC;

// @00630d34 (D2), @00630dac (D0)
Spikes::~Spikes()
{
}

// @005d09ec
Spikes* Spikes::create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    Spikes* spikes = new (std::nothrow) Spikes();
    if (spikes) {
        if (spikes->init(element, groupBody, groupOffset)) {
            spikes->autorelease();
        } else {
            delete spikes;
            spikes = nullptr;
        }
    }
    return spikes;
}

// @0062ec00
bool Spikes::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    bool sleeping = false;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &angle);
    element->boolAttribute("p3", &_fixed);
    element->intAttribute("p4", &_numSpikes);
    element->boolAttribute("p5", &sleeping);
    _soundCounter = 0;
    _angle = 0.0f;
    getLevel()->convertPositionAndRotationData(&x, &y, &angle);

    float ptm = getPtm();
    _spikeYOffset = ptm * 0.16f;
    _mc = Node::create();
    getLevelItemsNode()->addChild(_mc);
    _spikeWidth = 0.24f * ptm;
    _spikeHeight = 1.12f * ptm;
    _halfWidth = (float)_numSpikes * _spikeWidth * 0.5f;
    for (unsigned int i = 0; i < (unsigned int)_numSpikes; i++) {
        _spikeBloodCount.push_back(0);
    }
    _bloodOffset = b2Vec2(0.0f, 0.0f);
    if (groupBody) {
        _body = groupBody;
    }

    createBody(b2Vec2(groupOffset.x + x, groupOffset.y + y), _halfWidth / ptm, angle * -0.0174532924f, sleeping);
    _mc->setPosition(Vec2(ptm * x, ptm * y));
    _mc->setRotation(angle);
    setupMC();

    addToBeginContact(_sensor);
    addToEndContact(_sensor);
    getLevel()->addToActions(this);

    // Wake the body (a velocity kick that is cleared again right away).
    _body->SetLinearVelocity(b2Vec2(0.0f, -1.0f));
    _body->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
    return true;
}

// @0062f148
void Spikes::createBody(b2Vec2 position, float halfWidth, float angle, bool sleeping)
{
    b2PolygonShape shape;
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 0.25f;
    fixtureDef.isSensor = true;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;

    if (!_fixed) {
        if (!_body) {
            // Own body: sensor strip on top, solid base below.
            b2BodyDef bodyDef;
            bodyDef.type = b2_dynamicBody;
            bodyDef.position = position;
            bodyDef.angle = angle;
            bodyDef.awake = !sleeping;
            bodyDef.userData = _mc;
            shape.SetAsBox(halfWidth, 0.4f, b2Vec2(0.0f, 0.0f), 0.0f);
            fixtureDef.shape = &shape;
            _body = getWorld()->CreateBody(&bodyDef);
            b2Fixture* sensor = _body->CreateFixture(&fixtureDef);
            fixtureDef.isSensor = false;
            _stabbingBody = _body;
            _sensor = sensor;
            fixtureDef.density = 3.0f;
            shape.SetAsBox(halfWidth, 0.16f, b2Vec2(0.0f, -0.56f), 0.0f);
            fixtureDef.shape = &shape;
            _body->CreateFixture(&fixtureDef);
            _body->ResetMassData();
            getLevel()->addToPaintBody(_body);
        } else {
            // On the group body.
            float s = sinf(angle);
            float c = cosf(angle);
            _angle = angle;
            shape.SetAsBox(halfWidth, 0.4f, position, angle);
            fixtureDef.shape = &shape;
            b2Fixture* sensor = _body->CreateFixture(&fixtureDef);
            b2Vec2 baseCenter(position.x - s * -0.56f, position.y + c * -0.56f);
            _stabbingBody = _body;
            _sensor = sensor;
            shape.SetAsBox(halfWidth, 0.16f, baseCenter, angle);
            fixtureDef.isSensor = false;
            fixtureDef.density = 3.0f;
            fixtureDef.shape = &shape;
            _body->CreateFixture(&fixtureDef);
            _body->ResetMassData();
        }
    } else {
        // Fixed: on the level body.
        float s = sinf(angle);
        float c = cosf(angle);
        _angle = angle;
        shape.SetAsBox(halfWidth, 0.4f, position, angle);
        fixtureDef.density = 0.0f;
        fixtureDef.shape = &shape;
        _sensor = getLevelBody()->CreateFixture(&fixtureDef);
        _stabbingBody = getLevelBody();
        fixtureDef.isSensor = false;
        shape.SetAsBox(halfWidth, 0.16f, b2Vec2(position.x - s * -0.56f, position.y + c * -0.56f), angle);
        getLevelBody()->CreateFixture(&fixtureDef);
        _body = getLevelBody();
    }
}

// @0062f494
void Spikes::setupMC()
{
    float x = _spikeWidth / 2 - _halfWidth;
    for (unsigned int i = 0; i < (unsigned int)_numSpikes; i++) {
        Sprite* spike = Sprite::createWithSpriteFrameName("spike.png");
        spike->setPosition(Vec2(x, -_spikeYOffset));
        spike->setScaleX(1.08f);
        _mc->addChild(spike);
        x += _spikeWidth;
    }
}

// @0062f604
void Spikes::dealloc()
{
}

// @0062f608
void Spikes::stopInteractivity()
{
    _bodiesToAdd.clear();
    _bodiesToRemove.clear();
    _bjMap.clear();
    removeBeginContact(_sensor);
    removeEndContact(_sensor);
    getSession()->getDestructionListener()->removeListeners(this);
}

// @0062f674
void Spikes::removeSprites()
{
    _mc->removeFromParentAndCleanup(false);
}

// @0062f688
void Spikes::paintWithOffsetPoints(Vec2 offset, float angleDegrees)
{
    _mc->setPosition(offset);
    _mc->setRotation(angleDegrees);
}

// @0062f704 (also inlined into paintBloodAtPos)
void Spikes::positionBlood(Sprite* blood, unsigned int index)
{
    blood->setAnchorPoint(Vec2(0.5f, 1.0f));
    blood->setPosition(Vec2(index * _spikeWidth - _halfWidth + 1.5f, _spikeHeight / 2 - _spikeYOffset));
    blood->setOpacity(_mc->getOpacity());
}

// @0062f7e8 (also inlined into paintBloodAtPos)
void Spikes::removeExistingBloodAtIndex(unsigned int index)
{
    Node* blood = _mc->getChildByTag(index);
    if (blood) {
        _mc->removeChild(blood, false);
    }
}

// @0062f838
void Spikes::paintBloodAtPos(b2Vec2 worldPosition)
{
    // Spike index under the impact point (in _mc's frame), then blood on it and its neighbours.
    Vec2 position(worldPosition.x * getPtm(), worldPosition.y * getPtm());
    position = _mc->getParent()->convertToWorldSpace(position);
    position = _mc->convertToNodeSpace(position);
    float fraction = fmaxf(fminf((position.x + _halfWidth) / (_halfWidth + _halfWidth), 1.0f), 0.0f);
    unsigned int index = roundf(fraction * (float)(_numSpikes - 1));

    // Three different blood frames for the three spikes.
    unsigned int frame1 = roundf(CCRANDOM_0_1() * 4);
    unsigned int frame2;
    do {
        frame2 = roundf(CCRANDOM_0_1() * 4);
    } while (frame1 == frame2);
    unsigned int frame3;
    do {
        frame3 = roundf(CCRANDOM_0_1() * 4);
    } while (frame2 == frame3);

    Sprite* blood = nullptr;
    if (_spikeBloodCount[index] == 0) {
        _spikeBloodCount[index] = 1;
        blood = Sprite::createWithSpriteFrameName("spike_blood_1_" + patch::to_string(frame1) + ".png");
        blood->setScaleX(1.15f);
        positionBlood(blood, index);
        removeExistingBloodAtIndex(index);
        _mc->addChild(blood);
        blood->setTag(index);
    }
    if (index != 0) {
        unsigned int previous = index - 1;
        if (_spikeBloodCount[previous] == 0) {
            _spikeBloodCount[index]++;  // (sic) marks the centre spike, not the left one
            blood = Sprite::createWithSpriteFrameName("spike_blood_1_" + patch::to_string(frame2) + ".png");
            blood->setScaleX(1.15f);
            positionBlood(blood, previous);
            removeExistingBloodAtIndex(previous);
            _mc->addChild(blood);
            blood->setTag(previous);
        }
    }
    if ((unsigned int)(_numSpikes - 1) != index) {
        unsigned int next = index + 1;
        if (_spikeBloodCount[next] == 0) {
            _spikeBloodCount[next] = 1;
            blood = Sprite::createWithSpriteFrameName("spike_blood_1_" + patch::to_string(frame3) + ".png");
            blood->setScaleX(1.15f);
            positionBlood(blood, next);
            removeExistingBloodAtIndex(next);
            _mc->addChild(blood);
            blood->setTag(next);
        }
    }
    if (blood) {
        blood->setOpacity(_mc->getOpacity());
    }
}

// @0062ffb8
void Spikes::soundStopped()
{
    _soundCounter--;
}

// @0062ffc8
b2Body* Spikes::getJointBody(b2Vec2 point)
{
    return _body;
}

// @0062ffd0
void Spikes::setOpacity(float opacity)
{
    _mc->setOpacity(opacity * 255.0f);
}

// @0062fff0
float Spikes::opacity()
{
    return _mc->getOpacity() == 255 ? 1.0f : 0.0f;
}

// @00630024
void Spikes::jointWillBeDestroyed(b2Joint* joint)
{
    b2Body* body = joint->GetBodyB();
    _bjMap.erase(body);

    auto it = std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body);
    if (it != _bodiesToAdd.end()) {
        _bodiesToAdd.erase(it);
    }
    it = std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body);
    if (it != _bodiesToRemove.end()) {
        _bodiesToRemove.erase(it);
    }
}

// @00630174
void Spikes::actions()
{
    for (unsigned int i = 0; i < _bodiesToAdd.size(); i++) {
        createPrisJoint(_bodiesToAdd[i]);
    }
    _bodiesToAdd.clear();
    for (unsigned int i = 0; i < _bodiesToRemove.size(); i++) {
        removeJoint(_bodiesToRemove[i]);
    }
    _bodiesToRemove.clear();
}

// @006301f8
void Spikes::createPrisJoint(b2Body* body)
{
    b2Fixture* fixture = body->GetFixtureList();
    if (!fixture) {
        return;
    }
    b2Vec2 center = body->GetWorldCenter();
    LevelItem* item = static_cast<LevelItem*>(fixture->GetUserData());
    if (!body->GetUserData()) {
        return;
    }
    if (item && item->shapeImpale(fixture, true, b2Vec2(INFINITY, INFINITY), 0.0f) == 1) {
        paintBloodAtPos(center);
        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles) {
            float ptm = getPtm();
            Emitter* burst = BurstEmitter::createBloodBurst(5.0f, 15.0f, Vec2(ptm * center.x, ptm * center.y), 50);
            if (burst) {
                particles->addChild(burst);
            }
        }
    }

    // The impaled body slides along the spikes (perpendicular to the strip).
    b2Body* stabbingBody = _stabbingBody;
    b2PrismaticJointDef jointDef;
    float axisAngle = stabbingBody->GetAngle() + _angle;
    b2Vec2 axis(-sinf(axisAngle), cosf(axisAngle));
    jointDef.Initialize(stabbingBody, body, center, axis);
    jointDef.collideConnected = true;
    jointDef.enableMotor = true;
    jointDef.maxMotorForce = 30.0f;
    jointDef.motorSpeed = 0.0f;
    b2Joint* joint = getWorld()->CreateJoint(&jointDef);
    getSession()->getDestructionListener()->addJointListener(joint, this);
    _bjMap[body] = static_cast<b2PrismaticJoint*>(joint);

    if (_soundCounter < 2) {
        unsigned int variant = ceilf(CCRANDOM_0_1() * 3);
        Sound* sound = createPositionSound("ImpaleSpikes" + patch::to_string(variant), Vec2(center.x, center.y), 1.0f, false);
        if (sound) {
            _soundCounter++;
            sound->setFinishCallback([this](int&) { soundStopped(); });
        }
    }
}

// @006305e4
void Spikes::removeJoint(b2Body* body)
{
    if (_bjMap.find(body) != _bjMap.end()) {
        b2PrismaticJoint* joint = _bjMap[body];
        getSession()->getDestructionListener()->removeJointListener(this, joint);
        getWorld()->DestroyJoint(joint);
        _bjMap.erase(body);
    }
}

// @006307b4
void Spikes::setStabbingBody(b2Body* body)
{
    _stabbingBody = body;
}

// @006307bc
void Spikes::fixtureWillBeDestroyed(b2Fixture* fixture)
{
    b2Body* body = fixture->GetBody();
    _bjMap.erase(body);

    auto it = std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body);
    if (it != _bodiesToAdd.end()) {
        _bodiesToAdd.erase(it);
    }
    it = std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body);
    if (it != _bodiesToRemove.end()) {
        _bodiesToRemove.erase(it);
    }
}

// @0063090c
void Spikes::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    unsigned int material = getLevel()->getFixtureMaterial(otherFixture);
    if (material == 0xFFFFFFFF || (material & _stabbableMaterials) == 0) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    if (std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body) != _bodiesToAdd.end()) {
        return;
    }
    getSession()->getDestructionListener()->addFixtureListener(otherFixture, this);
    _bodiesToAdd.push_back(body);
}

// @00630aec
void Spikes::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    if ((getLevel()->getFixtureMaterial(otherFixture) & _stabbableMaterials) == 0) {
        return;
    }
    if (_bjMap.find(body) == _bjMap.end()) {
        if (std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body) == _bodiesToAdd.end()) {
            return;
        }
    }
    if (std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body) != _bodiesToRemove.end()) {
        return;
    }
    getSession()->getDestructionListener()->removeFixtureListener(this, otherFixture);
    _bodiesToRemove.push_back(body);
}
