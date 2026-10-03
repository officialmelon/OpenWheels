#include "TargetAction.h"

#include <cmath>

#include "2d/CCSprite.h"
#include "LevelB2D.h"
#include "ShapeItem.h"

USING_NS_CC;

// @0056fb80
TargetAction::TargetAction()
    : _shape(nullptr)
    , _refSprite(nullptr)
    , _shapeItem(nullptr)
    , _sprite(nullptr)
    , _actionIndex(0)
    , _action(0)
    , _properties()
    , _lastAngle(0.0f)
    , _counter(0.0f)
{
}

// @0056fbc8 (D1), @0056fc08 (D0)
TargetAction::~TargetAction()
{
}

// @0056fc2c
TargetAction* TargetAction::create(Sprite* refSprite, b2Fixture* fixture, Sprite* sprite,
                                   int action, std::vector<float> properties)
{
    // Plain (throwing) new, no null check; the result of init is ignored (init inlined).
    TargetAction* targetAction = new TargetAction();
    targetAction->initWithRefSprite(refSprite, fixture, sprite, action, properties);
    targetAction->autorelease();
    return targetAction;
}

// @0056fe20
bool TargetAction::initWithRefSprite(Sprite* refSprite, b2Fixture* fixture, Sprite* sprite,
                                     int action, std::vector<float> properties)
{
    _shape = fixture;
    _refSprite = refSprite;
    _sprite = sprite;
    _actionIndex = action;
    // _action is NOT set on this path (stays 0 from the constructor): singleAction() then
    // always takes the "wake" case. Kept as in the original.
    _lastAngle = refSprite->getRotation() * -0.017453292f;
    _instant = (action != 3);
    _properties = properties;
    return true;
}

// @0056fea0
TargetAction* TargetAction::create(ShapeItem* shapeItem, b2Fixture* fixture, Sprite* sprite,
                                   int action, std::vector<float> properties)
{
    // Plain (throwing) new, no null check; the result of init is ignored (init inlined).
    TargetAction* targetAction = new TargetAction();
    targetAction->initWithShapeItem(shapeItem, fixture, sprite, action, properties);
    targetAction->autorelease();
    return targetAction;
}

// @00570070
bool TargetAction::initWithShapeItem(ShapeItem* shapeItem, b2Fixture* fixture, Sprite* sprite,
                                     int action, std::vector<float> properties)
{
    _shape = fixture;
    _shapeItem = shapeItem;
    _sprite = sprite;
    _actionIndex = action;
    _action = action;
    _instant = (action != 3);
    _properties = properties;
    return true;
}

// @005700b4
void TargetAction::updateTargetActionsForCurrentShape(b2Fixture* oldFixture, b2Fixture* newFixture)
{
    _shape = newFixture;
}

// @005700bc
void TargetAction::updateTargetActionsForShapeItem(ShapeItem* shapeItem, b2Fixture* oldFixture,
                                                   b2Fixture* newFixture)
{
    _shapeItem = shapeItem;
    _shape = newFixture;
}

