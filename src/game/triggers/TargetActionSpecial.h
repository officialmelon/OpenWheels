#pragma once

// TargetActionSpecial: forwards a trigger to a LevelItem's own trigger hooks
// (triggerSingleActivation / triggerRepeatActivation). sizeof 0xe0 (arm64).

#include <vector>

#include "TargetActionBase.h"

class Trigger;

class TargetActionSpecial : public TargetActionBase
{
public:
    TargetActionSpecial();
    virtual ~TargetActionSpecial();

    static TargetActionSpecial* create(LevelItem* levelItem, Trigger* trigger, b2Fixture* fixture,
                                       int action, std::vector<float> properties);
    bool initWithSpecial(LevelItem* levelItem, Trigger* trigger, b2Fixture* fixture, int action,
                         std::vector<float> properties);

    virtual void singleAction() override;  // item->triggerSingleActivation(trigger, action, props)
    void updateTargetActionsSpecial(LevelItem* levelItem);  // empty in 1.1.3
    virtual void actions() override;       // item->triggerRepeatActivation(.., _counter) -> done

protected:
    // Names from the iOS original's TargetActionSpecial ivars.
    std::vector<float> _properties;  // +0xa0
    float _counter;                  // +0xb8
    Trigger* _trigger;               // +0xc0
    LevelItem* _levelItem;           // +0xc8
    b2Fixture* _shape;               // +0xd0
    int _actionIndex;                // +0xd8 (_instant = actionIndex != 3)
};
