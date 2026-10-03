#include "TargetActionTrigger.h"

#include "Trigger.h"

USING_NS_CC;

// @005725f0
TargetActionTrigger* TargetActionTrigger::create(Sprite* refSprite, LevelItem* sourceTrigger,
                                                 LevelItem* receivingTrigger, int targetAction,
                                                 std::vector<float> properties)
{
    // Plain (throwing) new with value-initialisation: the class has no user-declared constructor,
    // so the object is zero-filled before LevelItem() runs. No null check, init result ignored.
    TargetActionTrigger* action = new TargetActionTrigger();
    action->initWithRefSprite(refSprite, sourceTrigger, receivingTrigger, targetAction,
                              properties);
    action->autorelease();
    return action;
}

// @00572818
bool TargetActionTrigger::initWithRefSprite(Sprite* refSprite, LevelItem* sourceTrigger,
                                            LevelItem* receivingTrigger, int targetAction,
                                            std::vector<float> properties)
{
    _refSprite = refSprite;
    _sourceTrigger = sourceTrigger;
    _receivingTrigger = receivingTrigger;
    _targetActionType = TargetActionBaseTypeTrigger;
    _targetAction = targetAction;
    _instant = true;
    _properties = properties;
    return true;
}

// @00572860
void TargetActionTrigger::singleAction()
{
    Trigger* trigger = static_cast<Trigger*>(_receivingTrigger);
    switch (_targetAction)
    {
        case 0:
            trigger->activateByTrigger();
            break;
        case 1:
            trigger->setDisabled(true);
            break;
        case 2:
            trigger->setDisabled(false);
            break;
    }
}

// @0057289c (D2), @005728dc (D0)
TargetActionTrigger::~TargetActionTrigger()
{
}
