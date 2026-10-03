#include "Log.h"

#include <cmath>
#include <cstdlib>

#include "BurstEmitter.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "Session.h"
#include "Sound.h"

USING_NS_CC;

// @005e75d0 (D0; the complete destructor is LevelItem's)
Log::~Log()
{
}

// @005e6420
bool Log::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    _body = nullptr;
    _sound = nullptr;
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    float ptm = getPtm();

    float y = 0.0f;
    float x = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p4", &_rotation);
    getLevel()->convertPositionAndRotationData(&x, &y, &_rotation);
    element->floatAttribute("p2", &_shapeWidthMeters);
    element->floatAttribute("p3", &_shapeHeightMeters);
    getLevel()->convertLengthData(&_shapeWidthMeters);
    getLevel()->convertLengthData(&_shapeHeightMeters);
    bool fixed = false;
    bool sleeping = false;
    element->boolAttribute("p5", &fixed);
    element->boolAttribute("p6", &sleeping);

    Node* itemsNode = getLevelItemsNode();
    _mc = Node::create();
    _mc->setPosition(Vec2(ptm * x, ptm * y));
    _mc->setRotation(_rotation);
    itemsNode->addChild(_mc);

    float width = _shapeWidthMeters;
    float height = _shapeHeightMeters;
    _bottomMC = Sprite::createWithSpriteFrameName("log_bottom.png");
    _topMC = Sprite::createWithSpriteFrameName("log_top.png");
    _centerMC = Sprite::createWithSpriteFrameName("log_center.png");
    float scaleX = width / 0.576f;
    float scaleY = height / 6.4f;
    _bottomMC->setScaleX(scaleX);
    _bottomMC->setScaleY(scaleY);
    _topMC->setScaleX(scaleX);
    _topMC->setScaleY(scaleY);
    _centerMC->setScaleX(scaleX);
    _centerMC->setScaleY(scaleY);
    _centerMC->setPosition(Vec2(0.0f, 10.0f));
    _bottomMC->setPosition(
        Vec2(0.0f, scaleY * (_bottomMC->getTextureRect().size.height * 0.5f) +
                       _shapeHeightMeters * -0.5f * ptm));
    _topMC->setPosition(
        Vec2(0.0f, scaleY * (_topMC->getTextureRect().size.height * -0.5f) +
                       _shapeHeightMeters * 0.5f * ptm));
    _mc->addChild(_topMC);
    _mc->addChild(_bottomMC);
    _mc->addChild(_centerMC);

    _rotation = _rotation * -0.0174532924f;
    createBody(b2Vec2(x, y), _rotation, _shapeWidthMeters, _shapeHeightMeters, fixed, sleeping);
    getLevel()->addToActions(this);
    return true;
}

// @005e69ec
void Log::createBody(b2Vec2 position, float angle, float width, float height, bool fixed,
                     bool sleeping)
{
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.7f;
    fixtureDef.restitution = 0.2f;
    fixtureDef.density = 30.0f;
    fixtureDef.filter.categoryBits = 8;

    b2BodyDef bodyDef;
    b2PolygonShape shape;
    bodyDef.type = fixed ? b2_staticBody : b2_dynamicBody;

    if (!fixed)
    {
        shape.SetAsBox(width * 0.5f, height * 0.5f);
        bodyDef.userData = _mc;
        bodyDef.position = position;
        bodyDef.angle = angle;
        bodyDef.awake = !sleeping;
        fixtureDef.shape = &shape;
        _body = getWorld()->CreateBody(&bodyDef);
        _shape = _body->CreateFixture(&fixtureDef);
        _body->ResetMassData();
        getLevel()->addToPaintBody(_body);
        addToBeginContact(_shape);
    }
    else
    {
        _center = position;
        shape.SetAsBox(width * 0.5f, height * 0.5f, _center, angle);
        fixtureDef.shape = &shape;
        _shape = getLevelBody()->CreateFixture(&fixtureDef);
    }
    addToPostSolve(_shape);
}

// @005e6bb0
void Log::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                    const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2)
    {
        maxImpulse = maxImpulse > impulse->normalImpulses[1] ? maxImpulse
                                                             : impulse->normalImpulses[1];
    }
    if (maxImpulse > _maxImpulse)
    {
        removePostSolve(_shape);
        removeBeginContact(_shape);
        getLevel()->addToSingleActions(this);
    }
}

// @005e6c24
void Log::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    contactSoundHandler(fixture, otherFixture, contact, nullptr);
}

// @005e6c34
void Log::actions()
{
    handleContactAdds();
}

// @005e6c40
void Log::handleContactAdds()
{
    if (!_sound && _body)
    {
        if (_contactAddBufferDict.find(_shape) != _contactAddBufferDict.end())
        {
            int index = (int)ceilf(CCRANDOM_0_1() * 2);
            _sound = createBodySound("LumberHit" + patch::to_string(index), _body, 1.0f, false);
            if (_sound)
            {
                // @005e7648 ($_0)
                _sound->setFinishCallback([this](int&) { soundStopped(); });
            }
        }
    }
    _contactAddBufferDict.clear();
}

