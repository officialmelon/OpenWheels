#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "BladeWeapon.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "BurstEmitter.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Gameplay.h"      // EDITOR (PC addition): Gameplay::buildingTestLevel
#include "LevelSession.h"  // EDITOR (PC addition): user levels
#include "Patch.h"
#include "Session.h"
#include "Sound.h"

USING_NS_CC;

// Per blade type (index bladeType - 1): blade box and handle box, (centre x, centre y, width,
// height) in metres in the sprite's unrotated frame (dynamic initialiser _INIT_5).
static Rect s_bladeRects[12] = {
    // @00ac5f94
    Rect(0.0f, 0.52f, 0.35f, 0.43f),  Rect(0.02f, 0.18f, 0.05f, 1.03f), Rect(0.0f, 0.15f, 0.07f, 1.05f),
    Rect(0.0f, 0.11f, 0.33f, 0.16f),  Rect(0.0f, 0.11f, 0.13f, 1.1f),   Rect(0.0f, 0.43f, 0.17f, 0.27f),
    Rect(0.0f, 0.09f, 0.1f, 0.78f),   Rect(0.0f, 1.01f, 0.08f, 0.65f),  Rect(0.0f, 0.42f, 0.13f, 0.76f),
    Rect(0.0f, 0.1f, 0.06f, 2.44f),   Rect(0.0f, 0.14f, 0.05f, 0.71f),  Rect(0.0f, 0.88f, 0.29f, 0.44f),
};
static Rect s_handleRects[12] = {
    // @00ac6054
    Rect(0.0f, -0.17f, 0.05f, 1.03f), Rect(0.0f, -0.51f, 0.05f, 0.36f),  Rect(0.0f, -0.52f, 0.05f, 0.31f),
    Rect(0.04f, -0.16f, 0.07f, 0.22f), Rect(0.01f, -0.55f, 0.05f, 0.22f), Rect(0.04f, -0.09f, 0.08f, 0.86f),
    Rect(0.0f, -0.45f, 0.08f, 0.29f), Rect(0.0f, -0.32f, 0.04f, 2.02f),  Rect(0.0f, -0.02f, 0.12f, 0.12f),
    Rect(0.0f, -1.23f, 0.06f, 0.21f), Rect(0.0f, -0.36f, 0.06f, 0.28f),  Rect(0.0f, -0.23f, 0.06f, 1.76f),
};
// Blade box rotation per type, degrees (.rodata).
static const float s_bladeAngles[12] = {
    // @0041a3b8
    -89.98f, 2.99f, 0.0f, -89.97f, 0.0f, -90.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

// @00586b24 (D2), @00586b80 (D0)
BladeWeapon::~BladeWeapon()
{
}

// @00584cdc
bool BladeWeapon::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    _fleshSound = nullptr;
    _stabbableMaterials = 2;
    if (online::flashLevel()) _stabbableMaterials |= 4;  // ONLINE (PC addition): Flash stabs materials & 6 (food)
    _bladeType = 1;

    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    bool interactive = true;
    bool flipped = false;
    bool sleeping = false;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &angle);
    getLevel()->convertPositionAndRotationData(&x, &y, &angle);
    element->boolAttribute("p3", &flipped);
    element->boolAttribute("p4", &sleeping);
    element->boolAttribute("p5", &interactive);
    element->intAttribute("p6", &_bladeType);

    _mc = Sprite::createWithSpriteFrameName("blade_" + patch::to_string(_bladeType) + ".png");
    _mc->setPosition(Vec2(x * getPtm(), y * getPtm()));
    _mc->setRotation(angle);
    float flip = flipped ? -1.0f : 1.0f;
    _mc->setScaleX(flip * _mc->getScaleX());
    getLevelItemsNode()->addChild(_mc);

    if (!interactive) {
        return true;
    }

    b2PolygonShape shape;
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.75f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 0.5f;
    fixtureDef.isSensor = true;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;

    Rect bladeRect = s_bladeRects[_bladeType - 1];
    Rect handleRect = s_handleRects[_bladeType - 1];
    float bladeAngle = s_bladeAngles[_bladeType - 1];
    _weaponBody = groupBody;
    b2Vec2 bladeCenter(flip * bladeRect.origin.x, bladeRect.origin.y);
    b2Vec2 handleCenter(flip * handleRect.origin.x, handleRect.origin.y);
    _bladeAngle = flip * (bladeAngle * -0.0174532924f);

    if (!groupBody) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = b2Vec2(x, y);
        bodyDef.angle = angle * -0.0174532924f;
        bodyDef.awake = !sleeping;
        _weaponBody = getWorld()->CreateBody(&bodyDef);

        // Blade: a sensor (stabbing) and a solid copy that only collides with category 8.
        _bladeOffset = bladeCenter;
        shape.SetAsBox(bladeRect.size.width * 0.5f, bladeRect.size.height * 0.5f, bladeCenter, _bladeAngle);
        fixtureDef.shape = &shape;
        _sensorShape = _weaponBody->CreateFixture(&fixtureDef);
        fixtureDef.isSensor = false;
        fixtureDef.filter.maskBits = 0x0008;
        _solidShape = _weaponBody->CreateFixture(&fixtureDef);
        fixtureDef.filter.maskBits = 0xFFFF;

        shape.SetAsBox(handleRect.size.width * 0.5f, handleRect.size.height * 0.5f, handleCenter, 0.0f);
        fixtureDef.density = 1.0f;
        fixtureDef.shape = &shape;
        _handleShape = _weaponBody->CreateFixture(&fixtureDef);
        _weaponBody->ResetMassData();
        _weaponBody->SetUserData(_mc);
        getLevel()->addToPaintBody(_weaponBody);
    } else {
        // Part of a group: fixtures go on the group body, placed with the item's transform.
        _bloodOffset.x = groupOffset.x;
        _bloodOffset.y = groupOffset.y;
        float rotation = angle * -0.0174532924f;
        b2Vec2 position(groupOffset.x + x, groupOffset.y + y);
        AffineTransform transform = AffineTransformMakeIdentity();
        transform = AffineTransformRotate(transform, rotation);
        transform = AffineTransformTranslate(transform, bladeCenter.x, bladeCenter.y);
        _bladeOffset.x = position.x + transform.tx;
        _bladeOffset.y = position.y + transform.ty;
        _bladeAngle = rotation + _bladeAngle;
        shape.SetAsBox(bladeRect.size.width * 0.5f, bladeRect.size.height * 0.5f, _bladeOffset, _bladeAngle);
        fixtureDef.shape = &shape;
        _sensorShape = _weaponBody->CreateFixture(&fixtureDef);
        fixtureDef.isSensor = false;
        fixtureDef.filter.maskBits = 0x0008;
        _solidShape = _weaponBody->CreateFixture(&fixtureDef);
        fixtureDef.filter.maskBits = 0xFFFF;

        // The original discards the results of these two calls (no copy-back after either
        // `bl`), so the handle box sits at the item position itself, not at handleCenter.
        transform = AffineTransformMakeIdentity();
        AffineTransformRotate(transform, rotation);
        AffineTransformTranslate(transform, handleCenter.x, handleCenter.y);
        if (online::flashLevel() || LevelSession::getInstance()->isUserLevel() || Gameplay::buildingTestLevel()) {
            // ONLINE / EDITOR (PC addition): browser levels, user levels and the editor's test
            // play put the handle where it is drawn (Flash places it like the blade). With the
            // original's box at the item's centre, a grouped sword whose handle is off-centre (7
            // of the 12 types sit entirely off the drawn handle) had no solid handle to grab,
            // while its blade still hit. The campaign keeps the original's box (parity).
            transform = AffineTransformRotate(transform, rotation);
            transform = AffineTransformTranslate(transform, handleCenter.x, handleCenter.y);
        }
        b2Vec2 handlePosition(position.x + transform.tx, position.y + transform.ty);
        shape.SetAsBox(handleRect.size.width * 0.5f, handleRect.size.height * 0.5f, handlePosition, rotation);
        fixtureDef.density = 1.0f;
        fixtureDef.shape = &shape;
        _handleShape = _weaponBody->CreateFixture(&fixtureDef);
    }

    addToBeginContact(_sensorShape);
    addToEndContact(_sensorShape);
    getLevel()->addToActions(this);
    return true;
}

