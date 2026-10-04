#include "TargetActionGroup.h"

#include <algorithm>
#include <cmath>

#include "GroupItem.h"
#include "LevelB2D.h"
#include "online/FlashRuntime.h"    // ONLINE (PC addition)
#include "online/TriggerFilters.h"  // ONLINE (PC addition)

USING_NS_CC;

// @00570b24
TargetActionGroup::TargetActionGroup()
    : _properties()
    , _counter(0.0f)
    , _groupItem(nullptr)
    , _shape(nullptr)
    , _actionIndex(-1)
{
}

// @00570b74 (D1), @00570bb4 (D0)
TargetActionGroup::~TargetActionGroup()
{
}

// @00570bd8
TargetActionGroup* TargetActionGroup::create(GroupItem* groupItem, b2Fixture* fixture, int action,
                                             std::vector<float> properties)
{
    // Plain (throwing) new, no null check; the result of init is ignored (init inlined).
    TargetActionGroup* targetAction = new TargetActionGroup();
    targetAction->initWithGroupItem(groupItem, fixture, action, properties);
    targetAction->autorelease();
    return targetAction;
}

// @00570da8
bool TargetActionGroup::initWithGroupItem(GroupItem* groupItem, b2Fixture* fixture, int action,
                                          std::vector<float> properties)
{
    _groupItem = groupItem;
    _shape = fixture;
    _targetActionType = TargetActionBaseTypeGroup;
    _actionIndex = action;
    _instant = (action != 1);
    _properties = properties;
    return true;
}

// @00570df0
void TargetActionGroup::singleAction()
{
    if (online::flashLevel())
    {
        onlineSingleAction();  // ONLINE (PC addition)
        return;
    }
    switch (_actionIndex)
    {
        case 0:  // wake
            if (_groupItem != nullptr)
            {
                b2Body* body = _groupItem->getBody();
                if (body != nullptr)
                {
                    body->SetAwake(true);
                }
            }
            break;

        case 2:  // impulse: _properties = {x, y, angular}
            if (_groupItem != nullptr)
            {
                b2Body* body = _groupItem->getBody();
                if (body != nullptr && body->GetMass() > 0.0f)
                {
                    const float x = _properties[0];
                    float y = _properties[1];
                    float angular = _properties[2];
                    LevelB2D* level = getLevel();
                    level->convertDirectionIfNecessaryBasedOnRegistration(&y);
                    level->convertRotationData(&angular);
                    body->ApplyLinearImpulse(body->GetMass() * b2Vec2(x, y),
                                             body->GetWorldCenter(), true);
                    if (angular != 0.0f)
                    {
                        body->SetAngularVelocity(angular + body->GetAngularVelocity());
                    }
                }
            }
            break;

        case 3:  // make static (no null check on the body)
            if (_groupItem != nullptr)
            {
                b2Body* body = _groupItem->getBody();
                if (body->GetType() == b2_dynamicBody)
                {
                    body->SetType(b2_staticBody);
                }
            }
            break;

        case 4:  // make dynamic
            if (_groupItem != nullptr)
            {
                b2Body* body = _groupItem->getBody();
                if (body != nullptr && body->GetType() == b2_staticBody)
                {
                    body->SetType(b2_dynamicBody);
                    if (getLevel()->getVersion() > 1.84f)
                    {
                        for (b2Fixture* fixture = body->GetFixtureList(); fixture != nullptr;
                             fixture = fixture->GetNext())
                        {
                            b2Filter filter = fixture->GetFilterData();
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
                            fixture->SetFilterData(filter);
                        }
                    }
                }
            }
            break;

        case 5:  // stop interactivity
            if (_groupItem != nullptr && _groupItem->getBody() != nullptr)
            {
                _groupItem->stopInteractivity();
                getLevel()->updateTargetActionGroupsFor(_index, _groupItem, this);
            }
            break;

        case 6:  // delete
            if (_groupItem != nullptr)
            {
                _groupItem->stopInteractivity();
                _groupItem->removeSprites();
                getLevel()->removeGroupItem(_groupItem);
                _groupItem = nullptr;
                getLevel()->updateTargetActionGroupsFor(_index, nullptr, this);
            }
            break;

        case 7:  // change collision of every fixture: _properties[0] = collision type
            if (_groupItem != nullptr)
            {
                const float collisionValue = _properties[0];
                if (_groupItem->getBody() != nullptr)
                {
                    const int collision = (int)collisionValue;
                    for (b2Fixture* fixture = _groupItem->getBody()->GetFixtureList();
                         fixture != nullptr; fixture = fixture->GetNext())
                    {
                        b2Filter filter = fixture->GetFilterData();
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
                        fixture->SetFilterData(filter);
                    }
                    getLevel()->updateTargetActionGroupsFor(_index, _groupItem, this);
                }
            }
            break;

        default:  // 1 (fade) runs in actions()
            break;
    }
}