// @005e6e90
void Log::soundStopped()
{
    _sound = nullptr;
}

// @005e6e98
void Log::singleAction()
{
    if (_sound)
    {
        _sound->stop();
    }
    b2World* world = getWorld();
    getLevel()->removeFromActions(this);
    removePostSolve(_shape);

    b2Vec2 position;
    float angle;
    b2Vec2 linearVelocity;
    float angularVelocity;
    if (!_body)
    {
        position = _center;
        angle = _rotation;
        float ptm = getPtm();
        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles)
        {
            Emitter* burst =
                BurstEmitter::createWoodchipBurst(nullptr, Vec2(position.x * ptm, position.y * ptm));
            if (burst)
            {
                particles->addChild(burst);
            }
        }
        getLevelBody()->DestroyFixture(_shape);
        linearVelocity = b2Vec2(0.0f, 0.0f);
        angularVelocity = 0.0f;
    }
    else
    {
        getLevel()->removeFromPaintBody(_body);
        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles)
        {
            Emitter* burst = BurstEmitter::createWoodchipBurst(_body, Vec2::ZERO);
            if (burst)
            {
                particles->addChild(burst);
            }
        }
        position = _body->GetWorldCenter();
        angle = _body->GetAngle();
        linearVelocity = _body->GetLinearVelocity();
        angularVelocity = _body->GetAngularVelocity();
        world->DestroyBody(_body);
        _body = nullptr;
    }

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;

    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.7f;
    fixtureDef.restitution = 0.2f;
    fixtureDef.density = 30.0f;
    fixtureDef.filter.categoryBits = 8;

    b2PolygonShape shape;
    shape.SetAsBox(_shapeWidthMeters * 0.5f, _shapeHeightMeters * 0.25f,
                   b2Vec2(0.0f, _shapeHeightMeters * -0.25f), 0.0f);
    fixtureDef.shape = &shape;
    b2Body* bottomBody = world->CreateBody(&bodyDef);
    bottomBody->CreateFixture(&fixtureDef);
    bottomBody->ResetMassData();

    shape.SetAsBox(_shapeWidthMeters * 0.5f, _shapeHeightMeters * 0.25f,
                   b2Vec2(0.0f, _shapeHeightMeters * 0.25f), 0.0f);
    b2Body* topBody = world->CreateBody(&bodyDef);
    topBody->CreateFixture(&fixtureDef);
    topBody->ResetMassData();

    bottomBody->SetLinearVelocity(linearVelocity);
    bottomBody->SetAngularVelocity(angularVelocity);
    topBody->SetLinearVelocity(linearVelocity);
    topBody->SetAngularVelocity(angularVelocity);

    _mc->removeFromParentAndCleanup(false);
    Node* itemsNode = getLevelItemsNode();
    float width = _shapeWidthMeters;
    float height = _shapeHeightMeters;
    _bottomMC = Sprite::createWithSpriteFrameName("log_bottom.png");
    _bottomMC->setAnchorPoint(Vec2(0.5f, 0.95f));
    _bottomMC->setScaleX(width / 0.576f);
    _bottomMC->setScaleY(height / 6.4f);
    _topMC = Sprite::createWithSpriteFrameName("log_top.png");
    _topMC->setAnchorPoint(Vec2(0.5f, 0.03f));
    _topMC->setScaleX(width / 0.576f);
    _topMC->setScaleY(height / 6.4f);
    itemsNode->addChild(_topMC);
    itemsNode->addChild(_bottomMC);
    bottomBody->SetUserData(_bottomMC);
    topBody->SetUserData(_topMC);
    getLevel()->addToPaintBody(bottomBody);
    getLevel()->addToPaintBody(topBody);
    _body = nullptr;
    createBodySound("LumberBreak", bottomBody, 1.0f, false);
}

// @005e7420
void Log::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (!_body)
    {
        return;
    }
    if (action == 1)
    {
        LevelB2D* level = getLevel();
        float impulseX = properties[0];
        float impulseY = properties[1];
        level->convertDirectionIfNecessaryBasedOnRegistration(&impulseY);
        _body->ApplyLinearImpulse(
            b2Vec2(impulseX * _body->GetMass(), impulseY * _body->GetMass()),
            _body->GetWorldCenter(), true);
        float spin = properties[2];
        level->convertDirectionIfNecessaryBasedOnRegistration(&spin);
        _body->SetAngularVelocity(_body->GetAngularVelocity() + spin);
    }
    else if (action == 0)
    {
        _body->SetAwake(true);
    }
}

// @005e7584
b2Body* Log::getJointBody(b2Vec2 point)
{
    return _body;
}

// @005e758c
std::vector<b2Body*> Log::getBodyList()
{
    std::vector<b2Body*> bodies;
    if (_body)
    {
        bodies.push_back(_body);
    }
    return bodies;
}
