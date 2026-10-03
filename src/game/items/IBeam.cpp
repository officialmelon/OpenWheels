#include "IBeam.h"

#include <cmath>
#include <cstdlib>
#include <new>

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"

USING_NS_CC;

// @005c8b8c (D0; the complete destructor is LevelItem's)
IBeam::~IBeam()
{
}

// @005d08e8 (header-inline in the original; emitted in LevelB2D's TU)
IBeam* IBeam::create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    IBeam* beam = new (std::nothrow) IBeam();
    if (beam)
    {
        if (beam->init(element, groupBody, groupOffset))
        {
            beam->autorelease();
        }
        else
        {
            delete beam;
            beam = nullptr;
        }
    }
    return beam;
}

// @005c8198
bool IBeam::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float width = 0.0f;
    float rotation = 0.0f;
    float y = 0.0f;
    float x = 0.0f;
    float height = 0.0f;
    bool fixed = false;
    bool sleeping = false;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &width);
    element->floatAttribute("p3", &height);
    element->floatAttribute("p4", &rotation);
    element->boolAttribute("p5", &fixed);
    element->boolAttribute("p6", &sleeping);
    _body = groupBody;
    getLevel()->convertPositionAndRotationData(&x, &y, &rotation);
    getLevel()->convertLengthData(&width);
    getLevel()->convertLengthData(&height);

    Node* itemsNode = getLevelItemsNode();
    _mc = Node::create();
    if (groupBody)
    {
        _mc->setPosition((groupOffset.x + groupBody->GetPosition().x) * getPtm(),
                         (groupOffset.y + groupBody->GetPosition().y) * getPtm());
    }
    else
    {
        _mc->setPosition(x * getPtm(), y * getPtm());
    }
    _mc->setRotation(rotation);

    Sprite* sprite = Sprite::createWithSpriteFrameName("ibeam.png");
    sprite->setScale(width / _size.width * 2.0f * 1.01f, height / _size.height);
    _mc->addChild(sprite);
    itemsNode->addChild(_mc);

    rotation = rotation * -0.0174532924f;
    createBody(b2Vec2(groupOffset.x + x, groupOffset.y + y), rotation, width, height, fixed,
               sleeping);
    return true;
}

// @005c85f0
void IBeam::createBody(b2Vec2 position, float angle, float width, float height, bool fixed,
                       bool sleeping)
{
    b2BodyDef bodyDef;
    b2FixtureDef fixtureDef;
    b2PolygonShape shape;
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    bodyDef.type = fixed ? b2_staticBody : b2_dynamicBody;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 75.0f;
    fixtureDef.filter.categoryBits = 8;

    if (!fixed)
    {
        if (!_body)
        {
            shape.SetAsBox(halfWidth, halfHeight);
            bodyDef.userData = _mc;
            bodyDef.position = position;
            bodyDef.angle = angle;
            bodyDef.awake = !sleeping;
            fixtureDef.shape = &shape;
            _body = getWorld()->CreateBody(&bodyDef);
            _fixture = _body->CreateFixture(&fixtureDef);
            _body->ResetMassData();
            getLevel()->addToPaintBody(_body);
        }
        else
        {
            // part of a group: the fixture goes onto the group body
            shape.SetAsBox(halfWidth, halfHeight, position, angle);
            fixtureDef.shape = &shape;
            _fixture = _body->CreateFixture(&fixtureDef);
        }
        addToBeginContact(_fixture);
    }
    else
    {
        shape.SetAsBox(halfWidth, halfHeight, position, angle);
        fixtureDef.shape = &shape;
        getLevelBody()->CreateFixture(&fixtureDef);
    }
}

// @005c87dc
void IBeam::paintWithOffsetPoints(Vec2 offset, float angleDegrees)
{
    _mc->setRotation(angleDegrees);
    _mc->setPosition(offset);
}

// @005c884c
void IBeam::actions()
{
    handleContactAdds();
}

// @005c8858
void IBeam::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (action == 1)
    {
        if (_body)
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
    }
    else if (action == 0 && _body)
    {
        _body->SetAwake(true);
    }
}

// @005c89c8
b2Body* IBeam::getJointBody(b2Vec2 point)
{
    return _body;
}

// @005c89d0
void IBeam::handleContactAdds()
{
    if (_contactAddBufferDict.find(_fixture) != _contactAddBufferDict.end())
    {
        int index = (int)ceilf(CCRANDOM_0_1() * 2);
        createBodySound("IBeamHit" + patch::to_string(index), _body, 1.0f, false);
    }
    _contactAddBufferDict.clear();
}

// @005c8b48
std::vector<b2Body*> IBeam::getBodyList()
{
    std::vector<b2Body*> bodies;
    if (_body)
    {
        bodies.push_back(_body);
    }
    return bodies;
}