// @0058572c
void BladeWeapon::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    if (body->GetType() == b2_staticBody) {
        return;
    }
    unsigned int material = getLevel()->getFixtureMaterial(otherFixture);
    if (material == 0xFFFFFFFF || (material & _stabbableMaterials) == 0) {
        return;
    }
    if (std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body) != _bodiesToAdd.end()) {
        return;
    }
    LevelItem* item = static_cast<LevelItem*>(otherFixture->GetUserData());
    if (item && item->getFluidType() == 1) {
        addBlood();
    }
    getSession()->getDestructionListener()->addFixtureListener(otherFixture, this);
    _bodiesToAdd.push_back(body);
}

// @00585938
void BladeWeapon::addBlood()
{
    if (_stabCount >= 6) {
        return;
    }
    if (_stabCount++ == 0) {
        _bloodSprite = Sprite::createWithSpriteFrameName("blade_blood_1_" + patch::to_string(_bladeType) + ".png");
        _bloodSprite->setAnchorPoint(Vec2(0.0f, 0.0f));
        _mc->addChild(_bloodSprite);
    } else if (_stabCount == 5) {
        _bloodSprite->removeFromParentAndCleanup(false);
        _bloodSprite = Sprite::createWithSpriteFrameName("blade_blood_2_" + patch::to_string(_bladeType) + ".png");
        _bloodSprite->setAnchorPoint(Vec2(0.0f, 0.0f));
        _mc->addChild(_bloodSprite);
        _bloodied = true;
    }
    _bloodSprite->setOpacity(_mc->getOpacity());
}

