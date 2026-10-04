// ONLINE (PC addition): the character side of browser user vehicles (Flash CharacterB2D:
// checkKeyStates -> userVehicle.operateKeys, grabAction, userVehicleEject, the arm-break and death
// ejections, armsForwardPose / armsOverheadPose / holdPositionPose). See UserVehicle.h.
//
// Per-character state (Flash userVehicle, vehicleArm1Joint, vehicleArm2Joint) lives in the
// level's UserVehicleRider record instead of new CharacterB2D fields.
#include "CharacterB2D.h"

#include "cocos2d.h"

#include "DestructionListener.h"
#include "Session.h"
#include "online/vehicles/UserVehicle.h"

USING_NS_CC;

namespace {
// Flash currentPose values set from Vehicle.characterPose 1..3.
const int kPoseArmsForward = 10;
const int kPoseArmsOverhead = 11;
const int kPoseHoldPosition = 12;

void holdJoint(b2RevoluteJoint* joint)
{
    if (!joint->IsMotorEnabled()) {
        joint->EnableMotor(true);
    }
    joint->SetMotorSpeed(0.0f);
}
}  // namespace

bool CharacterB2D::onlineDriveUserVehicle(unsigned char state)
{
    online::UserVehicleRider* rider = online::userVehicleRider(this, false);
    if (!rider || !rider->vehicle) {
        return false;
    }
    rider->vehicle->operateKeys(Director::getInstance()->getTotalFrames(), state);
    return true;
}

// Flash grabAction for a shape whose user data is a Vehicle: the hand is pinned to it exactly like
// an ordinary grip (same joint), but as the vehicle arm joint, and the character becomes a rider.
// A hand touching another vehicle while already riding one grips it normally.
bool CharacterB2D::onlineGrabUserVehicle(int hand, b2Fixture* handle)
{
    online::UserVehicle* vehicle = online::userVehicleForHandle(handle);
    if (!vehicle) {
        return false;
    }
    online::UserVehicleRider* rider = online::userVehicleRider(this, true);
    if (!rider || (rider->vehicle && rider->vehicle != vehicle)) {
        return false;
    }
    b2Body* armBody = hand == 1 ? _lowerArm1Body : _lowerArm2Body;
    b2Fixture* armFixture = hand == 1 ? _lowerArm1Fixture : _lowerArm2Fixture;
    b2Body* otherBody = handle->GetBody();
    b2RevoluteJointDef jointDef;
    b2PolygonShape* shape = (b2PolygonShape*)armFixture->GetShape();
    b2Vec2 anchor = armBody->GetWorldPoint(b2Vec2(0.0f, -shape->m_vertices[2].y));
    jointDef.enableLimit = otherBody->GetType() != b2_staticBody;
    jointDef.maxMotorTorque = 4.0f;
    jointDef.Initialize(otherBody, armBody, anchor);
    if (hand == 1) {
        openHand1(false);
    } else {
        openHand2(false);
    }
    b2RevoluteJoint* joint = (b2RevoluteJoint*)getWorld()->CreateJoint(&jointDef);
    getSession()->getDestructionListener()->addJointListener(joint, this);
    removePostSolve(armFixture);
    if (hand == 1) {
        rider->arm1Joint = joint;
    } else {
        rider->arm2Joint = joint;
    }
    rider->vehicle = vehicle;
    vehicle->addCharacter(this);
    cocos2d::log("online: user vehicle grabbed (hand %d)", hand);
    setCurrentPose(CharacterPoseNone);
    switch (vehicle->getCharacterPose()) {
    case 1: setCurrentPose((CharacterPose)kPoseArmsForward); break;
    case 2: setCurrentPose((CharacterPose)kPoseArmsOverhead); break;
    case 3: setCurrentPose((CharacterPose)kPoseHoldPosition); break;
    default: break;
    }
    return true;
}

