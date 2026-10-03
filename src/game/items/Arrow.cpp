#include "Arrow.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <string>

#include "ArrowGun.h"
#include "BurstEmitter.h"
#include "ContactListener.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "Globals.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Sound.h"
#include "StageCamera.h"

USING_NS_CC;

// @0057d314
Arrow::Arrow()
    : _arrowMC(nullptr),
      _piece1(nullptr),
      _piece2(nullptr),
      _piece1Speed(Vec2::ZERO),
      _piece2Speed(Vec2::ZERO),
      _piece1Rotation(0.0f),
      _piece2Rotation(0.0f),
      _arrowGun(nullptr),
      _arrowBody(nullptr),
      _sensorShape(nullptr),
      _solidShape(nullptr),
      _previousBody(nullptr),
      _fleshSound(nullptr),
      _solidSound(nullptr)
{
}

// @0057d380 (D1), @0057d470 (D0)
Arrow::~Arrow()
{
    killSounds();
    getSession()->getDestructionListener()->removeListeners(this);
}

// @0057d424
void Arrow::killSounds()
{
    if (_fleshSound) {
        _fleshSound->stop();
        _fleshSound = nullptr;
    }
    if (_solidSound) {
        _solidSound->stop();
        _solidSound = nullptr;
    }
}

// @0057d494
Arrow* Arrow::createWithPos(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder)
{
    Arrow* arrow = new (std::nothrow) Arrow();
    if (arrow) {
        arrow->init(position, angle, velocity, zOrder);
        arrow->autorelease();
    }
    return arrow;
}

// @0057d544
bool Arrow::init(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder)
{
    _arrowGun = nullptr;
    _arrowMC = Sprite::createWithSpriteFrameName("arrow_1.png");
    _arrowMC->setPosition(Vec2(position.x * getPtm(), position.y * getPtm()));
    _arrowMC->setRotation(CC_RADIANS_TO_DEGREES(angle));
    _arrowMC->setVisible(false);  // shown by the first singleAction()
    getLevelItemsNode()->addChild(_arrowMC, zOrder - 1);

    createBody(position, angle, velocity);
    _arrowBody->SetUserData(_arrowMC);

    addToBeginContact(_sensorShape);
    addToEndContact(_sensorShape);
    getLevel()->addToActions(this);
    getLevel()->addToSingleActions(this);
    getLevel()->addToPaintBody(_arrowBody);
    return true;
}

// @0057d730
void Arrow::setArrowGun(ArrowGun* arrowGun)
{
    _arrowGun = arrowGun;
}

// @0057d738
void Arrow::createBody(b2Vec2 position, float angle, b2Vec2 velocity)
{
    const float ptmRatio = globals::flash::ptmRatio;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;

    // Tip sensor in front of the shaft.
    b2PolygonShape shape;
    shape.SetAsBox(10.0f / ptmRatio, 1.0f / ptmRatio, b2Vec2(16.5f / ptmRatio, 0.0f), 0.0f);

    _arrowBody = getWorld()->CreateBody(&bodyDef);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = 3.0f;
    fixtureDef.isSensor = true;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = -21;
    _sensorShape = _arrowBody->CreateFixture(&fixtureDef);

    // Solid shaft.
    shape.SetAsBox(16.5f / ptmRatio, 1.0f / ptmRatio, b2Vec2(-10.0f / ptmRatio, 0.0f), 0.0f);
    fixtureDef.isSensor = false;
    fixtureDef.filter.groupIndex = 0;
    _solidShape = _arrowBody->CreateFixture(&fixtureDef);

    _arrowBody->ResetMassData();
    _arrowBody->SetLinearVelocity(velocity);
}

// @0057d91c
void Arrow::fixtureWillBeDestroyed(b2Fixture* fixture)
{
    b2Body* body = fixture->GetBody();
    _bjDictionary.erase(body);

    auto it = std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body);
    if (it != _bodiesToAdd.end()) {
        _bodiesToAdd.erase(it);
    }
    it = std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body);
    if (it != _bodiesToRemove.end()) {
        _bodiesToRemove.erase(it);
    }
}