// @00585c1c
void BladeWeapon::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    if (body->GetType() == b2_staticBody) {
        return;
    }
    unsigned int material = getLevel()->getFixtureMaterial(otherFixture);
    if (material == 0xFFFFFFFF || (material & _stabbableMaterials) == 0) {
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

// @00585e60
void BladeWeapon::fixtureWillBeDestroyed(b2Fixture* fixture)
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

// @00585fb0
b2Body* BladeWeapon::getJointBody(b2Vec2 point)
{
    return _weaponBody;
}

// @00585fb8
void BladeWeapon::actions()
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

// @0058603c
void BladeWeapon::createPrisJoint(b2Body* body)
{
    // The impaled body slides along the blade axis (motor as friction, no limits).
    b2Body* weaponBody = _weaponBody;
    b2Fixture* fixture = body->GetFixtureList();

    b2PrismaticJointDef jointDef;
    b2Vec2 anchor = weaponBody->GetWorldPoint(_bladeOffset);
    float axisAngle = weaponBody->GetAngle() + _bladeAngle + M_PI_2;
    b2Vec2 axis(cosf(axisAngle), sinf(axisAngle));
    jointDef.Initialize(weaponBody, body, anchor, axis);
    jointDef.enableLimit = false;
    jointDef.lowerTranslation = 0.0f;
    jointDef.upperTranslation = 0.0f;
    jointDef.collideConnected = true;
    jointDef.enableMotor = true;
    jointDef.maxMotorForce = 10000.0f;
    jointDef.motorSpeed = 0.0f;
    jointDef.userData = this;
    b2Joint* joint = getWorld()->CreateJoint(&jointDef);
    _bjDictionary[body] = static_cast<b2PrismaticJoint*>(joint);

    if (_previousBody == body) {
        return;
    }
    _previousBody = body;
    getSession()->getDestructionListener()->removeFixtureListener(this, fixture);
    if ((getLevel()->getFixtureMaterial(fixture) & _stabbableMaterials) == 0) {
        return;
    }
    getSession()->getDestructionListener()->addJointListener(joint, this);
    LevelItem* item = static_cast<LevelItem*>(fixture->GetUserData());
    if (!item || item->shapeImpale(fixture, true, b2Vec2(INFINITY, INFINITY), 0.0f) != 1) {
        return;
    }
    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        float ptm = getPtm();
        Emitter* blood = BurstEmitter::createBloodBurst(5.0f, 15.0f, Vec2(ptm * anchor.x, ptm * anchor.y), 20);
        if (blood) {
            particles->addChild(blood);
        }
    }
    if (_fleshSound) {
        return;
    }
    unsigned int variant = ceilf(CCRANDOM_0_1() * 3);
    _fleshSound = createBodySound("BladeFlesh" + patch::to_string(variant), _weaponBody, 1.0f, false);
    if (_fleshSound) {
        _fleshSound->setFinishCallback([this](int&) { fleshSoundStopped(); });
    }
}

