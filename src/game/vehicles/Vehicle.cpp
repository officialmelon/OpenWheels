#include "Vehicle.h"

#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "SoundController.h"

USING_NS_CC;

// Vehicle has no user-declared constructor/destructor (see Vehicle.h); the empty virtual hooks
// are defined inline in the header.

// @0063d308
bool Vehicle::init(Vec2 position, std::string name, int groupID)
{
    _name = name;
    _wheelSoundVolume = 0.5f;
    if (_wheelSoundName.empty())
    {
        _wheelSoundName = "TireLoop1";
    }
    Settings::getInstance()->getSoundController()->preloadSound(_wheelSoundName);

    _wheelSound = nullptr;
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
    _groupID = groupID;

    _zeroFilter.categoryBits = 0x0104;
    _zeroFilter.maskBits = 0xffff;
    _zeroFilter.groupIndex = 0;
    _defaultFilter.categoryBits = 0x0104;
    _defaultFilter.maskBits = 0x010e;
    _defaultFilter.groupIndex = (int16)groupID;

    _origin = position;
    loadBodies(_name);

    SessionMode mode = getSession()->getMode();
    std::string prefix = (mode == SessionModeCharacterSelect) ? "character_select_" : "";
    std::string plist = "vehicles/" + prefix + _name + "_sprites.plist";
    loadSpriteFrames(plist);

    createSprites();
    createFilters();
    createBodies();
    createFixtures();
    createJoints();
    setLimits();
    addContactListeners();
    createDictionaries();

    if (_frontWheelFixture)
    {
        addContactWheel(_frontWheelFixture);
    }
    if (_backWheelFixture)
    {
        addContactWheel(_backWheelFixture);
    }
    if (mode == SessionModeCharacterSelect)
    {
        lockWheels();
    }
    getLevel()->addToActions(this);
    return true;
}

// @0063d70c
void Vehicle::loadBodies(std::string name)
{
    std::string fileName = "vehicles/bodies/" + name + ".plist";
    std::string fullPath = FileUtils::getInstance()->fullPathForFilename(fileName);
    _bodiesDict = FileUtils::getInstance()->getValueMapFromFile(fullPath);
}

// @0063d89c
void Vehicle::addContactWheel(b2Fixture* fixture)
{
    addToBeginContact(fixture);
    addToEndContact(fixture);
}

// @0063d8c8
void Vehicle::addBodyVehicleJoint(b2Body* body, b2Joint* joint)
{
    _bodyVehicleJointDict[body] = joint;
}

// @0063d98c
void Vehicle::removeBodyVehicleJoint(b2Body* body)
{
    auto it = _bodyVehicleJointDict.find(body);
    if (it != _bodyVehicleJointDict.end())
    {
        getWorld()->DestroyJoint(_bodyVehicleJointDict[body]);
        _bodyVehicleJointDict.erase(it);
    }
}

// @0063db00
void Vehicle::addCharacter(CharacterB2D* character)
{
    _characters.push_back(character);
}

// @0063dc74
void Vehicle::ejectBtnPressed()
{
    ejectAllCharacters();
}

// @0063dc80
void Vehicle::ejectAllCharacters()
{
    // The original tests the element count truncated to 32 bits, then keeps ejecting the first
    // rider (ejectCharacter erases it) until the list is empty.
    if ((int)_characters.size() != 0)
    {
        auto it = _characters.begin();
        while (it != _characters.end())
        {
            ejectCharacter(*it);
        }
    }
}

// @0063dcd4
bool Vehicle::ejectCharacter(CharacterB2D* character)
{
    auto it = std::find(_characters.begin(), _characters.end(), character);
    if (it != _characters.end())
    {
        character->eject();
        destroyAllCharacterJoints(character);
        _characters.erase(it);
        return true;
    }
    return false;
}

// @0063dd70
bool Vehicle::checkRevJoint(b2Joint* joint, float limit)
{
    if (!joint)
    {
        return false;
    }
    b2Vec2 force = joint->GetReactionForce(s_timeStepInverse);
    if (force.Length() > limit)
    {
        return true;
    }
    b2Vec2 anchorA = joint->GetAnchorA();
    b2Vec2 anchorB = joint->GetAnchorB();
    b2Vec2 drift = anchorB - anchorA;
    return drift.LengthSquared() > 0.25f;
}

