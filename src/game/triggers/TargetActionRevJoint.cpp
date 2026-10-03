#include "TargetActionRevJoint.h"

#include "DestructionListener.h"
#include "LevelB2D.h"
#include "Session.h"

USING_NS_CC;

// @00571988
TargetActionRevJoint::TargetActionRevJoint()
    : _properties()
    , _actionIndex(0)
    , _joint(nullptr)
    , _counter(0.0f)
{
}

// @005719d4 (D1), @00571a14 (D0)
TargetActionRevJoint::~TargetActionRevJoint()
{
}

// @00571a38
TargetActionRevJoint* TargetActionRevJoint::create(b2RevoluteJoint* joint, int action,
                                                   std::vector<float> properties)
{
    // Plain (throwing) new, no null check; the result of init is ignored (init inlined).
    TargetActionRevJoint* targetAction = new TargetActionRevJoint();
    targetAction->initWithJoint(joint, action, properties);
    targetAction->autorelease();
    return targetAction;
}

// @00571c14
bool TargetActionRevJoint::initWithJoint(b2RevoluteJoint* joint, int action,
                                         std::vector<float> properties)
{
    _joint = joint;
    _targetActionType = TargetActionBaseTypeRevJoint;
    _actionIndex = action;
    _properties = properties;
    _instant = (action != 1);
    getSession()->getDestructionListener()->addJointListener(_joint, this);
    return true;
}

// @00571c80
void TargetActionRevJoint::jointWillBeDestroyed(b2Joint* joint)
{
    _joint = nullptr;
}

// @00571c88
void TargetActionRevJoint::singleAction()
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
                getWorld()->DestroyJoint(_joint);
                _joint = nullptr;
                getLevel()->updateTargetActionRevJoint(_index, nullptr, this);
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

        case 4:  // change limits: _properties = {lower, upper} in degrees
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
                float lower = _properties[0] * 0.017453292f;
                float upper = _properties[1] * 0.017453292f;
                LevelB2D* level = getLevel();
                level->convertRotationData(&lower);
                level->convertRotationData(&upper);
                _joint->SetLimits(lower, upper);
            }
            break;

        default:  // 1 (motor speed ramp) runs in actions()
            break;
    }
}

// @00571e6c
void TargetActionRevJoint::actions()
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
        float targetSpeed = _properties[0];
        getLevel()->convertRotationData(&targetSpeed);
        const float duration = _properties[1];
        if (_counter >= duration)
        {
            _counter = 0.0f;
            _joint->SetMotorSpeed(targetSpeed);
            getLevel()->removeFromActions(this);
            return;
        }
        const float speed = _joint->GetMotorSpeed();
        _joint->SetMotorSpeed(speed + (targetSpeed - speed) / ((duration - _counter) / getTimeStep()));
    }
    _counter += getTimeStep();
}

// @00571fb8
void TargetActionRevJoint::updateTargetActionForJoint(b2Joint* joint)
{
    _joint = static_cast<b2RevoluteJoint*>(joint);
}