// @005700c8
void TargetAction::singleAction()
{
    switch (_action)
    {
        case 0:  // wake
            if (_shape != nullptr && _shape->GetBody() != nullptr)
            {
                _shape->GetBody()->SetAwake(true);
            }
            break;

        case 1:  // make static
            if (_shape != nullptr)
            {
                _shapeItem->setStatic(true);
                b2Body* body = _shape->GetBody();
                if (body->GetType() == b2_dynamicBody)
                {
                    body->SetType(b2_staticBody);
                }
            }
            break;

        case 2:  // make dynamic
            if (_shape != nullptr)
            {
                _shapeItem->setStatic(false);
                b2Body* body = _shape->GetBody();
                if (body == getLevelBody())
                {
                    // The shape is a fixture of the static level body: move it onto a new
                    // dynamic body of its own.
                    b2FixtureDef fixtureDef;
                    fixtureDef.density = _shape->GetDensity();
                    const float version = getLevel()->getVersion();
                    fixtureDef.userData = nullptr;
                    fixtureDef.friction = 0.2f;
                    fixtureDef.restitution = 0.1f;
                    fixtureDef.isSensor = false;
                    b2Filter filter = _shape->GetFilterData();
                    if (version > 1.84f)
                    {
                        if (filter.categoryBits == 0x10)
                        {
                            filter.maskBits = 0x10;
                            filter.groupIndex = -322;
                        }
                        else if (filter.categoryBits == 0x30)
                        {
                            filter.categoryBits = 0x20;
                        }
                        else if (filter.categoryBits == 0x18)
                        {
                            filter.categoryBits = 8;
                            if (filter.maskBits == 0x38)
                            {
                                filter.maskBits = 8;
                            }
                            else if (filter.groupIndex == -10)
                            {
                                filter.groupIndex = 0;
                            }
                        }
                    }
                    else if (filter.groupIndex == -10 && filter.maskBits == 0xffff)
                    {
                        filter.categoryBits = 8;
                        filter.groupIndex = 0;
                    }
                    fixtureDef.filter = filter;

                    b2BodyDef bodyDef;
                    bodyDef.type = b2_dynamicBody;
                    b2Body* newBody = getWorld()->CreateBody(&bodyDef);
                    fixtureDef.shape = _shape->GetShape();
                    b2Fixture* newFixture = newBody->CreateFixture(&fixtureDef);
                    _shapeItem->setFixtureRef(newFixture);
                    _shapeItem->setPtmRatio(getPtm());
                    _shapeItem->removeFromOwner(false);
                    getLevel()->addShapeItem(_shapeItem);
                    b2Fixture* oldFixture = _shape;
                    getLevel()->updateTargetActionsFor(_shapeItem->getIndex(), _shapeItem,
                                                       oldFixture, newFixture, this);
                    getLevelBody()->DestroyFixture(_shape);
                    _shape = newFixture;
                }
                else if (body->GetType() != b2_dynamicBody)
                {
                    body->SetType(b2_dynamicBody);
                }
            }
            break;

        case 4:  // impulse: _properties = {x, y, angular}
            if (_shape != nullptr)
            {
                b2Body* body = _shape->GetBody();
                if (body->GetMass() > 0.0f)
                {
                    LevelB2D* level = getLevel();
                    const float x = _properties[0];
                    float y = _properties[1];
                    level->convertDirectionIfNecessaryBasedOnRegistration(&y);
                    body->ApplyLinearImpulse(body->GetMass() * b2Vec2(x, y),
                                             body->GetWorldCenter(), true);
                    float angular = _properties[2];
                    if (angular != 0.0f)
                    {
                        level->convertRotationData(&angular);
                        body->SetAngularVelocity(body->GetAngularVelocity() + angular);
                    }
                }
            }
            break;

        case 5:  // release from its group body / the level body
            if (_shape != nullptr)
            {
                b2Body* body = _shape->GetBody();
                if (body == getLevelBody())
                {
                    getLevelBody()->DestroyFixture(_shape);
                    _shape = nullptr;
                    getLevel()->updateTargetActionsFor(_shapeItem->getIndex(), _shapeItem, nullptr,
                                                       nullptr, this);
                    return;
                }
                _shapeItem->removeFromOwner(true);
                _shapeItem->setStatic(true);
                getLevel()->addShapeItem(_shapeItem);
                getLevel()->updateTargetActionsFor(_shapeItem->getIndex(), _shapeItem, nullptr,
                                                   nullptr, this);
                getWorld()->DestroyBody(body);
                _shapeItem = nullptr;
            }
            break;

        case 6:  // delete
        {
            if (_shape != nullptr)
            {
                b2Body* body = _shape->GetBody();
                if (body == getLevelBody())
                {
                    body->DestroyFixture(_shape);
                }
                else
                {
                    getWorld()->DestroyBody(body);
                }
                _shape = nullptr;
            }
            unsigned int index;
            if (_shapeItem != nullptr)
            {
                index = _shapeItem->getIndex();
                _shapeItem->removeFromDrawNode();
                _shapeItem->removeFromOwner(true);
                _shapeItem = nullptr;
            }
            else
            {
                index = -1;
            }
            getLevel()->updateTargetActionsFor(index, nullptr, nullptr, nullptr, this);
            return;
        }

        case 7:  // change collision: _properties[0] = collision type
        {
            b2Fixture* fixture = _shape;
            if (fixture != nullptr)
            {
                b2Filter filter = fixture->GetFilterData();
                const int collision = (int)_properties[0];
                switch (collision)
                {
                    case 2:
                        filter.categoryBits = 8;
                        filter.maskBits = 8;
                        filter.groupIndex = 0;
                        break;
                    case 3:
                        filter.categoryBits = 1;
                        filter.maskBits = 1;
                        filter.groupIndex = -10;
                        break;
                    case 4:
                        filter.categoryBits = 8;
                        filter.maskBits = 0xffff;
                        filter.groupIndex = -321;
                        break;
                    case 5:
                        filter.categoryBits = 0x10;
                        filter.maskBits = 0x10;
                        filter.groupIndex = -322;
                        break;
                    case 6:
                        filter.categoryBits = 0x10;
                        filter.maskBits = 0x10;
                        filter.groupIndex = 0;
                        break;
                    default:
                        filter.categoryBits = 8;
                        filter.maskBits = 0xffff;
                        if (fixture->GetBody()->GetMass() == 0.0f)
                        {
                            filter.categoryBits = 0x18;
                            filter.groupIndex = -10;
                        }
                        break;
                }
                fixture->SetSensor(collision == 3);
                _shape->SetFilterData(filter);
            }
            getLevel()->updateTargetActionsFor(_shapeItem->getIndex(), _shapeItem, _shape, _shape,
                                               this);
            break;
        }

        default:  // 3 (fade) runs in actions()
            break;
    }
}

// @00570768
void TargetAction::actions()
{
    if (_action == 3)
    {
        // Fade: _properties = {target opacity in percent, duration in seconds}.
        const float duration = _properties[1];
        const float targetOpacity = _properties[0] * 0.01f;
        if (_counter >= duration)
        {
            if (_shapeItem != nullptr)
            {
                _shapeItem->setOpacity(targetOpacity);
            }
            getLevel()->removeFromActions(this);
            _counter = 0.0f;
            return;
        }
        float opacity = 0.0f;
        if (_shapeItem != nullptr)
        {
            opacity = _shapeItem->getOpacity();
        }
        const float remainingSteps = (duration - _counter) / getTimeStep();
        if (_shapeItem != nullptr)
        {
            opacity = opacity + (targetOpacity - opacity) / remainingSteps;
            _shapeItem->setOpacity(std::fmin(std::fmax(opacity, 0.0f), 1.0f));
        }
    }
    _counter += getTimeStep();
}
