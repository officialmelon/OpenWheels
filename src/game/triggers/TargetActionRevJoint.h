#pragma once

// TargetActionRevJoint: trigger action on a revolute joint. sizeof 0xd0 (arm64).

#include <vector>

#include "TargetActionBase.h"

class TargetActionRevJoint : public TargetActionBase
{
public:
    TargetActionRevJoint();
    virtual ~TargetActionRevJoint();

    static TargetActionRevJoint* create(b2RevoluteJoint* joint, int action,
                                        std::vector<float> properties);
    bool initWithJoint(b2RevoluteJoint* joint, int action, std::vector<float> properties);
    virtual void jointWillBeDestroyed(b2Joint* joint) override;  // _joint = nullptr

    virtual void singleAction() override;
    virtual void actions() override;
    void updateTargetActionForJoint(b2Joint* joint);

protected:
    // Names from the iOS original's ivars.
    std::vector<float> _properties;  // +0xa0
    int _actionIndex;                // +0xb8
    b2RevoluteJoint* _joint;         // +0xc0
    float _counter;                  // +0xc8
};
