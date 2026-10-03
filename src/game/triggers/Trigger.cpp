#include "Trigger.h"

#include "base/CCDirector.h"
#include "base/CCEventDispatcher.h"

#include "CharacterB2D.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"
#include "Settings.h"
#include "SoundController.h"
#include "TargetAction.h"
#include "TargetActionGroup.h"
#include "TargetActionPrisJoint.h"
#include "TargetActionRevJoint.h"
#include "TargetActionSpecial.h"
#include "TargetActionTrigger.h"

USING_NS_CC;

// The unnamed functions of this TU are libc++/Box2D-independent template code generated from the
// containers used below (no game logic): @00572a20 noexcept-dtor terminate pad, @0057382c
// vector<float>::push_back slow path, @0057587c throw bad_array_new_length, @005758b0
// map<b2Body*,unsigned>::destroy, @005758f0 red-black tree rebalance after insert, @00575a90
// vector length_error("vector").

// @00572928
Trigger::Trigger()
    : _triggeringBody(nullptr)
    , _sensor(nullptr)
{
    // Every other scalar is left uninitialised until init(), as in the original.
}

// @0057297c (D1), @00572a30 (D0)
Trigger::~Trigger()
{
    for (int i = 0; i < _targets.size(); i++)
    {
        _targets[i]->release();
    }
    _targets.clear();
}

// @00572a54
bool Trigger::init(LevelDataElement* element, b2Body* levelBody, b2Vec2 offset)
{
    // levelBody / offset are unused: the sensor goes onto getLevelBody() at the item's position.
    _triggeringBody = nullptr;
    _sensor = nullptr;
    _soundEffect = "";
    _soundId = -1;
    _disabled = false;
    _activationDictionaryNulled = false;
    _systemTriggerId = -1;
    _counter = 0;
    _triggeredBy = 0;
    _xMeters = 0.0f;
    _yMeters = 0.0f;
    _volume = 0.0f;
    _panning = 0.0f;
    _delayTime = 0.0f;
    _type = -1;
    _triggeringCount = 0;
    _repeatType = 0;
    _repeatFrames = 0.0f;
    _repeatCount = 0.0f;
    _soundLocation = 1;

    element->floatAttribute("d", &_delayTime);
    element->floatAttribute("i", &_repeatFrames);
    _repeatCount = _repeatFrames;
    element->intAttribute("t", &_type);
    element->intAttribute("b", &_triggeredBy);
    element->intAttribute("r", &_repeatType);
    if (_type == TriggerTypeSystem)
    {
        element->intAttribute("i", &_systemTriggerId);
    }
    else if (_type == TriggerTypeSound)
    {
        element->intAttribute("l", &_soundLocation);
        element->floatAttribute("p", &_panning);
        element->floatAttribute("v", &_volume);
        element->intAttribute("s", &_soundId);
    }
    createShape(element);
    return true;
}

// @00572df8
void Trigger::createShape(LevelDataElement* element)
{
    element->floatAttribute("x", &_xMeters);
    element->floatAttribute("y", &_yMeters);
    getLevel()->convertPositionData(&_xMeters, &_yMeters);

    if (_triggeredBy != TriggerTriggeredByTrigger)
    {
        b2PolygonShape shape;
        b2FixtureDef fixtureDef;
        fixtureDef.isSensor = true;
        fixtureDef.filter.categoryBits = 8;
        fixtureDef.filter.groupIndex = -20;

        float width = 0.0f;
        float height = 0.0f;
        float angle = 0.0f;
        element->floatAttribute("w", &width);
        element->floatAttribute("h", &height);
        element->floatAttribute("a", &angle);
        getLevel()->convertLengthData(&width);
        getLevel()->convertLengthData(&height);
        angle = angle * -0.017453292f;
        shape.SetAsBox(width * 0.5f, height * 0.5f, b2Vec2(_xMeters, _yMeters), angle);
        fixtureDef.shape = &shape;

        _sensor = getLevelBody()->CreateFixture(&fixtureDef);
        if (static_cast<unsigned int>(_triggeredBy - 1) < 4)
        {
            addToBeginContact(_sensor);
        }
    }
}