// @0063de20
void Vehicle::setJoint(b2RevoluteJoint* joint, float angle, float gain, float maxSpeed)
{
    if (!joint->IsMotorEnabled())
    {
        joint->EnableMotor(true);
    }
    float target = joint->GetUpperLimit() + angle;
    float diff = joint->GetJointAngle() - target;
    float distance = (diff < 0.0f) ? -diff : diff;
    float speed = b2Min(distance * distance * gain, maxSpeed);
    joint->SetMotorSpeed((diff < 0.0f) ? speed : -speed);
}

// @0063deb4
void Vehicle::destroyAllCharacterJoints(CharacterB2D* character)
{
    std::vector<b2Body*> bodies;
    if (character->getShoulderJoint1())
    {
        bodies.push_back(character->getUpperArm1Body());
        if (character->getElbowJoint1())
        {
            bodies.push_back(character->getLowerArm1Body());
        }
    }
    if (character->getShoulderJoint2())
    {
        bodies.push_back(character->getUpperArm2Body());
        if (character->getElbowJoint2())
        {
            bodies.push_back(character->getLowerArm2Body());
        }
    }
    if (character->getChestBody())
    {
        bodies.push_back(character->getChestBody());
    }
    bodies.push_back(character->getPelvisBody());
    if (character->getHipJoint1())
    {
        bodies.push_back(character->getUpperLeg1Body());
        if (character->getKneeJoint1())
        {
            bodies.push_back(character->getLowerLeg1Body());
        }
    }
    if (character->getHipJoint2())
    {
        bodies.push_back(character->getUpperLeg2Body());
        if (character->getKneeJoint2())
        {
            bodies.push_back(character->getLowerLeg2Body());
        }
    }

    for (auto it = bodies.begin(); it < bodies.end(); ++it)
    {
        b2Body* body = *it;
        b2Joint* joint = _bodyVehicleJointDict[body];
        if (joint)
        {
            getWorld()->DestroyJoint(joint);
        }
        _bodyVehicleJointDict.erase(body);
    }
}

// @0063ee98
int Vehicle::indexOfCharacter(CharacterB2D* character)
{
    auto it = std::find(_characters.begin(), _characters.end(), character);
    if (it != _characters.end())
    {
        return (int)(it - _characters.begin());
    }
    return -1;
}

// @0063eee0
void Vehicle::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if ((fixture == _frontWheelFixture || fixture == _backWheelFixture) && !otherFixture->IsSensor())
    {
        _wheelContacts++;
    }
    contactSoundHandler(fixture, otherFixture, contact, nullptr);
}

// @0063ef14
void Vehicle::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if ((fixture == _frontWheelFixture || fixture == _backWheelFixture) && !otherFixture->IsSensor())
    {
        _wheelContacts--;
    }
}

// @0063ef40
void Vehicle::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                        const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2)
    {
        maxImpulse = b2Max(maxImpulse, impulse->normalImpulses[1]);
    }
    if (_contactImpulseDict[fixture] < maxImpulse)
    {
        if (_contactResultBufferDict[fixture].impulse < maxImpulse)
        {
            _contactResultBufferDict[fixture].impulse = maxImpulse;
        }
    }
}

// @0063f170
void Vehicle::wheelSoundStopped()
{
    _wheelSound = nullptr;
}

// @0063f178
void Vehicle::setCurrentPose(VehiclePose pose)
{
    if (_currentPose != pose)
    {
        _currentPose = pose;
        if (pose == VehiclePoseNone)
        {
            cancelPose();
        }
    }
}

// @0063f19c
void Vehicle::cancelPose()
{
    CharacterB2D* character = _characters[0];
    if (character->getNeckJoint())
    {
        character->getNeckJoint()->EnableMotor(false);
    }
    if (character->getShoulderJoint1())
    {
        character->getShoulderJoint1()->EnableMotor(false);
    }
    if (character->getShoulderJoint2())
    {
        character->getShoulderJoint2()->EnableMotor(false);
    }
    if (character->getElbowJoint1())
    {
        character->getElbowJoint1()->EnableMotor(false);
    }
    if (character->getElbowJoint2())
    {
        character->getElbowJoint2()->EnableMotor(false);
    }
    if (character->getHipJoint1())
    {
        character->getHipJoint1()->EnableMotor(false);
    }
    if (character->getHipJoint2())
    {
        character->getHipJoint2()->EnableMotor(false);
    }
    if (character->getKneeJoint1())
    {
        character->getKneeJoint1()->EnableMotor(false);
    }
    if (character->getKneeJoint2())
    {
        character->getKneeJoint2()->EnableMotor(false);
    }
}

