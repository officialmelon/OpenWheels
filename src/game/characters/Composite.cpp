#include "Composite.h"

USING_NS_CC;

// @005a5630
Composite* Composite::create(const std::string& fileName, float timeStep)
{
    Composite* ret = new (std::nothrow) Composite();
    if (ret && ret->initWithFileName(fileName, timeStep))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

// @005a56f8
bool Composite::initWithFileName(const std::string& fileName, float timeStep)
{
    bool ok = Node::init();
    if (ok)
    {
        _timeStepInverse = 1.0f / timeStep;
    }
    return ok;
}

// @005a5738
std::vector<b2DistanceJoint*> Composite::getDistanceJoints()
{
    return _distanceJoints;
}

// @005a5834
void Composite::checkDistJoint(b2DistanceJoint* joint)
{
    if (joint)
    {
        // The force is computed and dropped (the call is the whole function body).
        joint->GetReactionForce(_timeStepInverse);
    }
}

// @005a5850
void Composite::timeStepChanged(float timeStepInverse)
{
    float limit = (timeStepInverse * 30.0f) / 60.0f;
    _breakLimitSquared = limit * limit;
}