// @005730e4
void Trigger::addActivationBody(b2Body* body)
{
    if (_triggeredBy == TriggerTriggeredByBodies && !_activationDictionaryNulled)
    {
        _activationDictionary[body] = 1;
    }
}

// @005731c0
void Trigger::addActivationBodies(std::vector<b2Body*> bodies)
{
    if (_triggeredBy == TriggerTriggeredByBodies && !_activationDictionaryNulled)
    {
        for (std::vector<b2Body*>::iterator it = bodies.begin(); it != bodies.end(); ++it)
        {
            _activationDictionary[*it] = 1;
        }
    }
}

// @005732dc
void Trigger::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    // The checkAddN helpers are inlined here in the original.
    switch (_triggeredBy)
    {
        case TriggerTriggeredByCharacter:
            checkAdd1(otherFixture);
            break;
        case TriggerTriggeredByAnyCharacter:
            checkAdd2(otherFixture);
            break;
        case TriggerTriggeredByAnyObject:
            checkAdd3(otherFixture);
            break;
        case TriggerTriggeredByBodies:
            checkAdd4(otherFixture);
            break;
    }
}

// @00573408
void Trigger::checkAdd1(b2Fixture* fixture)
{
    b2Body* body = fixture->GetBody();
    if (body == getLevel()->getCharacter()->getCentralBody())
    {
        addTriggeringBody(body);
    }
}

// @00573450
void Trigger::checkAdd2(b2Fixture* fixture)
{
    LevelItem* item = static_cast<LevelItem*>(fixture->GetUserData());
    if (item != nullptr && item->getSpecialType() == SpecialTypeCharacter)
    {
        b2Body* body = fixture->GetBody();
        if (body == static_cast<CharacterB2D*>(item)->getCentralBody())
        {
            addTriggeringBody(body);
        }
    }
}

// @005734c8
void Trigger::checkAdd3(b2Fixture* fixture)
{
    if (!fixture->IsSensor() && fixture->GetBody()->GetMass() > 0.0f)
    {
        addTriggeringBody(fixture->GetBody());
    }
}

// @005734e8
void Trigger::checkAdd4(b2Fixture* fixture)
{
    if (!fixture->IsSensor())
    {
        b2Body* body = fixture->GetBody();
        if (body->GetMass() > 0.0f &&
            _activationDictionary.find(body) != _activationDictionary.end())
        {
            addTriggeringBody(body);
        }
    }
}

// @00573548
void Trigger::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    b2Body* body = otherFixture->GetBody();
    // The contact count is decremented before the mass test (and stays decremented if it fails).
    if (body == _triggeringBody && !otherFixture->IsSensor() && --_triggeringCount <= 0 &&
        body->GetMass() > 0.0f)
    {
        _triggeringCount = 0;
        _triggeringBody = nullptr;
    }
}

// @0057358c
void Trigger::addTriggeringBody(b2Body* body)
{
    if (_triggeringBody != nullptr)
    {
        if (_triggeringBody == body)
        {
            _triggeringCount++;
        }
        return;
    }

    // Ignore a body that is already touching the sensor (contact points with exactly one of the
    // two fixtures being the sensor).
    b2ContactEdge* edge = body->GetContactList();
    if (edge != nullptr)
    {
        int points = 0;
        do
        {
            b2Contact* contact = edge->contact;
            if ((contact->GetFixtureA() == _sensor) != (contact->GetFixtureB() == _sensor))
            {
                points += contact->GetManifold()->pointCount;
            }
            edge = edge->next;
        } while (edge != nullptr);
        if (points > 0)
        {
            return;
        }
    }

    _triggeringBody = body;
    _triggeringCount++;
    // RE-NOTE(@0057358c): the result of this getSession() call is unused in the original.
    getSession();

    if (_repeatType > TriggerRepeatTypeEachTime)
    {
        if (getLevel()->addToActions(this) && _repeatType == TriggerRepeatTypeContinuous)
        {
            addToEndContact(_sensor);
        }
        return;
    }

    if (_repeatType == TriggerRepeatTypeOnce)
    {
        removeBeginContact(_sensor);
        _activationDictionary.clear();
        _activationDictionaryNulled = true;
    }
    else
    {
        addToEndContact(_sensor);
    }

    if (_delayTime == 0.0f)
    {
        getLevel()->addToSingleActions(this);
        return;
    }
    _delayVector.push_back(0.0f);
    getLevel()->addToActions(this);
}

