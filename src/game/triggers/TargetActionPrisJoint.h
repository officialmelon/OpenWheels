#pragma once

// TargetActionPrisJoint: trigger action on a prismatic joint. sizeof 0xd0 (arm64).
// Note: it registers with the DestructionListener but does NOT override jointWillBeDestroyed;
// it has a non-virtual jointWillBeDestroy() (sic) instead.

#include <vector>

#include "TargetActionBase.h"

class TargetActionPrisJoint : public TargetActionBase
{
public:
    TargetActionPrisJoint();
    virtual ~TargetActionPrisJoint();

    static TargetActionPrisJoint* create(b2PrismaticJoint* joint, int action,
                                         std::vector<float> properties);
    bool initWithJoint(b2PrismaticJoint* joint, int action, std::vector<float> properties);
    void jointWillBeDestroy();

    // _actionIndex: 0 disable motor, 1 motor speed ramp (actions()), 2 delete joint, 3 disable limits,
    // 4 change limits.
    virtual void singleAction() override;
    virtual void actions() override;
    void updateTargetActionForJoint(b2Joint* joint);

protected:
    // Names from the iOS original's ivars.
    std::vector<float> _properties;  // +0xa0
    int _actionIndex;                // +0xb8 (-1)
    b2PrismaticJoint* _joint;        // +0xc0
    float _counter;                  // +0xc8
};