// ---------------------------------------------------------------------------------------------
// ONLINE (PC addition): Flash TargetActionGroup.singleAction (browser levels). Filters only change
// on the group's own shape fixtures (Flash: m_material == 8); "set to fixed" uses the 1.85+
// fixed filter variants and drops joints to fixed-rotation bodies (levels > 1.84); a group whose
// body is gone ("delete shapes") ignores body actions instead of dereferencing it.
void TargetActionGroup::onlineSingleAction()
{
    b2Body* body = _groupItem != nullptr ? _groupItem->getBody() : nullptr;
    const float version = online::flashVersion();
    auto ownFixture = [this](b2Fixture* fixture) {
        const std::vector<b2Fixture*>& own = _groupItem->onlineShapeFixtures;
        return std::find(own.begin(), own.end(), fixture) != own.end();
    };
    switch (_actionIndex)
    {
        case 0:  // wake from sleep
            if (body != nullptr)
            {
                body->SetAwake(true);
            }
            break;

        case 2:  // apply impulse: {x, y, spin}
        {
            if (body == nullptr || !(body->GetMass() > 0.0f) || _properties.size() < 2)
            {
                break;
            }
            LevelB2D* level = getLevel();
            float y = _properties[1];
            level->convertDirectionIfNecessaryBasedOnRegistration(&y);
            body->ApplyLinearImpulse(body->GetMass() * b2Vec2(_properties[0], y),
                                     body->GetWorldCenter(), true);
            float spin = _properties.size() > 2 ? _properties[2] : 0.0f;
            if (spin != 0.0f && spin == spin)
            {
                level->convertRotationData(&spin);
                body->SetAngularVelocity(body->GetAngularVelocity() + spin);
            }
            break;
        }

        case 3:  // set to fixed
        {
            if (body == nullptr || !(body->GetMass() > 0.0f))
            {
                break;
            }
            body->SetType(b2_staticBody);
            if (version > 1.84f)
            {
                for (b2Fixture* fixture = body->GetFixtureList(); fixture != nullptr;
                     fixture = fixture->GetNext())
                {
                    if (ownFixture(fixture))
                    {
                        b2Filter filter = fixture->GetFilterData();
                        online::filterToFixed(&filter, version, true);
                        fixture->SetFilterData(filter);
                    }
                }
                online::destroyJointsOf(body, [body](b2Joint* joint) {
                    b2Body* other = joint->GetBodyA() == body ? joint->GetBodyB() : joint->GetBodyA();
                    return other->IsFixedRotation() && other->GetMass() != 0.0f;
                });
            }
            break;
        }

        case 4:  // set to non fixed
        {
            if (body == nullptr || body->GetType() != b2_staticBody ||
                getLevel()->onlineNanMassBodies.count(body))
            {
                break;
            }
            body->SetType(b2_dynamicBody);
            body->SetAwake(true);
            if (version > 1.84f)
            {
                for (b2Fixture* fixture = body->GetFixtureList(); fixture != nullptr;
                     fixture = fixture->GetNext())
                {
                    b2Filter filter = fixture->GetFilterData();
                    online::filterToNonFixed(&filter, version, true);
                    fixture->SetFilterData(filter);
                }
            }
            break;
        }

        case 5:  // delete shapes: the body goes, the art stays where it is
            if (body != nullptr)
            {
                _groupItem->onlineShapeFixtures.clear();
                _groupItem->stopInteractivity();
                getLevel()->updateTargetActionGroupsFor(_index, _groupItem, this);
            }
            break;

        case 6:  // delete self
            if (_groupItem != nullptr)
            {
                _groupItem->onlineShapeFixtures.clear();
                _groupItem->stopInteractivity();
                _groupItem->removeSprites();
                getLevel()->removeGroupItem(_groupItem);
                _groupItem = nullptr;
                getLevel()->updateTargetActionGroupsFor(_index, nullptr, this);
            }
            break;

        case 7:  // change collision: {collision type}
        {
            if (body == nullptr || _properties.empty())
            {
                break;
            }
            const int collision = (int)_properties[0];
            const bool fixed =
                body->GetMass() == 0.0f && !getLevel()->onlineNanMassBodies.count(body);
            for (b2Fixture* fixture = body->GetFixtureList(); fixture != nullptr;
                 fixture = fixture->GetNext())
            {
                if (!ownFixture(fixture))
                {
                    continue;
                }
                b2Filter filter = fixture->GetFilterData();
                bool sensor = false;
                online::filterForCollision(&filter, &sensor, collision, fixed, version);
                fixture->SetSensor(sensor);
                fixture->SetFilterData(filter);
            }
            break;
        }

        default:  // 1 (change opacity) runs in actions()
            break;
    }
}

// @005712a0
void TargetActionGroup::actions()
{
    if (_actionIndex == 1)
    {
        // Fade: _properties = {target opacity in percent, duration in seconds}.
        if (_groupItem == nullptr)
        {
            getLevel()->removeFromActions(this);
        }
        else
        {
            const float duration = _properties[1];
            const float targetOpacity = _properties[0] * 0.01f;
            if (_counter >= duration)
            {
                _groupItem->setOpacity(targetOpacity);
                getLevel()->removeFromActions(this);
                // No return (unlike TargetAction): the counter restarts at one time step.
                if (online::flashLevel())
                {
                    // ONLINE (PC addition): Flash restarts at 0 (levels > 1.8) and keeps the
                    // counter of older levels at the duration (the next fade is instant).
                    if (online::flashVersion() > 1.8f)
                    {
                        _counter = 0.0f;
                    }
                    return;
                }
                _counter = 0.0f;
            }
            else
            {
                float opacity = _groupItem->getArtOpacity();
                opacity = opacity + (targetOpacity - opacity) / ((duration - _counter) / getTimeStep());
                _groupItem->setOpacity(std::fmin(std::fmax(opacity, 0.0f), 1.0f));
            }
        }
    }
    _counter += getTimeStep();
}

// @00571394
void TargetActionGroup::updateTargetActionsForGroupItem(GroupItem* groupItem)
{
    _groupItem = groupItem;
}