// @0057da6c
void Arrow::jointWillBeDestroyed(b2Joint* joint)
{
    b2Body* body = joint->GetBodyB();
    _bjDictionary.erase(body);

    auto it = std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body);
    if (it != _bodiesToAdd.end()) {
        _bodiesToAdd.erase(it);
    }
    it = std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body);
    if (it != _bodiesToRemove.end()) {
        _bodiesToRemove.erase(it);
    }
}

// @0057dbbc
void Arrow::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (!_arrowBody) {
        return;
    }
    if (otherFixture->IsSensor()) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    if (body->GetType() == b2_staticBody) {
        return;
    }
    int material = getLevel()->getFixtureMaterial(otherFixture);
    if (material == -1) {
        return;
    }
    if ((material & 2) == 0) {
        return;
    }
    if (std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body) != _bodiesToAdd.end()) {
        return;
    }
    getSession()->getDestructionListener()->addFixtureListener(otherFixture, this);
    _bodiesToAdd.push_back(body);
}

// @0057dda4
void Arrow::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (!_arrowBody) {
        return;
    }
    if (otherFixture->IsSensor()) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    int material = getLevel()->getFixtureMaterial(otherFixture);
    // Static bodies pass even when they are not stabbable (as in the original).
    if ((material & 2) == 0 && body->GetType() != b2_staticBody) {
        return;
    }
    if (_bjDictionary.find(body) == _bjDictionary.end()) {
        if (std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body) == _bodiesToAdd.end()) {
            return;
        }
    }
    if (std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body) != _bodiesToRemove.end()) {
        return;
    }
    _bodiesToRemove.push_back(body);
}

// @0057dfe0
void Arrow::remoteBreak()
{
    if (_arrowBody) {
        arrowDie();
        getLevel()->removeFromPaintBody(_arrowBody);
        createPiecesWithVelocity(_arrowBody->GetLinearVelocity());
        getWorld()->DestroyBody(_arrowBody);
        _arrowBody = nullptr;
    }
}

// @0057e03c
void Arrow::arrowDie()
{
    killSounds();
    _bodiesToAdd.clear();
    _bodiesToRemove.clear();
    _bjDictionary.clear();

    ContactListener* contactListener = getSession()->getContactListener();
    contactListener->removeBeginContactListener(_sensorShape, this);
    contactListener->removeEndContactListener(_sensorShape, this);
    contactListener->removePostSolveListener(_solidShape, this);
}

// @0057e0e8
void Arrow::createPiecesWithVelocity(b2Vec2 velocity)
{
    _piece1Speed = Vec2(velocity.x * getPtm() * s_timeStep + CCRANDOM_MINUS1_1(),
                        velocity.y * getPtm() * s_timeStep + CCRANDOM_MINUS1_1());
    _piece2Speed = Vec2(velocity.x * getPtm() * s_timeStep + CCRANDOM_MINUS1_1(),
                        velocity.y * getPtm() * s_timeStep + CCRANDOM_MINUS1_1());
    _piece1Rotation = CCRANDOM_MINUS1_1() * 10.0f;
    _piece2Rotation = CCRANDOM_MINUS1_1() * 10.0f;

    // Split the arrow's texture rect in two halves.
    Texture2D* texture = _arrowMC->getTexture();
    Rect rect = _arrowMC->getTextureRect();
    Rect rect1;
    Rect rect2;
    bool rotated = _arrowMC->isTextureRectRotated();
    if (!rotated) {
        rect.size.width *= 0.5f;
        rect1 = Rect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        rect.origin.x += rect.size.width;
    } else {
        rect.size.height *= 0.5f;
        rect1 = Rect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        rect.origin.y -= rect.size.height;
    }
    rect2 = Rect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

    _piece1 = Sprite::createWithTexture(texture);
    _piece2 = Sprite::createWithTexture(texture);
    _piece1->setTextureRect(rect1, rotated, rect1.size);
    _piece2->setTextureRect(rect2, rotated, rect2.size);

    // The rotated transform is computed and then ignored (original behaviour): the offset is taken
    // from the identity transform, i.e. (width / 4, 0).
    AffineTransform transform = AffineTransformMakeIdentity();
    AffineTransformRotate(transform, _arrowBody->GetAngle());
    float offset = _arrowMC->getContentSize().width * 0.25f;
    float offsetX = offset * transform.a + transform.tx;
    float offsetY = offset * transform.b + transform.ty;

    _piece1->setPosition(
        Vec2(_arrowMC->getPosition().x - offsetX, _arrowMC->getPosition().y - offsetY));
    _piece2->setPosition(
        Vec2(offsetX + _arrowMC->getPosition().x, offsetY + _arrowMC->getPosition().y));

    float rotation = _arrowMC->getRotation();
    _piece1->setRotation(rotation);
    _piece2->setRotation(rotation);

    Node* parent = _arrowMC->getParent();
    parent->addChild(_piece1, _arrowMC->getLocalZOrder());
    parent->addChild(_piece2, _arrowMC->getLocalZOrder());
    _arrowMC->removeFromParentAndCleanup(false);
}