// @0057399c
float Trigger::getXMeters()
{
    return _xMeters;
}

// @005739a4
void Trigger::setXMeters(float xMeters)
{
    _xMeters = xMeters;
}

// @005739ac
float Trigger::getYMeters()
{
    return _yMeters;
}

// @005739b4
void Trigger::setYMeters(float yMeters)
{
    _yMeters = yMeters;
}

// @005739bc
float Trigger::getVolume()
{
    return _volume;
}

// @005739c4
void Trigger::setVolume(float volume)
{
    _volume = volume;
}

// @005739cc
float Trigger::getPanning()
{
    return _panning;
}

// @005739d4
void Trigger::setPanning(float panning)
{
    _panning = panning;
}

// @005739dc
int Trigger::getType()
{
    return _type;
}

// @005739e4
void Trigger::setType(int type)
{
    _type = type;
}

// @005739ec
float Trigger::getDelayTime()
{
    return _delayTime;
}

// @005739f4
void Trigger::setDelayTime(float delayTime)
{
    _delayTime = delayTime;
}

// @005739fc
int Trigger::getCounter()
{
    return _counter;
}

// @00573a04
void Trigger::setCounter(int counter)
{
    _counter = counter;
}

// @00573a0c
int Trigger::getTriggeredBy()
{
    return _triggeredBy;
}

// @00573a14
void Trigger::setTriggeredBy(int triggeredBy)
{
    _triggeredBy = triggeredBy;
}

// @00573a1c
int Trigger::getTriggeringCount()
{
    return _triggeringCount;
}

// @00573a24
void Trigger::setTriggeringCount(int triggeringCount)
{
    _triggeringCount = triggeringCount;
}

// @00573a2c
int Trigger::getRepeatType()
{
    return _repeatType;
}

// @00573a34
void Trigger::setRepeatType(int repeatType)
{
    _repeatType = repeatType;
}

// @00573a3c
float Trigger::getRepeatFrames()
{
    return _repeatFrames;
}

// @00573a44
void Trigger::setRepeatFrames(float repeatFrames)
{
    _repeatFrames = repeatFrames;
}

// @00573a4c
float Trigger::getRepeatCount()
{
    return _repeatCount;
}

// @00573a54
void Trigger::setRepeatCount(float repeatCount)
{
    _repeatCount = repeatCount;
}

// @00573a5c
int Trigger::getSoundLocation()
{
    return _soundLocation;
}

// @00573a64
bool Trigger::getDisabled()
{
    return _disabled;
}

// @00573a6c
void Trigger::setDisabled(bool disabled)
{
    if (!_disabled)
    {
        if (disabled)
        {
            removeBeginContact(_sensor);
            removeEndContact(_sensor);
            _triggeringBody = nullptr;
            _triggeringCount = 0;
            _repeatCount = _repeatFrames;
            removeFromSingleAction();
            removeFromActions();
        }
    }
    else if (!disabled && !_activationDictionaryNulled)
    {
        if (static_cast<unsigned int>(_triggeredBy - 1) < 4)
        {
            addToBeginContact(_sensor);
        }
        if (_sensor != nullptr)
        {
            // Re-apply the unchanged filter to re-run contact filtering on the sensor.
            b2Filter filter = _sensor->GetFilterData();
            _sensor->SetFilterData(filter);
            _sensor->Refilter();
        }
    }
    _disabled = disabled;
}

