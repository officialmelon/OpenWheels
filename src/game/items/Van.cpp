#include "Van.h"

#include <string>

#include "BurstEmitter.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"

USING_NS_CC;

// @0063d2e4 (D0; D1 is LevelItem's)
Van::~Van()
{
}

// @0063c17c
bool Van::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    float ptm = getPtm();

    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    bool sleeping = false;
    bool interactive = true;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &angle);
    element->boolAttribute("p3", &sleeping);
    element->boolAttribute("p4", &interactive);
    getLevel()->convertPositionData(&x, &y);
    getLevel()->convertRotationData(&angle);

    Vec2 position(ptm * x, ptm * y);
    _mc = Node::create();
    _mc->setPosition(position);
    _mc->setRotation(angle);
    getLevelItemsNode()->addChild(_mc);

    // The body sprite is two mirrored halves.
    Sprite* leftHalf = Sprite::createWithSpriteFrameName("van.png");
    leftHalf->setPosition(Vec2(-(ptm * 0.648f), 0.0f));
    _mc->addChild(leftHalf);
    Sprite* rightHalf = Sprite::createWithSpriteFrameName("van.png");
    rightHalf->setScaleX(-1.0f);
    rightHalf->setPosition(Vec2(ptm * 0.648f, 0.0f));
    _mc->addChild(rightHalf);

    _wheelSize = b2Vec2(0.16f, 0.36f);
    _wheelPosPixels = Vec2(0.928f, 0.768f) * ptm;

    // Wheels: in the level items node next to the van when interactive, children of _mc otherwise.
    // (The odd "0 - y" / "+ 0" terms are the original's arithmetic.)
    Sprite* wheel1 = Sprite::createWithSpriteFrameName("van_wheel.png");
    Node* wheel1Parent;
    if (interactive) {
        wheel1->setPosition(position - _wheelPosPixels);
        wheel1Parent = getLevelItemsNode();
    } else {
        wheel1->setPosition(Vec2(-_wheelPosPixels.x, 0.0f - _wheelPosPixels.y));
        wheel1Parent = _mc;
    }
    wheel1Parent->addChild(wheel1, -1);

    Sprite* wheel2 = Sprite::createWithSpriteFrameName("van_wheel.png");
    Node* wheel2Parent;
    if (interactive) {
        wheel2->setPosition(Vec2(position.x + _wheelPosPixels.x, position.y - _wheelPosPixels.y + 0.0f));
        wheel2Parent = getLevelItemsNode();
    } else {
        wheel2->setPosition(Vec2(_wheelPosPixels.x, 0.0f - _wheelPosPixels.y));
        wheel2Parent = _mc;
    }
    wheel2Parent->addChild(wheel2, -1);

    Sprite* label = Sprite::createWithSpriteFrameName("van_label.png");
    label->setPosition(Vec2(0.0f, 0.0f + ptm * -0.832f));
    _mc->addChild(label);

    if (interactive) {
        createBodyAt(b2Vec2(x, y), angle, sleeping);
        createJoints();

        _body->SetUserData(_mc);
        _tire1->GetBody()->SetUserData(wheel1);
        _tire2->GetBody()->SetUserData(wheel2);
        getLevel()->addToPaintBody(_body);
        getLevel()->addToPaintBody(_tire1->GetBody());
        getLevel()->addToPaintBody(_tire2->GetBody());

        addToPostSolve(_fixture);
        addToBeginContact(_fixture);
        addToBeginContact(_tire1);
        addToBeginContact(_tire2);
        _contactAddSounds[_fixture] = "VanHit";
        _contactAddSounds[_tire1] = "CarTire1";
        _contactAddSounds[_tire2] = "CarTire1";

        getLevel()->addToPaintItem(this);
        getLevel()->addToActions(this);
    }
    return true;
}

