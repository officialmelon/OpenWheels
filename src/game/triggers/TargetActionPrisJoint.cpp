#include "TargetActionPrisJoint.h"

#include "DestructionListener.h"
#include "LevelB2D.h"
#include "Session.h"
#include "online/FlashRuntime.h"  // ONLINE (PC addition)

USING_NS_CC;

// @0057139c
TargetActionPrisJoint::TargetActionPrisJoint()
    : _properties()
    , _actionIndex(-1)
    , _joint(nullptr)
    , _counter(0.0f)
{
}

// @005713ec (D1), @0057142c (D0)
TargetActionPrisJoint::~TargetActionPrisJoint()
{
}

// @00571450
TargetActionPrisJoint* TargetActionPrisJoint::create(b2PrismaticJoint* joint, int action,
                                                     std::vector<float> properties)
{
    // Plain (throwing) new, no null check; the result of init is ignored (init inlined).
    TargetActionPrisJoint* targetAction = new TargetActionPrisJoint();
    targetAction->initWithJoint(joint, action, properties);
    targetAction->autorelease();
    return targetAction;
}

// @00571630
bool TargetActionPrisJoint::initWithJoint(b2PrismaticJoint* joint, int action,
                                          std::vector<float> properties)
{
    _targetActionType = TargetActionBaseTypePrisJoint;
    _joint = joint;
    _actionIndex = action;
    _properties = properties;
    _instant = (_actionIndex != 1);
    // Registered for joint destruction, but this class does not override jointWillBeDestroyed
    // (only the non-virtual jointWillBeDestroy below exists), so _joint is never cleared.
    getSession()->getDestructionListener()->addJointListener(_joint, this);
    return true;
}

// @0057169c
void TargetActionPrisJoint::jointWillBeDestroy()
{
    _joint = nullptr;
}

// @005716a4
void TargetActionPrisJoint::singleAction()
{
    switch (_actionIndex)
    {
        case 0:  // disable motor
            if (_joint != nullptr)
            {
                _joint->EnableMotor(false);
            }
            break;

        case 2:  // delete joint
            if (_joint != nullptr)
            {
                b2Joint* joint = _joint;
                if (online::flashLevel())
                {
                    // ONLINE (PC addition): tell the items holding this joint (an NPC's user
                    // joints, the other trigger actions on it) before it goes, as
                    // b2World::DestroyBody does for the joints it destroys.
                    getSession()->getDestructionListener()->SayGoodbye(joint);
                }
                getWorld()->DestroyJoint(joint);
                _joint = nullptr;
                getLevel()->updateTargetActionPrisJoint(_index, nullptr, this);
            }
            break;

        case 3:  // disable limits
            if (_joint != nullptr)
            {
                if (!_joint->GetBodyA()->IsAwake())
                {
                    _joint->GetBodyA()->SetAwake(true);
                }
                if (!_joint->GetBodyB()->IsAwake())
                {
                    _joint->GetBodyB()->SetAwake(true);
                }
                _joint->EnableLimit(false);
            }
            break;

        case 4:  // change limits: _properties = {upper, lower}
            if (_joint != nullptr)
            {
                if (!_joint->GetBodyA()->IsAwake())
                {
                    _joint->GetBodyA()->SetAwake(true);
                }
                if (!_joint->GetBodyB()->IsAwake())
                {
                    _joint->GetBodyB()->SetAwake(true);
                }
                LevelB2D* level = getLevel();
                float upper = _properties[0];
                float lower = _properties[1];
                level->convertLengthData(&upper);
                level->convertLengthData(&lower);
                _joint->SetLimits(lower, upper);
                // ONLINE (PC addition): Flash also switches the limits on (levels > 1.84).
                if (online::flashLevel() && online::flashVersion() > 1.84f)
                {
                    _joint->EnableLimit(true);
                }
            }
            break;

        default:  // 1 (motor speed ramp) runs in actions()
            break;
    }
}

// @00571874
void TargetActionPrisJoint::actions()
{
    if (_actionIndex == 1 && _joint != nullptr)
    {
        // Motor speed ramp: _properties = {target speed, duration in seconds}.
        if (!_joint->GetBodyA()->IsAwake())
        {
            _joint->GetBodyA()->SetAwake(true);
        }
        if (!_joint->GetBodyB()->IsAwake())
        {
            _joint->GetBodyB()->SetAwake(true);
        }
        if (!_joint->IsMotorEnabled())
        {
            _joint->EnableMotor(true);
        }
        const float targetSpeed = _properties[0];
        const float duration = _properties[1];
        if (_counter >= duration)
        {
            // Unlike the other target actions the counter is left at the duration (sic).
            _counter = duration;
            // ONLINE (PC addition): Flash restarts the ramp counter (levels > 1.8), so the
            // next activation ramps again instead of jumping to the speed.
            if (online::flashLevel() && online::flashVersion() > 1.8f)
            {
                _counter = 0.0f;
            }
            _joint->SetMotorSpeed(targetSpeed);
            getLevel()->removeFromActions(this);
            return;
        }
        const float speed = _joint->GetMotorSpeed();
        _joint->SetMotorSpeed(speed + (targetSpeed - speed) / ((duration - _counter) / getTimeStep()));
    }
    _counter += getTimeStep();
}

// ONLINE (PC addition): in browser levels the joint reference is dropped when Box2D destroys the
// joint with one of its bodies (a deleted shape or group), as TargetActionRevJoint does; Flash
// keeps a harmless detached joint object there.
void TargetActionPrisJoint::jointWillBeDestroyed(b2Joint* joint)
{
    if (online::flashLevel() && joint == _joint)
    {
        _joint = nullptr;
    }
}

// @00571980
void TargetActionPrisJoint::updateTargetActionForJoint(b2Joint* joint)
{
    _joint = static_cast<b2PrismaticJoint*>(joint);
}