// @0057e500
void Arrow::actions()
{
    if (_arrowBody) {
        for (unsigned int i = 0; i < _bodiesToAdd.size(); i++) {
            createPrisJoint(_bodiesToAdd[i]);
        }
        _bodiesToAdd.clear();
        for (unsigned int i = 0; i < _bodiesToRemove.size(); i++) {
            removeJoint(_bodiesToRemove[i]);
        }
        _bodiesToRemove.clear();
        return;
    }

    if (!_piece1 && !_piece2) {
        getLevel()->removeFromActions(this);
        getLevel()->removeFromFrameActions(this);
        return;
    }

    float gravity = s_timeStep * -10.0f;
    if (_piece1) {
        _piece1Speed.y += gravity;
        _piece1->setRotation(_piece1->getRotation() + _piece1Rotation);
        _piece1->setPosition(Vec2(_piece1->getPosition().x + _piece1Speed.x,
                                  _piece1->getPosition().y + _piece1Speed.y));
        if (_piece1->getPosition().y < getSession()->getCamera()->getYParticleLimit()) {
            _piece1->removeFromParentAndCleanup(false);
            _piece1 = nullptr;
        }
    }
    if (_piece2) {
        _piece2Speed.y += gravity;
        // Original bug kept: the second piece also spins by _piece1Rotation.
        _piece2->setRotation(_piece2->getRotation() + _piece1Rotation);
        _piece2->setPosition(Vec2(_piece2->getPosition().x + _piece2Speed.x,
                                  _piece2->getPosition().y + _piece2Speed.y));
        if (_piece2->getPosition().y < getSession()->getCamera()->getYParticleLimit()) {
            _piece2->removeFromParentAndCleanup(false);
            _piece2 = nullptr;
        }
    }
}

