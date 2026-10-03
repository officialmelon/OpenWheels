#include "TargetActionSpecial.h"

#include "LevelB2D.h"
#include "Trigger.h"

USING_NS_CC;

// @00571fc0
TargetActionSpecial::TargetActionSpecial()
    : _properties()
    , _counter(0.0f)
    , _trigger(nullptr)
    , _levelItem(nullptr)
    , _shape(nullptr)
    , _actionIndex(0)
{
}

// @00572010 (D1), @00572050 (D0)
TargetActionSpecial::~TargetActionSpecial()
{
}

// @00572074
TargetActionSpecial* TargetActionSpecial::create(LevelItem* levelItem, Trigger* trigger,
                                                 b2Fixture* fixture, int action,
                                                 std::vector<float> properties)
{
    // Plain (throwing) new, no null check; the result of init is ignored (init inlined).
    TargetActionSpecial* targetAction = new TargetActionSpecial();
    targetAction->initWithSpecial(levelItem, trigger, fixture, action, properties);
    targetAction->autorelease();
    return targetAction;
}

// @00572250
bool TargetActionSpecial::initWithSpecial(LevelItem* levelItem, Trigger* trigger,
                                          b2Fixture* fixture, int action,
                                          std::vector<float> properties)
{
    _shape = fixture;
    _actionIndex = action;
    _targetActionType = TargetActionBaseTypeSpecial;
    _trigger = trigger;
    _levelItem = levelItem;
    _properties = properties;
    _instant = (_actionIndex != 3);
    return true;
}

// @005722a8
void TargetActionSpecial::singleAction()
{
    if (_levelItem != nullptr)
    {
        _levelItem->triggerSingleActivation(_trigger, _actionIndex, _properties);
    }
}

// @00572434
void TargetActionSpecial::updateTargetActionsSpecial(LevelItem* levelItem)
{
}

// @00572438
void TargetActionSpecial::actions()
{
    // No null check on _levelItem here (unlike singleAction).
    if (_levelItem->triggerRepeatActivation(_trigger, _actionIndex, _properties, _counter))
    {
        getLevel()->removeFromActions(this);
    }
    _counter += getTimeStep();
}
