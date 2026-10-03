#pragma once

// TargetActionGroup: trigger action on a GroupItem's body. sizeof 0xd8 (arm64).

#include <vector>

#include "TargetActionBase.h"

class GroupItem;

class TargetActionGroup : public TargetActionBase
{
public:
    TargetActionGroup();
    virtual ~TargetActionGroup();

    static TargetActionGroup* create(GroupItem* groupItem, b2Fixture* fixture, int action,
                                     std::vector<float> properties);
    bool initWithGroupItem(GroupItem* groupItem, b2Fixture* fixture, int action,
                           std::vector<float> properties);

    // _actionIndex: 0 wake, 1 fade (actions()), 2 impulse, 3 make static, 4 make dynamic,
    // 5 stop interactivity, 6 delete, 7 change collision.
    virtual void singleAction() override;
    virtual void actions() override;
    void updateTargetActionsForGroupItem(GroupItem* groupItem);

protected:
    // Names from the iOS original's TargetActionGroup ivars.
    std::vector<float> _properties;  // +0xa0
    float _counter;                  // +0xb8
    GroupItem* _groupItem;           // +0xc0
    b2Fixture* _shape;               // +0xc8
    int _actionIndex;                // +0xd0 (-1)
};