// @0057e7d8
void Arrow::createPrisJoint(b2Body* body)
{
    b2Fixture* fixture = body->GetFixtureList();

    b2PrismaticJointDef jointDef;
    b2Vec2 anchor = _arrowBody->GetWorldPoint(b2Vec2(13.25f / globals::flash::ptmRatio, 0.0f));
    float angle = _arrowBody->GetAngle();
    b2Vec2 axis(cosf(angle), sinf(angle));
    jointDef.Initialize(_arrowBody, body, anchor, axis);
    jointDef.lowerTranslation = 0.0f;
    jointDef.upperTranslation = 0.0f;
    jointDef.enableLimit = true;
    jointDef.collideConnected = true;
    jointDef.enableMotor = true;
    jointDef.maxMotorForce = 100000.0f;
    jointDef.motorSpeed = 0.0f;
    jointDef.userData = this;
    b2Joint* joint = getWorld()->CreateJoint(&jointDef);
    _bjDictionary[body] = static_cast<b2PrismaticJoint*>(joint);

    if (!_previousBody) {
        addToPostSolve(_solidShape);
    }
    if (_previousBody == body) {
        return;
    }
    _previousBody = body;

    getSession()->getDestructionListener()->removeFixtureListener(this, fixture);
    int material = getLevel()->getFixtureMaterial(fixture);
    if (material & 2) {
        // Flesh: keep the joint alive while the body exists, swap to a bloody arrow.
        getSession()->getDestructionListener()->addJointListener(joint, this);
        LevelItem* item = static_cast<LevelItem*>(fixture->GetUserData());
        if (!item) {
            return;
        }
        if (item->shapeImpale(fixture, true, anchor, 0.01f) != 1) {
            return;
        }

        Node* parent = _arrowMC->getParent();
        unsigned int frame = ceilf(CCRANDOM_0_1() * 5) + 1;
        Sprite* bloodyArrow =
            Sprite::createWithSpriteFrameName("arrow_" + patch::to_string(frame) + ".png");
        bloodyArrow->setPosition(_arrowMC->getPosition());
        bloodyArrow->setRotation(_arrowMC->getRotation());
        parent->addChild(bloodyArrow, _arrowMC->getLocalZOrder());
        _arrowMC->removeFromParentAndCleanup(false);
        _arrowMC = bloodyArrow;
        _arrowBody->SetUserData(bloodyArrow);

        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles) {
            float ptm = getPtm();
            Emitter* blood =
                BurstEmitter::createBloodBurst(5.0f, 15.0f, Vec2(ptm * anchor.x, ptm * anchor.y), 20);
            if (blood) {
                particles->addChild(blood);
            }
        }

        if (_fleshSound) {
            return;
        }
        unsigned int variant = ceilf(CCRANDOM_0_1() * 3);
        _fleshSound = createBodySound("ArrowFlesh" + patch::to_string(variant), _arrowBody, 1.0f, false);
        if (_fleshSound) {
            _fleshSound->setFinishCallback([this](int&) { fleshSoundStopped(); });
        }
    } else {
        if (_solidSound) {
            return;
        }
        unsigned int variant = ceilf(CCRANDOM_0_1() * 2);
        _solidSound = createPositionSound("ArrowSolid" + patch::to_string(variant),
                                          Vec2(_arrowBody->GetPosition().x, _arrowBody->GetPosition().y),
                                          1.0f, false);
        if (_solidSound) {
            _solidSound->setFinishCallback([this](int&) { solidSoundStopped(); });
        }
    }
}

// @0057ef10
void Arrow::removeJoint(b2Body* body)
{
    if (_bjDictionary.find(body) != _bjDictionary.end()) {
        b2PrismaticJoint* joint = _bjDictionary[body];
        if (body->GetType() == b2_dynamicBody) {
            getSession()->getDestructionListener()->removeJointListener(this, joint);
        }
        getWorld()->DestroyJoint(joint);
        _bjDictionary.erase(body);
    }
}

// @0057f0ec
void Arrow::singleAction()
{
    if (!_arrowMC->isVisible()) {
        _arrowMC->setVisible(true);
        return;
    }
    if (_arrowBody) {
        createPiecesWithVelocity(_arrowBody->GetLinearVelocity());
        b2Vec2 position = _arrowBody->GetPosition();
        getWorld()->DestroyBody(_arrowBody);
        _arrowBody = nullptr;

        unsigned int variant = ceilf(CCRANDOM_0_1() * 2);
        std::string soundName = "ArrowSnap" + patch::to_string(variant);
        createPositionSound(soundName, Vec2(position.x, position.y), 1.0f, false);
        if (_arrowGun) {
            _arrowGun->arrowBroken(this);
        }
    }
}

// @0057f454
void Arrow::fleshSoundStopped()
{
    _fleshSound = nullptr;
}

// @0057f45c
void Arrow::solidSoundStopped()
{
    _solidSound = nullptr;
}

// @0057f464
void Arrow::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                      const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        maxImpulse = b2Max(maxImpulse, impulse->normalImpulses[1]);
    }
    if (maxImpulse > 1.0f) {
        arrowDie();
        getLevel()->addToSingleActions(this);
        getLevel()->removeFromPaintBody(_arrowBody);
    }
}

// @0057f4d8
void Arrow::arrowResultHandler()
{
}