// @0063cab0
void Van::createBodyAt(b2Vec2 position, float angleDegrees, bool sleeping)
{
    b2World* world = getWorld();

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angleDegrees * -0.0174532924f;
    bodyDef.awake = !sleeping;

    b2PolygonShape shape;
    shape.SetAsBox(1.056f, 0.928f);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = 3.0f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 10.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;

    _body = world->CreateBody(&bodyDef);
    _fixture = _body->CreateFixture(&fixtureDef);

    // Tires: separate bodies (same angle / sleep state), joined by createJoints().
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.3f;

    bodyDef.position = _body->GetWorldPoint(b2Vec2(-0.928f, -0.768f));
    b2Body* tireBody = world->CreateBody(&bodyDef);
    shape.SetAsBox(_wheelSize.x, _wheelSize.y);
    fixtureDef.shape = &shape;
    _tire1 = tireBody->CreateFixture(&fixtureDef);

    bodyDef.position = _body->GetWorldPoint(b2Vec2(0.928f, -0.768f));
    tireBody = world->CreateBody(&bodyDef);
    fixtureDef.shape = &shape;
    _tire2 = tireBody->CreateFixture(&fixtureDef);
}

// @0063ccc0 (also inlined into init)
void Van::createJoints()
{
    // Wheels are locked (limit [0, 0]); the motor torque is set but the motor is never enabled.
    b2RevoluteJointDef jointDef;
    jointDef.enableLimit = true;
    jointDef.lowerAngle = 0.0f;
    jointDef.upperAngle = 0.0f;
    jointDef.motorSpeed = 0.0f;
    jointDef.maxMotorTorque = 10000.0f;

    jointDef.Initialize(_body, _tire1->GetBody(), _tire1->GetBody()->GetWorldCenter());
    b2World* world = getWorld();
    _leftJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    jointDef.Initialize(_body, _tire2->GetBody(), _tire2->GetBody()->GetWorldCenter());
    _rightJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
}

// @0063cdac
void Van::actions()
{
    handleContactAdds();
}

// @0063cdb8
void Van::singleAction()
{
    // Replace the body box by the lower, squashed one (the old _fixture pointer is left dangling,
    // as in the original).
    _body->DestroyFixture(_fixture);

    b2PolygonShape shape;
    shape.SetAsBox(1.056f, 0.8f);
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 10.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;
    _body->CreateFixture(&fixtureDef);

    _mc->removeFromParentAndCleanup(false);
    Sprite* smashed = Sprite::createWithSpriteFrameName("van_smashed.png");
    _body->SetUserData(smashed);
    getLevelItemsNode()->addChild(smashed);

    getWorld()->DestroyJoint(_rightJoint);
    getWorld()->DestroyJoint(_leftJoint);

    createBodySound("VanSmash", _body, 1.0f, false);

    Node* particles = getSession()->getParticlesForeground();
    if (particles) {
        BurstEmitter* glass = BurstEmitter::createVanGlassBurst(_body, Vec2::ZERO);
        if (glass) {
            particles->addChild(glass);
        }
    }
}

// @0063cff4
void Van::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (action == 1) {
        // properties: impulse x, impulse y (per unit mass), angular velocity delta.
        LevelB2D* level = getLevel();
        float impulseX = properties[0];
        float impulseY = properties[1];
        level->convertDirectionIfNecessaryBasedOnRegistration(&impulseY);
        _body->ApplyLinearImpulse(b2Vec2(impulseX * _body->GetMass(), impulseY * _body->GetMass()),
                                  _body->GetWorldCenter(), true);
        float spin = properties[2];
        level->convertDirectionIfNecessaryBasedOnRegistration(&spin);
        _body->SetAngularVelocity(_body->GetAngularVelocity() + spin);
    } else if (action == 0) {
        _body->SetAwake(true);
    }
}

// @0063d158
void Van::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    contactSoundHandler(fixture, otherFixture, contact, nullptr);
}

// @0063d168
b2Body* Van::getJointBody(b2Vec2 point)
{
    return _body;
}

// @0063d170
std::vector<b2Body*> Van::getBodyList()
{
    std::vector<b2Body*> bodies;
    if (_body) {
        bodies.push_back(_body);
    }
    return bodies;
}

// @0063d1b4
void Van::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                    const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        maxImpulse = b2Max(maxImpulse, impulse->normalImpulses[1]);
    }
    if (maxImpulse > 150.0f) {
        // erase(find()) - the original does not check for end().
        _contactAddSounds.erase(_contactAddSounds.find(_fixture));
        removeBeginContact(_fixture);
        removePostSolve(_fixture);
        getLevel()->addToSingleActions(this);
    }
}
