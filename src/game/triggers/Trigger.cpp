#include "Trigger.h"

#include <cmath>

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
#include "Sound.h"
#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "online/items/NPCharacter.h"  // ONLINE (PC addition)
#include "online/items/NPCharacter.h"  // ONLINE (PC addition)

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
    _onlineClickSpent = false;  // ONLINE (PC addition)
    _onlineClickHover = false;

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
        if (online::flashLevel())
        {
            // ONLINE (PC addition): Flash trigger sensors are category 24 (8 | 16), so shapes that
            // only collide with "fixed" / category-16 shapes (collision 5 and 6) still enter them.
            fixtureDef.filter.categoryBits = 24;
        }

        float width = 0.0f;
        float height = 0.0f;
        float angle = 0.0f;
        element->floatAttribute("w", &width);
        element->floatAttribute("h", &height);
        element->floatAttribute("a", &angle);
        getLevel()->convertLengthData(&width);
        getLevel()->convertLengthData(&height);
        _onlineHalfWidth = width * 0.5f;  // ONLINE (PC addition): click-trigger button area
        _onlineHalfHeight = height * 0.5f;
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
    // ONLINE (PC addition): Flash checkAdd2 also accepts the central body of an NPCharacter.
    else if (item != nullptr && online::flashLevel() &&
             online::isNPCharacterCentralBody(item, fixture->GetBody()))
    {
        addTriggeringBody(fixture->GetBody());
    }
    // ONLINE (PC addition): Flash Trigger.checkAdd2 also accepts browser NPCs
    // (NPCharacter.centralBody).
    else if (item != nullptr && online::flashLevel() &&
             online::isNPCharacterCentralBody(item, fixture->GetBody()))
    {
        addTriggeringBody(fixture->GetBody());
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
            // ONLINE (PC addition): Flash Trigger.disabled = true also drops the pending delayed
            // activations (delayVector = new Vector) and the "continuously" click hover.
            if (online::flashLevel())
            {
                _delayVector.clear();
                _onlineClickHover = false;
            }
        }
    }
    else if (!disabled && !_activationDictionaryNulled)
    {
        if (static_cast<unsigned int>(_triggeredBy - 1) < 4)
        {
            addToBeginContact(_sensor);
        }
        if (_sensor != nullptr && online::flashLevel())
        {
            // ONLINE (PC addition): Flash's world.Refilter(sensor) (Box2D 2.0) recreates the
            // sensor's broad-phase proxy, so a body already inside an enabled trigger gets a fresh
            // contact "add" and fires it. Box2D 2.3's Refilter keeps touching contacts, so the
            // sensor fixture is rebuilt instead.
            b2FixtureDef fixtureDef;
            fixtureDef.shape = _sensor->GetShape();
            fixtureDef.isSensor = true;
            fixtureDef.filter = _sensor->GetFilterData();
            b2Fixture* oldSensor = _sensor;
            removeBeginContact(oldSensor);
            removeEndContact(oldSensor);
            _sensor = getLevelBody()->CreateFixture(&fixtureDef);
            getLevelBody()->DestroyFixture(oldSensor);
            if (static_cast<unsigned int>(_triggeredBy - 1) < 4)
            {
                addToBeginContact(_sensor);
            }
        }
        else if (_sensor != nullptr)
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
    if (online::flashLevel())
    {
        onlineActivateByTrigger();  // ONLINE (PC addition)
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

// ---------------------------------------------------------------------------------------------
// ONLINE (PC addition): Flash Trigger.activateByTrigger for converted browser levels. Differences
// to the mobile version above: "each time" without delay does nothing while the trigger already
// waits in the single-action vector, "continuously" with a delay only queues when the trigger is
// not already running, and the queue checks also see the items added this step (Flash pushes into
// actionsVector at once). Trigger chains with no delay recurse exactly as in Flash; Flash ends a
// runaway chain with a stack overflow that aborts the frame, here the recursion is cut at a depth
// Flash's stack would not reach either.
namespace {
int g_onlineActivationDepth = 0;
const int kOnlineMaxActivationDepth = 256;
}  // namespace

void Trigger::onlineActivateByTrigger()
{
    if (g_onlineActivationDepth >= kOnlineMaxActivationDepth)
    {
        return;
    }
    struct DepthGuard
    {
        DepthGuard() { g_onlineActivationDepth++; }
        ~DepthGuard() { g_onlineActivationDepth--; }
    } guard;

    LevelB2D* level = getLevel();
    const bool inSingle = level->singleActionsContainsLevelItem(this);
    const bool inActions = level->onlineActionsContainsLevelItem(this);
    switch (_repeatType)
    {
        case TriggerRepeatTypeOnce:
            if (_activationDictionaryNulled)
            {
                break;
            }
            if (_delayTime == 0.0f)
            {
                if (!inSingle)
                {
                    removeBeginContact(_sensor);
                    _activationDictionary.clear();
                    _activationDictionaryNulled = true;
                    singleAction();
                }
            }
            else if (!inActions)
            {
                removeBeginContact(_sensor);
                _activationDictionary.clear();
                _activationDictionaryNulled = true;
                level->addToActions(this);
                _delayVector.push_back(0.0f);
            }
            break;

        case TriggerRepeatTypeEachTime:
            if (_delayTime == 0.0f)
            {
                if (!inSingle)
                {
                    singleAction();
                }
            }
            else
            {
                level->addToActions(this);
                _delayVector.push_back(0.0f);
            }
            break;

        case TriggerRepeatTypeContinuous:
            if (_triggeringBody != nullptr)
            {
                break;
            }
            if (_delayTime == 0.0f)
            {
                if (!inSingle)
                {
                    singleAction();
                }
            }
            else if (!inActions)
            {
                level->addToActions(this);
                _delayVector.push_back(0.0f);
            }
            break;

        case TriggerRepeatTypeContinuousForever:
            if (_triggeringBody == nullptr)
            {
                _triggeringBody = getLevelBody();
                level->addToActions(this);
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
        if (online::flashLevel())
        {
            // ONLINE (PC addition): Flash plays the sound at the trigger's volume (0..1):
            // playSoundInstance(effect, 0, 0, SoundTransform(volume, panning)) or
            // playPointSoundInstance(effect, position, volume). Panning has no AudioEngine
            // equivalent and is dropped.
            if (_soundLocation == 1)
            {
                Sound::playSound(soundName, _volume, 1.0f, _panning, false);
            }
            else if (Sound* sound =
                         createPositionSound(soundName, Vec2(_xMeters, _yMeters), 1.0f, false))
            {
                sound->setMaxVolume(_volume);
            }
        }
        else if (_soundLocation == 1)
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
        // ONLINE (PC addition): a Flash victory trigger completes the level even when the
        // character is dead (Trigger.singleAction only checks isReplay).
        if (!getSession()->getIsReplay() &&
            (online::flashLevel() || !getLevel()->getCharacter()->getDead()))
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
    if (online::flashLevel())
    {
        onlineActions();  // ONLINE (PC addition)
        return;
    }
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

// ONLINE (PC addition): Flash Trigger.actions. The delay list is walked backwards by index as in
// Flash, so activations queued by the trigger's own targets (a trigger re-activating itself) or a
// "disable" clearing the list while it is walked are safe.
void Trigger::onlineActions()
{
    bool delaysDone = true;
    if (!_delayVector.empty())
    {
        for (int i = (int)_delayVector.size() - 1; i >= 0; i--)
        {
            if (i >= (int)_delayVector.size())
            {
                continue;  // the list was cleared (trigger disabled) by an earlier entry
            }
            const float elapsed = _delayVector[i];
            if (elapsed < _delayTime)
            {
                _delayVector[i] = elapsed + getTimeStep();
            }
            else
            {
                _delayVector.erase(_delayVector.begin() + i);
                singleAction();
            }
        }
        delaysDone = _delayVector.empty();
    }

    bool repeatDone = true;
    if (_repeatType > TriggerRepeatTypeEachTime && _triggeringBody != nullptr)
    {
        repeatDone = false;
        if (_repeatCount < _repeatFrames)
        {
            _repeatCount += getTimeStep();
        }
        else
        {
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
    }
    if (delaysDone && repeatDone)
    {
        removeFromActions();
    }
}

// ---------------------------------------------------------------------------------------------
// ONLINE (PC addition): mouse-click triggers of browser levels (Flash Trigger.as).

bool Trigger::onlineClickHit(const b2Vec2& worldPoint)
{
    // Flash's button hit area is the trigger box without its rotation (Trigger.createShape
    // copies x, y, scaleX, scaleY to the hit-area sprite, not the angle).
    return _triggeredBy == TriggerTriggeredByMouseClick && !_disabled && !_onlineClickSpent &&
           onlineInButton(worldPoint);
}

bool Trigger::onlineInButton(const b2Vec2& worldPoint)
{
    return std::fabs(worldPoint.x - _xMeters) <= _onlineHalfWidth &&
           std::fabs(worldPoint.y - _yMeters) <= _onlineHalfHeight;
}

// Flash mouseUpHandler.
void Trigger::onlineMouseClick()
{
    LevelB2D* level = getLevel();
    if (_repeatType > TriggerRepeatTypeEachTime)
    {
        if (!level->actionsContainsLevelItem(this))
        {
            level->addToActions(this);
            _triggeringBody = getLevelBody();
            _onlineClickHover = _repeatType == TriggerRepeatTypeContinuous;
        }
        return;
    }
    if (_repeatType == TriggerRepeatTypeOnce)
    {
        _onlineClickSpent = true;
    }
    if (_delayTime == 0.0f)
    {
        level->addToSingleActions(this);
    }
    else
    {
        _delayVector.push_back(0.0f);
        level->addToActions(this);
    }
}

// Flash mouseOutHandler (registered for "continuously" click triggers).
void Trigger::onlineMouseMove(const b2Vec2& worldPoint)
{
    if (!_onlineClickHover || onlineInButton(worldPoint))
    {
        return;
    }
    _onlineClickHover = false;
    if (_triggeringBody == getLevelBody())
    {
        _triggeringBody = nullptr;
    }
}
