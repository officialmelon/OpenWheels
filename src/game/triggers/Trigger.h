#pragma once

// Trigger: level trigger zone (box sensor on the level body) or trigger-activated trigger.
// Fires its TargetActionBase list, a sound, level completion, slow motion or a
// "system_trigger" custom event, once / each time / continuously, optionally delayed.
// LevelItem subclass, sizeof 0x150 (arm64).

#include <map>
#include <string>
#include <vector>

#include "LevelItem.h"

namespace cocos2d {
class Sprite;
}

class GroupItem;
class ShapeItem;
class TargetAction;
class TargetActionBase;
class TargetActionGroup;
class TargetActionPrisJoint;
class TargetActionRevJoint;
class TargetActionSpecial;
class TargetActionTrigger;

// Trigger::_type ("t").
enum TriggerType
{
    TriggerTypeTargets = 1,          // run the target actions
    TriggerTypeSound = 2,            // play sound _soundId (global or positional)
    TriggerTypeFinish = 3,           // LevelB2D::levelCompleted (not in replays / when dead)
    TriggerTypeSlowMotion = 5000,    // Session::slowMotion(10) (empty in 1.1.3)
    TriggerTypeSlowMotionStop = 5001,
    TriggerTypeSystem = 10000,       // dispatch "system_trigger" with &_systemTriggerId
                                     // (StageCamera: 6 lead right, 7 lead left, 8/9 shake)
};

// Trigger::_triggeredBy ("b").
enum TriggerTriggeredBy
{
    TriggerTriggeredByCharacter = 1,      // the player's central body
    TriggerTriggeredByAnyCharacter = 2,   // any LevelItem with SpecialTypeCharacter
    TriggerTriggeredByAnyObject = 3,      // any non-sensor body with mass
    TriggerTriggeredByBodies = 4,         // bodies in _activationDictionary
    TriggerTriggeredByTrigger = 5,        // no sensor; TargetActionTrigger -> activateByTrigger()
};

// Trigger::_repeatType ("r").
enum TriggerRepeatType
{
    TriggerRepeatTypeOnce = 1,
    TriggerRepeatTypeEachTime = 2,
    TriggerRepeatTypeContinuous = 3,      // every _repeatFrames while the body stays inside
    TriggerRepeatTypeContinuousForever = 4,  // RE-TODO: name; keeps firing after activation
};

class Trigger : public LevelItem
{
public:
    Trigger();
    virtual ~Trigger();

    virtual bool init(LevelDataElement* element, b2Body* levelBody, b2Vec2 offset) override;
    void createShape(LevelDataElement* element);
    void addActivationBody(b2Body* body);
    void addActivationBodies(std::vector<b2Body*> bodies);
    virtual void beginContact(b2Fixture* fixture, b2Fixture* otherFixture,
                              b2Contact* contact) override;
    void checkAdd1(b2Fixture* fixture);
    void checkAdd2(b2Fixture* fixture);
    void checkAdd3(b2Fixture* fixture);
    void checkAdd4(b2Fixture* fixture);
    virtual void endContact(b2Fixture* fixture, b2Fixture* otherFixture,
                            b2Contact* contact) override;
    void addTriggeringBody(b2Body* body);

    float getXMeters();
    void setXMeters(float xMeters);
    float getYMeters();
    void setYMeters(float yMeters);
    float getVolume();
    void setVolume(float volume);
    float getPanning();
    void setPanning(float panning);
    int getType();
    void setType(int type);
    float getDelayTime();
    void setDelayTime(float delayTime);
    int getCounter();
    void setCounter(int counter);
    int getTriggeredBy();
    void setTriggeredBy(int triggeredBy);
    int getTriggeringCount();
    void setTriggeringCount(int triggeringCount);
    int getRepeatType();
    void setRepeatType(int repeatType);
    float getRepeatFrames();
    void setRepeatFrames(float repeatFrames);
    float getRepeatCount();
    void setRepeatCount(float repeatCount);
    int getSoundLocation();
    bool getDisabled();
    void setDisabled(bool disabled);
    b2Fixture* getSensor();
    void setSensor(b2Fixture* sensor);

    // Factories: create, retain and append to _targets; return the new action.
    TargetAction* addTargetActionShape(cocos2d::Sprite* refSprite, b2Fixture* fixture,
                                       cocos2d::Sprite* sprite, int action,
                                       std::vector<float> properties);
    TargetActionGroup* addTargetActionGroup(GroupItem* groupItem, b2Fixture* fixture, int action,
                                            std::vector<float> properties);
    TargetActionSpecial* addTargetItemSpecial(LevelItem* levelItem, b2Fixture* fixture,
                                              cocos2d::Sprite* sprite, int action,
                                              std::vector<float> properties);
    TargetActionRevJoint* addTargetActionRevJoint(b2RevoluteJoint* joint, int action,
                                                  std::vector<float> properties);
    TargetActionPrisJoint* addTargetActionPrisJoint(b2PrismaticJoint* joint, int action,
                                                    std::vector<float> properties);
    TargetActionTrigger* addTargetActionTrigger(cocos2d::Sprite* refSprite, Trigger* sourceTrigger,
                                                Trigger* targetTrigger, int action,
                                                std::vector<float> properties);
    TargetAction* addTargetActionShapeItem(ShapeItem* shapeItem, b2Fixture* fixture,
                                           cocos2d::Sprite* sprite, int action,
                                           std::vector<float> properties);

    void activateByTrigger();
    virtual void singleAction() override;
    virtual void actions() override;

protected:
    // Names from the iOS original's Trigger ivars.
    b2Body* _triggeringBody;                         // +0x098 body currently inside
    b2Fixture* _sensor;                              // +0x0a0 box sensor on the level body
    std::string _soundEffect;                        // +0x0a8 assigned "" in init, else unused
    int _soundId;                                    // +0x0c0 "s" (SoundController index), -1
    std::vector<TargetActionBase*> _targets;         // +0x0c8 retained
    std::map<b2Body*, unsigned int> _activationDictionary;  // +0x0e0 body -> 1 (triggeredBy 4)
    std::vector<float> _delayVector;                 // +0x0f8 pending delayed activations
    int _systemTriggerId;                            // +0x110 "i" for TriggerTypeSystem, -1
    float _xMeters;                                  // +0x114 "x"
    float _yMeters;                                  // +0x118 "y"
    float _volume;                                   // +0x11c "v"
    float _panning;                                  // +0x120 "p"
    float _delayTime;                                // +0x124 "d"
    int _type;                                       // +0x128 "t" TriggerType
    int _counter;                                    // +0x12c
    int _triggeredBy;                                // +0x130 "b" TriggerTriggeredBy
    int _triggeringCount;                            // +0x134 contacts of _triggeringBody
    int _repeatType;                                 // +0x138 "r" TriggerRepeatType
    float _repeatFrames;                             // +0x13c "i" (seconds between repeats)
    float _repeatCount;                              // +0x140 time since the last repeat
    int _soundLocation;                              // +0x144 "l": 1 global, else positional
    bool _disabled;                                  // +0x148 "sd" (set by LevelB2D)
    bool _activationDictionaryNulled;                // +0x149 one-shot trigger already fired
};