// @00573b5c
b2Fixture* Trigger::getSensor()
{
    return _sensor;
}

// @00573b64
void Trigger::setSensor(b2Fixture* sensor)
{
    _sensor = sensor;
}

// @00573b6c
TargetAction* Trigger::addTargetActionShape(Sprite* refSprite, b2Fixture* fixture, Sprite* sprite,
                                            int action, std::vector<float> properties)
{
    TargetAction* targetAction =
        TargetAction::create(refSprite, fixture, sprite, action, properties);
    targetAction->retain();
    _targets.push_back(targetAction);
    return targetAction;
}

// @00573e54
TargetActionGroup* Trigger::addTargetActionGroup(GroupItem* groupItem, b2Fixture* fixture,
                                                 int action, std::vector<float> properties)
{
    TargetActionGroup* targetAction =
        TargetActionGroup::create(groupItem, fixture, action, properties);
    targetAction->retain();
    _targets.push_back(targetAction);
    return targetAction;
}

// @00574134
TargetActionSpecial* Trigger::addTargetItemSpecial(LevelItem* levelItem, b2Fixture* fixture,
                                                   Sprite* sprite, int action,
                                                   std::vector<float> properties)
{
    // sprite is unused.
    TargetActionSpecial* targetAction =
        TargetActionSpecial::create(levelItem, this, fixture, action, properties);
    targetAction->retain();
    _targets.push_back(targetAction);
    return targetAction;
}

// @00574418
TargetActionRevJoint* Trigger::addTargetActionRevJoint(b2RevoluteJoint* joint, int action,
                                                       std::vector<float> properties)
{
    TargetActionRevJoint* targetAction = TargetActionRevJoint::create(joint, action, properties);
    targetAction->retain();
    _targets.push_back(targetAction);
    return targetAction;
}

// @005746f0
TargetActionPrisJoint* Trigger::addTargetActionPrisJoint(b2PrismaticJoint* joint, int action,
                                                         std::vector<float> properties)
{
    TargetActionPrisJoint* targetAction = TargetActionPrisJoint::create(joint, action, properties);
    targetAction->retain();
    _targets.push_back(targetAction);
    return targetAction;
}

// @005749c8
TargetActionTrigger* Trigger::addTargetActionTrigger(Sprite* refSprite, Trigger* sourceTrigger,
                                                     Trigger* targetTrigger, int action,
                                                     std::vector<float> properties)
{
    TargetActionTrigger* targetAction =
        TargetActionTrigger::create(refSprite, sourceTrigger, targetTrigger, action, properties);
    targetAction->retain();
    _targets.push_back(targetAction);
    return targetAction;
}

// @00574cb0
TargetAction* Trigger::addTargetActionShapeItem(ShapeItem* shapeItem, b2Fixture* fixture,
                                                Sprite* sprite, int action,
                                                std::vector<float> properties)
{
    TargetAction* targetAction =
        TargetAction::create(shapeItem, fixture, sprite, action, properties);
    targetAction->retain();
    _targets.push_back(targetAction);
    return targetAction;
}