// @00586494
void BladeWeapon::removeJoint(b2Body* body)
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

// @00586670
void BladeWeapon::jointWillBeDestroyed(b2Joint* joint)
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

// @005867c0
void BladeWeapon::fleshSoundStopped()
{
    _fleshSound = nullptr;
}

// @005867c8
void BladeWeapon::solidSoundStopped()
{
    _solidSound = nullptr;
}

// @005867d0 (also inlined into stopInteractivity)
void BladeWeapon::killSounds()
{
    if (_fleshSound) {
        _fleshSound->stop();
        _fleshSound = nullptr;
    }
}

// @00586804
void BladeWeapon::paintWithOffsetPoints(Vec2 offset, float angleDegrees)
{
    _mc->setPosition(offset);
    _mc->setRotation(angleDegrees);
}

// @00586880
void BladeWeapon::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (action == 1) {
        // properties: impulse x, impulse y (per unit mass), angular velocity delta.
        if (_weaponBody) {
            LevelB2D* level = getLevel();
            float impulseX = properties[0];
            float impulseY = properties[1];
            level->convertDirectionIfNecessaryBasedOnRegistration(&impulseY);
            _weaponBody->ApplyLinearImpulse(
                b2Vec2(impulseX * _weaponBody->GetMass(), impulseY * _weaponBody->GetMass()),
                _weaponBody->GetWorldCenter(), true);
            float spin = properties[2];
            level->convertDirectionIfNecessaryBasedOnRegistration(&spin);
            _weaponBody->SetAngularVelocity(_weaponBody->GetAngularVelocity() + spin);
        }
    } else if (action == 0 && _weaponBody) {
        _weaponBody->SetAwake(true);
    }
}

// @005869f0
void BladeWeapon::setOpacity(float opacity)
{
    _mc->setOpacity(opacity * 255.0f);
    if (_bloodSprite) {
        _bloodSprite->setOpacity(opacity * 255.0f);
    }
}

// @00586a50
float BladeWeapon::getOpacity()
{
    return _mc->getOpacity() == 255 ? 1.0f : 0.0f;
}

// @00586a84
void BladeWeapon::stopInteractivity()
{
    _bodiesToAdd.clear();
    _bodiesToRemove.clear();
    _bjDictionary.clear();
    removeBeginContact(_sensorShape);
    removeEndContact(_sensorShape);
    getSession()->getDestructionListener()->removeListeners(this);
    killSounds();
    LevelItem::stopInteractivity();
}

// @00586b10
void BladeWeapon::removeSprites()
{
    _mc->removeFromParentAndCleanup(false);
}