// @0063f2c0
void Vehicle::checkPose()
{
    switch (_currentPose)
    {
    case VehiclePoseForward:
        forwardPose();
        break;
    case VehiclePoseBack:
        backPose();
        break;
    case VehiclePoseLeanForward:
        leanForwardPose();
        break;
    case VehiclePoseLeanBack:
        leanBackPose();
        break;
    case VehiclePoseNoLeanBack:
        noLeanBackPose();
        break;
    case VehiclePoseNoLeanForward:
        noLeanForwardPose();
        break;
    case VehiclePoseSpecial:
        specialPose();
        break;
    case VehiclePoseExtra1:
        extraPose1();
        break;
    case VehiclePoseExtra2:
        extraPose2();
        break;
    case VehiclePoseExtra3:
        extraPose3();
        break;
    default:
        break;
    }
}

// @0063f2e4
void Vehicle::addWheelJoint(b2RevoluteJoint* joint, b2Body* wheelBody)
{
    if (_wheelJoints.size() == 0)
    {
        _wheelJointSpeedDict[joint] = 1.0f;
        _mainWheelShapeRadius = wheelBody->GetFixtureList()->GetShape()->m_radius;
    }
    else
    {
        float mainRadius = _mainWheelShapeRadius;
        float radius = wheelBody->GetFixtureList()->GetShape()->m_radius;
        _wheelJointSpeedDict[joint] = radius / mainRadius;
    }
    _wheelJoints.push_back(joint);
}

// @0063f5bc
void Vehicle::forwardButtonPressed()
{
    for (unsigned int i = 0; i < _wheelJoints.size(); i++)
    {
        b2RevoluteJoint* joint = _wheelJoints[i];
        if (!joint->IsMotorEnabled())
        {
            joint->EnableMotor(true);
        }
        float speed = joint->GetJointSpeed();
        float newSpeed = 0.0f;
        if (speed <= 0.0f)
        {
            float ratio = _wheelJointSpeedDict[joint];
            newSpeed = (speed <= ratio * _maxSpeed) ? speed : speed - ratio * _accelStep;
        }
        joint->SetMotorSpeed(newSpeed);
    }
    setCurrentPose(VehiclePoseForward);
}

// @0063f720
void Vehicle::backButtonPressed()
{
    for (unsigned int i = 0; i < _wheelJoints.size(); i++)
    {
        b2RevoluteJoint* joint = _wheelJoints[i];
        if (!joint->IsMotorEnabled())
        {
            joint->EnableMotor(true);
        }
        float speed = joint->GetJointSpeed();
        float newSpeed = 0.0f;
        if (speed >= 0.0f)
        {
            float ratio = _wheelJointSpeedDict[joint];
            newSpeed = (-(_maxSpeed * ratio) <= speed) ? speed : speed + ratio * _accelStep;
        }
        joint->SetMotorSpeed(newSpeed);
    }
    setCurrentPose(VehiclePoseBack);
}

// @0063f884
void Vehicle::forwardBackButtonsNull()
{
    for (unsigned int i = 0; i < _wheelJoints.size(); i++)
    {
        b2RevoluteJoint* joint = _wheelJoints[i];
        if (joint->IsMotorEnabled())
        {
            joint->EnableMotor(false);
        }
    }
    if (_currentPose == VehiclePoseForward || _currentPose == VehiclePoseBack)
    {
        setCurrentPose(VehiclePoseNone);
    }
}

// @0063f920
void Vehicle::leanButtonsNull()
{
    if (_currentPose == VehiclePoseLeanForward || _currentPose == VehiclePoseLeanBack ||
        _currentPose == VehiclePoseNoLeanBack || _currentPose == VehiclePoseNoLeanForward)
    {
        setCurrentPose(VehiclePoseNone);
    }
}