// @00574f98
void Trigger::activateByTrigger()
{
    if (_disabled)
    {
        return;
    }

    switch (_repeatType)
    {
        case TriggerRepeatTypeOnce:
            if (!_activationDictionaryNulled)
            {
                const float delayTime = _delayTime;
                LevelB2D* level = getLevel();
                if (delayTime == 0.0f)
                {
                    if (!level->singleActionsContainsLevelItem(this))
                    {
                        removeBeginContact(_sensor);
                        _activationDictionary.clear();
                        _activationDictionaryNulled = true;
                        singleAction();
                    }
                }
                else if (!level->actionsContainsLevelItem(this))
                {
                    if (_sensor != nullptr)
                    {
                        removeBeginContact(_sensor);
                    }
                    _activationDictionary.clear();
                    _activationDictionaryNulled = true;
                    getLevel()->addToActions(this);
                    _delayVector.push_back(0.0f);
                }
            }
            break;

        case TriggerRepeatTypeEachTime:
            if (_delayTime == 0.0f)
            {
                singleAction();
            }
            else
            {
                getLevel()->addToActions(this);
                _delayVector.push_back(0.0f);
            }
            break;

        case TriggerRepeatTypeContinuous:
            if (_triggeringBody == nullptr)
            {
                const float delayTime = _delayTime;
                LevelB2D* level = getLevel();
                if (delayTime == 0.0f)
                {
                    if (!level->singleActionsContainsLevelItem(this))
                    {
                        singleAction();
                    }
                }
                else
                {
                    level->addToActions(this);
                    _delayVector.push_back(0.0f);
                }
            }
            break;

        case TriggerRepeatTypeContinuousForever:
            if (_triggeringBody == nullptr)
            {
                // The level body stands in as the "body inside" so actions() keeps repeating.
                _triggeringBody = getLevelBody();
                getLevel()->addToActions(this);
            }
            break;
    }
}

// @0057532c
void Trigger::singleAction()
{
    const int type = _type;
    if (type == TriggerTypeTargets)
    {
        for (unsigned int i = 0; i < _targets.size(); i++)
        {
            TargetActionBase* target = _targets[i];
            switch (target->_targetActionType)
            {
                case TargetActionBaseTypeShape:
                case TargetActionBaseTypeSpecial:
                case TargetActionBaseTypeGroup:
                case TargetActionBaseTypeRevJoint:
                case TargetActionBaseTypePrisJoint:
                    if (target->_instant)
                    {
                        target->singleAction();
                    }
                    else
                    {
                        getLevel()->addToActions(target);
                    }
                    break;
                case TargetActionBaseTypeTrigger:
                    target->singleAction();
                    break;
            }
        }
    }
    else if (type == TriggerTypeSound)
    {
        std::string soundName =
            Settings::getInstance()->getSoundController()->soundFileName(_soundId);
        if (_soundLocation == 1)
        {
            SoundController::playSound(soundName, _volume, 1.0f, _panning);
        }
        else
        {
            createPositionSound(soundName, Vec2(_xMeters, _yMeters), 1.0f, false);
        }
    }
    else if (type == TriggerTypeFinish)
    {
        if (!getSession()->getIsReplay() && !getLevel()->getCharacter()->getDead())
        {
            getLevel()->levelCompleted();
        }
    }
    else if (type == TriggerTypeSlowMotion)
    {
        getSession()->slowMotion(10.0f);
    }
    else if (type == TriggerTypeSlowMotionStop)
    {
        getSession()->slowMotionStop();
    }
    else if (type == TriggerTypeSystem)
    {
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("system_trigger",
                                                                           &_systemTriggerId);
    }

    if (_repeatType == TriggerRepeatTypeOnce && _sensor != nullptr)
    {
        getLevelBody()->DestroyFixture(_sensor);
        _sensor = nullptr;
    }
}

// @00575610
void Trigger::actions()
{
    // Delayed activations: each entry counts up to _delayTime, then fires and is removed.
    std::vector<float>::iterator it = _delayVector.begin();
    while (it != _delayVector.end())
    {
        const float elapsed = *it;
        if (elapsed < _delayTime)
        {
            *it = elapsed + getTimeStep();
            ++it;
        }
        else
        {
            singleAction();
            it = _delayVector.erase(it);
        }
    }

    if (_repeatType < TriggerRepeatTypeContinuous || _triggeringBody == nullptr)
    {
        if (_delayVector.empty())
        {
            removeFromActions();
        }
        return;
    }

    // Continuous repeat while a body is inside.
    if (_repeatCount < _repeatFrames)
    {
        _repeatCount += getTimeStep();
        return;
    }
    if (_delayTime == 0.0f)
    {
        singleAction();
    }
    else
    {
        _delayVector.push_back(0.0f);
    }
    _repeatCount = 0.0f;
}
