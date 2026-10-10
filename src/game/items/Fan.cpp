#include "Fan.h"

#include <algorithm>
#include <cmath>

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "Settings.h"
#include "Sound.h"
#include "SoundController.h"
#include "online/FlashPhysics.h"  // ONLINE (PC addition)

USING_NS_CC;

// @005adf90 (D1), @005adfe0 (D0)
Fan::~Fan()
{
}

// @005ad010
bool Fan::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float y = 0.0f;
    float x = 0.0f;
    float rotation = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &rotation);
    getLevel()->convertPositionAndRotationData(&x, &y, &rotation);

    Node* itemsNode = getLevelItemsNode();
    _mc = Sprite::createWithSpriteFrameName("fan_base.png");
    _mc->setPosition(x * getPtm(), y * getPtm());
    _mc->setRotation(rotation);
    _mc->setAnchorPoint(Vec2(0.5f, 1.0f));
    itemsNode->addChild(_mc);
    Size size = _mc->getTextureRect().size;

    for (int i = 1; i < 7; i++)
    {
        if (i % 2 == 1)
        {
            _frames.push_back(SpriteFrameCache::getInstance()->getSpriteFrameByName(
                "fan_blade_" + patch::to_string(i) + ".png"));
        }
    }

    _fanBlade = Sprite::createWithSpriteFrameName("fan_blade_1.png");
    _fanBlade->setAnchorPoint(Vec2(0.5f, 0.0f));
    _fanBlade->setPosition(Vec2(size.width * 0.5f, size.height));
    _mc->addChild(_fanBlade);

    Sprite* cover = Sprite::createWithSpriteFrameName("fan_cover.png");
    cover->setAnchorPoint(Vec2(1.0f, 0.0f));
    cover->setPosition(Vec2(size.width * 0.5f + 1.0f, size.height));
    _mc->addChild(cover);
    cover = Sprite::createWithSpriteFrameName("fan_cover.png");
    cover->setScaleX(-1.0f);
    cover->setAnchorPoint(Vec2(1.0f, 0.0f));
    cover->setPosition(Vec2(size.width * 0.5f + -1.0f, size.height));
    _mc->addChild(cover);

    float angle = rotation * -0.0174532924f;
    _cosVal = cosf(angle);
    _sinVal = sinf(angle);
    createBodies(b2Vec2(x, y), angle);
    addToBeginContact(_sensor);
    addToEndContact(_sensor);
    _fanSound = Settings::getInstance()->getSoundController()->createPositionSound(
        "SwooshFan", Vec2(_centerMeters.x, _centerMeters.y), 1.0f, true);
    getLevel()->addToFrameActions(this);
    if (online::browserPhysicsWanted()) getLevel()->addToActions(this);  // ONLINE (PC addition)
    return true;
}

// @005ad7e8
void Fan::createBodies(b2Vec2 position, float angle)
{
    b2Body* levelBody = getLevelBody();
    b2PolygonShape shape;
    float sinVal = sinf(angle);
    float cosVal = cosf(angle);

    // the blow area in front of the fan
    _centerMeters = b2Vec2(position.x - sinVal * 4.8f, position.y + cosVal * 4.8f);
    shape.SetAsBox(2.4f, 4.0f, _centerMeters, angle);
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.isSensor = true;
    fixtureDef.filter.categoryBits = 8;
    fixtureDef.filter.groupIndex = -20;
    _sensor = levelBody->CreateFixture(&fixtureDef);

    // fan box
    fixtureDef.isSensor = false;
    fixtureDef.filter.groupIndex = 0;
    shape.SetAsBox(2.4f, 0.4f, b2Vec2(position.x - sinVal * 0.4f, position.y + cosVal * 0.4f),
                   angle);
    fixtureDef.shape = &shape;
    levelBody->CreateFixture(&fixtureDef);
    levelBody->ResetMassData();

    // housing
    AffineTransform transform = AffineTransformRotate(AffineTransformMakeIdentity(), angle);
    const Vec2 points[7] = {
        Vec2(-2.4f, 0.0f), Vec2(-2.0f, -1.352f), Vec2(-1.04f, -2.192f), Vec2(0.0f, -2.4f),
        Vec2(1.04f, -2.192f), Vec2(2.0f, -1.352f), Vec2(2.4f, 0.0f),
    };
    b2Vec2 verts[7];
    for (int i = 0; i < 7; i++)
    {
        verts[i] = position + b2Vec2(transform.a * points[i].x + transform.c * points[i].y +
                                         transform.tx,
                                     transform.b * points[i].x + transform.d * points[i].y +
                                         transform.ty);
    }
    shape.Set(verts, 7);
    fixtureDef.shape = &shape;
    levelBody->CreateFixture(&fixtureDef);
}

// @005ada68
void Fan::prepareForTrigger()
{
    getLevel()->removeFromActions(this);
    getLevel()->removeFromFrameActions(this);
    if (_fanSound)
    {
        _fanSound->stop();
        _fanSound = nullptr;
    }
}

// @005adab8
void Fan::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (_triggered)
    {
        return;
    }
    _triggered = true;
    getLevel()->addToActions(this);
    getLevel()->addToFrameActions(this);
    addToEndContact(_sensor);
    _fanSound = Settings::getInstance()->getSoundController()->createPositionSound(
        "SwooshFan", Vec2(_centerMeters.x, _centerMeters.y), 1.0f, true);
}

// @005adbb0
void Fan::frameAction()
{
    _skipFrame = !_skipFrame;
    // ONLINE (PC addition): two blade frames per browser physics step (1/30), one per 1/60 step.
    if (online::stepsPerFlashFrame() == 1 && ++_frameIndex == _frames.size()) _frameIndex = 0;
    _frameIndex++;
    if (_frameIndex == _frames.size())
    {
        _frameIndex = 0;
    }
    _fanBlade->setSpriteFrame(_frames[_frameIndex]);
    if (online::browserPhysics()) return;  // ONLINE (PC addition): blown in actions()
    blowBodies();  // inlined in the original
}

// ONLINE (PC addition): see Fan.h.
void Fan::actions()
{
    if (!online::browserPhysics()) return;
    std::vector<b2Body*> blown;
    for (b2Body* body : _bodies)
    {
        if (online::flashPersists(body, _sensor)) blown.push_back(body);
    }
    std::swap(blown, _bodies);
    blowBodies();
    std::swap(blown, _bodies);
}

// @005adcc4
void Fan::blowBodies()
{
    for (unsigned int i = 0; i < _bodies.size(); i++)
    {
        b2Body* body = _bodies[i];
        if (body->GetType() == b2_dynamicBody)
        {
            b2Vec2 center = body->GetWorldCenter();
            float distance = _sinVal * (center.x - _centerMeters.x) -
                             _cosVal * (center.y - _centerMeters.y) + 4.0f;
            distance = fmaxf(fminf(distance, 4.0f), 0.0f);
            float strength = distance * 0.25f;
            float force = strength * strength * 15.0f;
            body->ApplyForceToCenter(b2Vec2(-_sinVal * force, _cosVal * force), true);
        }
    }
}

// @005add7c
void Fan::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor())
    {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    if (std::find(_bodies.begin(), _bodies.end(), body) == _bodies.end())
    {
        _bodies.push_back(body);
    }
}

// @005adf20
void Fan::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor())
    {
        return;
    }
    std::vector<b2Body*>::iterator it =
        std::find(_bodies.begin(), _bodies.end(), otherFixture->GetBody());
    if (it != _bodies.end())
    {
        _bodies.erase(it);
    }
}