// @0063f944
void Vehicle::actions()
{
    checkPose();
    checkJoints();
    handleContactAdds();
    handleContactResults();

    if (_wheelContacts == 0)
    {
        if (_wheelSound)
        {
            _wheelSound->fadeTo(0.0f, _soundFadeTime, true);
        }
    }
    else if (_frontWheelFixture && _frontWheelFixture->GetBody())
    {
        b2Body* wheelBody = _frontWheelFixture->GetBody();
        if (fabsf(wheelBody->GetAngularVelocity()) <= 1.0f)
        {
            if (_wheelSound)
            {
                _wheelSound->fadeTo(0.0f, _soundFadeTime, true);
            }
        }
        else if (_wheelSound)
        {
            _wheelSound->fadeTo(_wheelSoundVolume, _soundFadeTime, false);
        }
        else
        {
            _wheelSound = createBodySound(_wheelSoundName, wheelBody, 1.0f, true);
            if (_wheelSound)
            {
                // @006400a4 (std::function target "ZN7Vehicle7actionsEvE3$_0")
                _wheelSound->setFinishCallback([this](int&) { wheelSoundStopped(); });
                _wheelSound->setMaxVolume(0.0f);
                _wheelSound->fadeTo(_wheelSoundVolume, _soundFadeTime, false);
            }
        }
    }
}

// @0063fb6c
void Vehicle::handleInjury(CharacterInjury injury, CharacterB2D* character)
{
    switch (static_cast<int>(injury))  // 15 is never produced but is in the jump table
    {
    case CharacterInjuryPelvisSmash:
    case CharacterInjuryChestSmash:
        handleFatalWound(character);
        break;
    case CharacterInjuryFoot1Smash:
        handleLowerLeg1Injury(character);
        break;
    case CharacterInjuryFoot2Smash:
        handleLowerLeg2Injury(character);
        break;
    case CharacterInjuryShoulder1Break:
        handleUpperArm1Injury(character);
        break;
    case CharacterInjuryShoulder2Break:
        handleUpperArm2Injury(character);
        break;
    case CharacterInjuryElbow1Break:
        handleLowerArm1Injury(character);
        break;
    case CharacterInjuryElbow2Break:
        handleLowerArm2Injury(character);
        break;
    case CharacterInjuryHip1Break:
        handleUpperLeg1Injury(character);
        break;
    case CharacterInjuryHip2Break:
        handleUpperLeg2Injury(character);
        break;
    case CharacterInjuryKnee1Break:
        handleLowerLeg1Injury(character);
        break;
    case CharacterInjuryKnee2Break:
        handleLowerLeg2Injury(character);
        break;
    case CharacterInjuryTorsoBreak:
    case CharacterInjuryNeckBreak:
    case 15:
    case CharacterInjuryDeath:
        handleFatalWound(character);
        break;
    default:
        break;
    }
}

// @0063fb98
void Vehicle::handleUpperArm1Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getUpperArm1Body());
    destroyJointsForBody(character->getLowerArm1Body());
    checkStateOfCharacter(character);
}

// @0063fc00
void Vehicle::handleUpperArm2Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getUpperArm2Body());
    destroyJointsForBody(character->getLowerArm2Body());
    checkStateOfCharacter(character);
}

// @0063fc68
void Vehicle::handleLowerArm1Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getLowerArm1Body());
    checkStateOfCharacter(character);
}

// @0063fcb4
void Vehicle::handleLowerArm2Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getLowerArm2Body());
    checkStateOfCharacter(character);
}

// @0063fd00
void Vehicle::handleUpperLeg1Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getUpperLeg1Body());
    destroyJointsForBody(character->getLowerLeg1Body());
    checkStateOfCharacter(character);
}

// @0063fd68
void Vehicle::handleUpperLeg2Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getUpperLeg2Body());
    destroyJointsForBody(character->getLowerLeg2Body());
    checkStateOfCharacter(character);
}

// @0063fdd0
void Vehicle::handleLowerLeg1Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getLowerLeg1Body());
    checkStateOfCharacter(character);
}

// @0063fe1c
void Vehicle::handleLowerLeg2Injury(CharacterB2D* character)
{
    destroyJointsForBody(character->getLowerLeg2Body());
    checkStateOfCharacter(character);
}

// @0063fe68
void Vehicle::handleFatalWound(CharacterB2D* character)
{
    ejectCharacter(character);
}

// @0063fe74
void Vehicle::destroyJointsForBody(b2Body* body)
{
    if (body)
    {
        b2Joint* joint = _bodyVehicleJointDict[body];
        if (joint)
        {
            _bodyVehicleJointDict.erase(body);
            if (joint->IsActive())
            {
                getWorld()->DestroyJoint(joint);
            }
        }
    }
}
