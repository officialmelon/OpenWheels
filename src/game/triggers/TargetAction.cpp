#include "TargetAction.h"

#include <cmath>

#include "2d/CCSprite.h"
#include "LevelB2D.h"
#include "ShapeItem.h"
#include "DestructionListener.h"
#include "Session.h"
#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "online/TriggerFilters.h"  // ONLINE (PC addition)

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
    if (online::flashLevel())
    {
        onlineSingleAction();  // ONLINE (PC addition)
        return;
    }
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

// ---------------------------------------------------------------------------------------------
// ONLINE (PC addition): Flash TargetAction.singleAction (browser levels). Flash rebuilds a shape
// when it changes between fixed and non-fixed (Box2D 2.0 has no body types); here the body type
// is switched and the visible differences of the rebuild are reproduced: the "fixed" collision
// filter variants (levels > 1.84), the default friction 0.2 / restitution 0 of the new shape,
// density 1 for a shape that was fixed in the editor, and the joints of a body that becomes
// fixed are destroyed with it. "Delete shape" removes the physics and leaves the art frozen where
// it is, "change collision" uses Flash's per-version filters. Targets that no longer exist are
// skipped (Flash keeps null references and checks them).
void TargetAction::onlineSingleAction()
{
    const float version = online::flashVersion();
    switch (_action)
    {
        case 0:  // wake from sleep
            if (_shape != nullptr && _shape->GetBody() != nullptr)
            {
                _shape->GetBody()->SetAwake(true);
            }
            break;

        case 1:  // set to fixed
        {
            if (_shape == nullptr)
            {
                break;
            }
            b2Body* body = _shape->GetBody();
            if (!(body->GetMass() > 0.0f))
            {
                break;
            }
            online::destroyJointsOf(body);
            if (_shapeItem != nullptr)
            {
                _shapeItem->setStatic(true);
            }
            body->SetType(b2_staticBody);
            b2Filter filter = _shape->GetFilterData();
            online::filterToFixed(&filter, version);
            _shape->SetFilterData(filter);
            _shape->SetFriction(0.2f);
            _shape->SetRestitution(0.0f);
            break;
        }

        case 2:  // set to non fixed
        {
            if (_shape == nullptr)
            {
                break;
            }
            b2Body* body = _shape->GetBody();
            if (body->GetMass() != 0.0f || getLevel()->onlineNanMassBodies.count(body))
            {
                break;
            }
            b2Filter filter = _shape->GetFilterData();
            online::filterToNonFixed(&filter, version);
            if (body == getLevelBody())
            {
                // Mobile path of the original (a new body for the fixture), with Flash's values.
                b2FixtureDef fixtureDef;
                // Flash builds fixed shapes with density 0 and gives the new body density 1.
                fixtureDef.density = 1.0f;
                fixtureDef.friction = 0.2f;
                fixtureDef.restitution = 0.0f;
                fixtureDef.isSensor = _shape->IsSensor();
                fixtureDef.filter = filter;
                b2BodyDef bodyDef;
                bodyDef.type = b2_dynamicBody;
                b2Body* newBody = getWorld()->CreateBody(&bodyDef);
                fixtureDef.shape = _shape->GetShape();
                b2Fixture* newFixture = newBody->CreateFixture(&fixtureDef);
                const int material = getLevel()->getFixtureMaterial(_shape);
                getLevel()->removeFixtureMaterial(_shape);
                if (material != 0)
                {
                    getLevel()->addFixtureMaterial(newFixture, material);
                }
                if (_shapeItem != nullptr)
                {
                    _shapeItem->setStatic(false);
                    _shapeItem->setFixtureRef(newFixture);
                    _shapeItem->setPtmRatio(getPtm());
                    _shapeItem->removeFromOwner(false);
                    getLevel()->addShapeItem(_shapeItem);
                    getLevel()->updateTargetActionsFor(_shapeItem->getIndex(), _shapeItem, _shape,
                                                       newFixture, this);
                }
                getLevelBody()->DestroyFixture(_shape);
                _shape = newFixture;
            }
            else
            {
                if (_shapeItem != nullptr)
                {
                    _shapeItem->setStatic(false);
                }
                body->SetType(b2_dynamicBody);
                body->SetAwake(true);
                _shape->SetFilterData(filter);
                _shape->SetFriction(0.2f);
                _shape->SetRestitution(0.0f);
            }
            break;
        }

        case 4:  // apply impulse: {x, y, spin}
        {
            if (_shape == nullptr)
            {
                break;
            }
            b2Body* body = _shape->GetBody();
            const float mass = body->GetMass();
            if (!(mass > 0.0f) || _properties.size() < 2)
            {
                break;
            }
            LevelB2D* level = getLevel();
            float y = _properties[1];
            level->convertDirectionIfNecessaryBasedOnRegistration(&y);
            body->ApplyLinearImpulse(mass * b2Vec2(_properties[0], y), body->GetWorldCenter(),
                                     true);
            float spin = _properties.size() > 2 ? _properties[2] : 0.0f;
            if (spin != 0.0f && spin == spin)
            {
                level->convertRotationData(&spin);
                body->SetAngularVelocity(body->GetAngularVelocity() + spin);
            }
            break;
        }

        case 5:  // delete shape: the physics goes, the art stays where it is
        {
            if (_shape == nullptr)
            {
                break;
            }
            b2Body* body = _shape->GetBody();
            getLevel()->removeFixtureMaterial(_shape);
            if (_shapeItem != nullptr)
            {
                _shapeItem->setStatic(true);  // freeze the art at the current transform
                _shapeItem->setFixtureRef(nullptr);
            }
            if (body == getLevelBody())
            {
                body->DestroyFixture(_shape);
            }
            else
            {
                getWorld()->DestroyBody(body);
            }
            _shape = nullptr;
            if (_shapeItem != nullptr)
            {
                getLevel()->updateTargetActionsFor(_shapeItem->getIndex(), _shapeItem, nullptr,
                                                   nullptr, this);
            }
            break;
        }

        case 6:  // delete self
        {
            unsigned int index = (unsigned int)-1;
            if (_shape != nullptr)
            {
                b2Body* body = _shape->GetBody();
                getLevel()->removeFixtureMaterial(_shape);
                if (_shapeItem != nullptr)
                {
                    _shapeItem->setFixtureRef(nullptr);
                }
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
            if (_shapeItem != nullptr)
            {
                index = _shapeItem->getIndex();
                _shapeItem->removeFromDrawNode();
                _shapeItem->removeFromOwner(true);
                _shapeItem = nullptr;
            }
            if (index != (unsigned int)-1)
            {
                getLevel()->updateTargetActionsFor(index, nullptr, nullptr, nullptr, this);
            }
            break;
        }

        case 7:  // change collision: {collision type}
        {
            if (_shape == nullptr || _properties.empty())
            {
                break;
            }
            const int collision = (int)_properties[0];
            const bool fixed = _shape->GetBody()->GetMass() == 0.0f &&
                               !getLevel()->onlineNanMassBodies.count(_shape->GetBody());
            b2Filter filter = _shape->GetFilterData();
            bool sensor = false;
            online::filterForCollision(&filter, &sensor, collision, fixed, version);
            _shape->SetSensor(sensor);
            _shape->SetFilterData(filter);
            break;
        }

        default:  // 3 (change opacity) runs in actions()
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