// Flash userVehicleEject: leave the vehicle, drop both vehicle arm joints, release any grip.
void CharacterB2D::onlineUserVehicleEject()
{
    online::UserVehicleRider* rider = online::userVehicleRider(this, false);
    if (!rider || !rider->vehicle) {
        return;
    }
    online::UserVehicle* vehicle = rider->vehicle;
    rider->vehicle = nullptr;
    vehicle->removeCharacter(this);
    cocos2d::log("online: user vehicle ejected");
    b2RevoluteJoint** joints[2] = {&rider->arm1Joint, &rider->arm2Joint};
    b2Body* arms[2] = {_lowerArm1Body, _lowerArm2Body};
    for (int i = 0; i < 2; i++) {
        if (*joints[i]) {
            getSession()->getDestructionListener()->removeJointListener(this, *joints[i]);
            getWorld()->DestroyJoint(*joints[i]);
            *joints[i] = nullptr;
            if (arms[i] && arms[i]->GetFixtureList()) {
                arms[i]->GetFixtureList()->Refilter();
            }
        }
    }
    endGrab();  // Flash releaseGrip
    setCurrentPose(CharacterPoseNone);
}

// Flash shoulderBreak/elbowBreak drop that arm's vehicle joint (ejecting when it was the last);
// set dead ejects.
void CharacterB2D::onlineUserVehicleInjury(CharacterInjury injury)
{
    online::UserVehicleRider* rider = online::userVehicleRider(this, false);
    if (!rider || !rider->vehicle) {
        return;
    }
    b2RevoluteJoint** lost = nullptr;
    b2RevoluteJoint* other = nullptr;
    switch (injury) {
    case CharacterInjuryShoulder1Break:
    case CharacterInjuryElbow1Break:
        lost = &rider->arm1Joint;
        other = rider->arm2Joint;
        break;
    case CharacterInjuryShoulder2Break:
    case CharacterInjuryElbow2Break:
        lost = &rider->arm2Joint;
        other = rider->arm1Joint;
        break;
    case CharacterInjuryDeath:
        onlineUserVehicleEject();
        return;
    default:
        return;
    }
    if (*lost) {
        getSession()->getDestructionListener()->removeJointListener(this, *lost);
        getWorld()->DestroyJoint(*lost);
        *lost = nullptr;
        if (!other) {
            onlineUserVehicleEject();
        }
    }
}

// Flash armsForwardPose / armsOverheadPose / holdPositionPose, with the mobile port's conventions
// for setJoint (mirrored angle, half the gain, max speed 20) and its broken-limb guards.
void CharacterB2D::onlineUserVehiclePose()
{
    switch ((int)_currentPose) {
    case kPoseArmsForward:
    case kPoseArmsOverhead: {
        const bool forward = (int)_currentPose == kPoseArmsForward;
        const float shoulder = forward ? -1.5f : 0.0f;
        const float elbow = forward ? -2.0f : -2.5f;
        if (_neckJoint) {
            setJoint(_neckJoint, -0.5f, 1.0f, 20.0f);
        }
        if (_shoulderJoint1) {
            setJoint(_shoulderJoint1, shoulder, 10.0f, 20.0f);
            if (_elbowJoint1 && !_upperArm3Body) {
                setJoint(_elbowJoint1, elbow, 7.5f, 20.0f);
            }
        }
        if (_shoulderJoint2) {
            setJoint(_shoulderJoint2, shoulder, 10.0f, 20.0f);
            if (_elbowJoint2 && !_upperArm4Body) {
                setJoint(_elbowJoint2, elbow, 7.5f, 20.0f);
            }
        }
        break;
    }
    case kPoseHoldPosition: {
        b2RevoluteJoint* joints[] = {_neckJoint,   _hipJoint1,      _hipJoint2,
                                     _kneeJoint1,  _kneeJoint2,     _shoulderJoint1,
                                     _shoulderJoint2, _elbowJoint1, _elbowJoint2};
        for (b2RevoluteJoint* joint : joints) {
            if (joint) {
                holdJoint(joint);
            }
        }
        break;
    }
    default:
        break;
    }
}

void CharacterB2D::onlineUserVehicleJointDestroyed(b2Joint* joint)
{
    online::UserVehicleRider* rider = online::userVehicleRider(this, false);
    if (!rider) {
        return;
    }
    if (rider->arm1Joint == joint) {
        rider->arm1Joint = nullptr;
    } else if (rider->arm2Joint == joint) {
        rider->arm2Joint = nullptr;
    } else {
        return;
    }
    // The vehicle (or the arm) went away: stop riding without touching Box2D any further.
    if (!rider->arm1Joint && !rider->arm2Joint && rider->vehicle) {
        online::UserVehicle* vehicle = rider->vehicle;
        rider->vehicle = nullptr;
        vehicle->removeCharacter(this);
        setCurrentPose(CharacterPoseNone);
    }
}
