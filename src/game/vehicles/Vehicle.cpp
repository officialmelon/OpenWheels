#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "SoundController.h"
#include "platform/compat/Box2DFloat.h"
#include "qol/QoL.h"  // QOL (PC addition): re-grab vehicle

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
    // QOL (PC addition): re-grab vehicle - the rider's starting pose (no effect on the game).
    qolCacheRider(character);
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
        _qolLastRider = character;  // QOL (PC addition): see cancelPose
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
    float diff = owb2::jointAngle(joint) - target;
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
    // QOL (PC addition): the subclasses' ejectCharacter cancel the pose after the rider was
    // erased; the binary then reads the erased pointer still in the vector's storage (the rider
    // just ejected). Same rider, without reading past the end.
    CharacterB2D* character = _characters.empty() ? _qolLastRider : _characters[0];
    if (!character)
    {
        return;
    }
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
        float speed = owb2::jointSpeed(joint);
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
        float speed = owb2::jointSpeed(joint);
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

// ---------------------------------------------------------------------------------------------
// QOL (PC addition): re-grab vehicle (qol::regrabVehicle, docs/QOL.md). Not in the original.
// ---------------------------------------------------------------------------------------------

namespace
{

// b2ContactFilter::ShouldCollide (the world keeps the default contact filter).
bool qolShouldCollide(const b2Filter& a, const b2Filter& b)
{
    if (a.groupIndex == b.groupIndex && a.groupIndex != 0)
    {
        return a.groupIndex > 0;
    }
    return (a.maskBits & b.categoryBits) != 0 && (a.categoryBits & b.maskBits) != 0;
}

b2Transform qolTransform(b2Vec2 p, float a)
{
    return b2Transform(p, b2Rot(a));
}

// Something solid the re-mounted rider would end up inside of.
class QolOverlapQuery : public b2QueryCallback
{
public:
    b2Fixture* fixture = nullptr;
    int32 child = 0;
    b2Transform xf;
    const std::set<b2Body*>* ignore = nullptr;
    bool hit = false;

    bool ReportFixture(b2Fixture* other) override
    {
        b2Body* body = other->GetBody();
        if (other->IsSensor() || ignore->count(body))
        {
            return true;
        }
        // Light loose things (debris, his own severed limbs) are just pushed aside.
        if (body->GetType() == b2_dynamicBody && body->GetMass() < 0.5f)
        {
            return true;
        }
        if (!qolShouldCollide(fixture->GetFilterData(), other->GetFilterData()))
        {
            return true;
        }
        const b2Shape* shape = other->GetShape();
        for (int32 i = 0; i < shape->GetChildCount(); i++)
        {
            if (b2TestOverlap(fixture->GetShape(), child, shape, i, xf, body->GetTransform()))
            {
                hit = true;
                return false;
            }
        }
        return true;
    }
};

}  // namespace

void Vehicle::qolCacheRider(CharacterB2D* character)
{
    b2Body* frame = qolFrameBody();
    if (!frame || !character || _qolRiderPoses.count(character))
    {
        return;
    }
    QolRiderPose& pose = _qolRiderPoses[character];
    pose.frame = {frame->GetPosition(), frame->GetAngle()};
    for (b2Body* body : character->qolParts())
    {
        if (body)
        {
            pose.bodies[body] = {body->GetPosition(), body->GetAngle()};
        }
    }
    if (!_qolCached)
    {
        _qolCached = true;
        for (b2Body* body : qolVehicleBodies(character))
        {
            _qolSpawnTransforms[body] = {body->GetPosition(), body->GetAngle()};
            for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
            {
                _qolFilters[fixture] = fixture->GetFilterData();
            }
        }
    }
}

// The frame and every dynamic body jointed to it, directly or not, but the riders'.
std::vector<b2Body*> Vehicle::qolVehicleBodies(CharacterB2D* rider)
{
    std::vector<b2Body*> bodies;
    b2Body* frame = qolFrameBody();
    if (!frame)
    {
        return bodies;
    }
    auto excluded = [this, rider](b2Body* body) {
        if (body->GetType() != b2_dynamicBody || (rider && rider->ownsBody(body)))
        {
            return true;
        }
        for (CharacterB2D* character : _characters)
        {
            if (character->ownsBody(body))
            {
                return true;
            }
        }
        return false;
    };
    bodies.push_back(frame);
    for (size_t i = 0; i < bodies.size() && bodies.size() < 256; i++)
    {
        for (b2JointEdge* edge = bodies[i]->GetJointList(); edge; edge = edge->next)
        {
            b2Body* other = edge->other;
            if (std::find(bodies.begin(), bodies.end(), other) == bodies.end() && !excluded(other))
            {
                bodies.push_back(other);
            }
        }
    }
    return bodies;
}

bool Vehicle::qolOwnsBody(b2Body* body, CharacterB2D* rider)
{
    if (!body || !_qolSpawnTransforms.count(body))
    {
        return false;
    }
    std::vector<b2Body*> bodies = qolVehicleBodies(rider);
    return std::find(bodies.begin(), bodies.end(), body) != bodies.end();
}

b2Body* Vehicle::qolTouchedBody(b2Fixture* hand, CharacterB2D* rider)
{
    if (!hand || !_qolCached)
    {
        return nullptr;
    }
    const b2Transform& handXf = hand->GetBody()->GetTransform();
    b2AABB handBox;
    hand->GetShape()->ComputeAABB(&handBox, handXf, 0);
    for (b2Body* body : qolVehicleBodies(rider))
    {
        if (!_qolSpawnTransforms.count(body))
        {
            continue;  // (jointed to the vehicle later, e.g. something the magnet holds)
        }
        for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
        {
            const b2Shape* shape = fixture->GetShape();
            for (int32 i = 0; i < shape->GetChildCount(); i++)
            {
                b2AABB box;
                shape->ComputeAABB(&box, body->GetTransform(), i);
                if (b2TestOverlap(handBox, box) &&
                    b2TestOverlap(hand->GetShape(), 0, shape, i, handXf, body->GetTransform()))
                {
                    return body;
                }
            }
        }
    }
    return nullptr;
}

bool Vehicle::qolLimb(CharacterB2D* character, b2RevoluteJoint* joint)
{
    if (!joint)
    {
        return false;
    }
    b2Body* body = joint->GetBodyB();
    return body != character->getUpperArm3Body() && body != character->getUpperArm4Body() &&
           body != character->getUpperLeg3Body() && body != character->getUpperLeg4Body();
}

bool Vehicle::qolRiderFit(CharacterB2D* character)
{
    static const CharacterInjury fatal[] = {CharacterInjuryHeadSmash,  CharacterInjuryChestSmash,
                                            CharacterInjuryPelvisSmash, CharacterInjuryTorsoBreak,
                                            CharacterInjuryNeckBreak,  CharacterInjuryDeath};
    if (!character->qolFit())
    {
        return false;
    }
    for (CharacterInjury injury : fatal)
    {
        if (character->qolHasInjury(injury))
        {
            return false;
        }
    }
    return true;
}

bool Vehicle::qolTryRemount(CharacterB2D* character)
{
    if (!qol::regrabVehicle() || !character || !qolFrameBody() || !_qolRiderPoses.count(character) ||
        std::find(_characters.begin(), _characters.end(), character) != _characters.end())
    {
        return false;
    }
    if (!qolRiderFit(character) || !qolCanRemount(character) || !qolPlanRemount(character))
    {
        return false;
    }
    qolRemount(character);
    _qolPlan.clear();
    if (std::find(_characters.begin(), _characters.end(), character) == _characters.end())
    {
        return false;  // a lost limb threw him off again (he is ejected again)
    }
    character->qolRemounted();
    return true;
}

// The rider's bodies and where they go: his parts back into their starting pose relative to the
// frame; whatever else hangs on him (a dislocated stub, a ligament, an arrow) moves along with
// the part it hangs from. Refused when he is pinned to the level or would end up inside it.
bool Vehicle::qolPlanRemount(CharacterB2D* character)
{
    _qolPlan.clear();
    b2Body* frame = qolFrameBody();
    const QolRiderPose& pose = _qolRiderPoses[character];
    b2Body* chest = character->getChestBody();
    if (!chest || !pose.bodies.count(chest))
    {
        return false;
    }
    // spawn space -> now: rotate by the frame's turn since the start, about the frame.
    const QolXf now = {frame->GetPosition(), frame->GetAngle()};
    const float turn = now.a - pose.frame.a;
    const b2Rot rot(turn);
    auto toNow = [&](const QolXf& x) {
        return QolXf{b2Mul(rot, x.p - pose.frame.p) + now.p, x.a + turn};
    };

    std::vector<b2Body*> vehicle = qolVehicleBodies(character);
    std::set<b2Body*> ignore(vehicle.begin(), vehicle.end());
    QolXf chestSpawn = pose.bodies.at(chest);
    _qolPlan.push_back({chest, chestSpawn, toNow(chestSpawn)});
    for (size_t i = 0; i < _qolPlan.size(); i++)
    {
        b2Body* body = _qolPlan[i].body;
        for (b2JointEdge* edge = body->GetJointList(); edge; edge = edge->next)
        {
            b2Body* other = edge->other;
            if (character->qolIsGripJoint(edge->joint) || ignore.count(other))
            {
                continue;
            }
            bool planned = false;
            for (const QolPlanBody& p : _qolPlan)
            {
                planned = planned || p.body == other;
            }
            if (planned)
            {
                continue;
            }
            if (other->GetType() != b2_dynamicBody || _qolPlan.size() >= 64)
            {
                _qolPlan.clear();
                return false;
            }
            QolXf spawn;
            auto cached = pose.bodies.find(other);
            if (cached != pose.bodies.end() && pose.bodies.count(body))
            {
                spawn = cached->second;  // a skeleton joint: his starting pose
            }
            else
            {
                b2Vec2 local = b2MulT(body->GetTransform(), other->GetPosition());
                const QolXf& parent = _qolPlan[i].spawn;
                spawn = {b2Mul(b2Rot(parent.a), local) + parent.p,
                         parent.a + (other->GetAngle() - body->GetAngle())};
            }
            _qolPlan.push_back({other, spawn, toNow(spawn)});
        }
    }

    for (const QolPlanBody& p : _qolPlan)
    {
        ignore.insert(p.body);
    }
    for (const QolPlanBody& p : _qolPlan)
    {
        b2Transform xf = qolTransform(p.target.p, p.target.a);
        for (b2Fixture* fixture = p.body->GetFixtureList(); fixture; fixture = fixture->GetNext())
        {
            if (fixture->IsSensor())
            {
                continue;
            }
            for (int32 child = 0; child < fixture->GetShape()->GetChildCount(); child++)
            {
                QolOverlapQuery query;
                query.fixture = fixture;
                query.child = child;
                query.xf = xf;
                query.ignore = &ignore;
                b2AABB box;
                fixture->GetShape()->ComputeAABB(&box, xf, child);
                getWorld()->QueryAABB(&query, box);
                if (query.hit)
                {
                    _qolPlan.clear();
                    return false;
                }
            }
        }
    }
    return true;
}

void Vehicle::qolMount(CharacterB2D* character, const std::function<void()>& attach)
{
    b2Body* frame = qolFrameBody();
    const QolRiderPose& pose = _qolRiderPoses[character];
    const QolXf now = {frame->GetPosition(), frame->GetAngle()};
    const float turn = now.a - pose.frame.a;
    const b2Rot rot(turn);
    const b2Rot back(-turn);

    character->qolBeginRemount();

    std::vector<b2Body*> vehicle = qolVehicleBodies(character);
    std::vector<b2Body*> reset;
    qolRemountResetBodies(&reset);
    std::vector<QolXf> saved;
    for (b2Body* body : vehicle)
    {
        saved.push_back({body->GetPosition(), body->GetAngle()});
    }
    auto isReset = [&](b2Body* body) {
        return _qolSpawnTransforms.count(body) &&
               std::find(reset.begin(), reset.end(), body) != reset.end();
    };

    // Spawn space: the vehicle as it is, turned and moved so the frame is where it started (the
    // reset bodies exactly as they started), the rider in his starting pose.
    for (size_t i = 0; i < vehicle.size(); i++)
    {
        b2Body* body = vehicle[i];
        if (isReset(body))
        {
            const QolXf& start = _qolSpawnTransforms[body];
            body->SetTransform(start.p, start.a);
        }
        else
        {
            body->SetTransform(b2Mul(back, saved[i].p - now.p) + pose.frame.p, saved[i].a - turn);
        }
    }
    for (const QolPlanBody& p : _qolPlan)
    {
        p.body->SetTransform(p.spawn.p, p.spawn.a);
    }

    // The attach code sets filters / sensors on his limbs; the lost ones keep what their injury
    // gave them (a severed leg masked off like a seated one would fall through the level).
    std::vector<b2Fixture*> lost = character->qolLostFixtures();
    std::vector<std::pair<b2Filter, bool>> lostState;
    for (b2Fixture* fixture : lost)
    {
        lostState.push_back({fixture->GetFilterData(), fixture->IsSensor()});
    }

    attach();

    for (size_t i = 0; i < lost.size(); i++)
    {
        lost[i]->SetFilterData(lostState[i].first);
        lost[i]->SetSensor(lostState[i].second);
    }

    // Back: the vehicle bodies exactly where they were (the reset ones follow the frame), the
    // rider on it, moving with the frame.
    for (size_t i = 0; i < vehicle.size(); i++)
    {
        b2Body* body = vehicle[i];
        if (isReset(body))
        {
            const QolXf& start = _qolSpawnTransforms[body];
            body->SetTransform(b2Mul(rot, start.p - pose.frame.p) + now.p, start.a + turn);
        }
        else
        {
            body->SetTransform(saved[i].p, saved[i].a);
        }
    }
    const float angularVelocity = frame->GetAngularVelocity();
    for (const QolPlanBody& p : _qolPlan)
    {
        p.body->SetTransform(p.target.p, p.target.a);
        p.body->SetLinearVelocity(frame->GetLinearVelocityFromWorldPoint(p.body->GetWorldCenter()));
        p.body->SetAngularVelocity(angularVelocity);
        p.body->SetAwake(true);
    }
    frame->SetAwake(true);
    character->qolSetRiding();
}

// The vehicle's fixtures get their riding filters back (the ejects zero them or move them out of
// the riders' group).
void Vehicle::qolRestoreFilters(CharacterB2D* rider)
{
    for (b2Body* body : qolVehicleBodies(rider))
    {
        for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
        {
            auto it = _qolFilters.find(fixture);
            if (it != _qolFilters.end())
            {
                fixture->SetFilterData(it->second);
            }
        }
    }
}

void Vehicle::qolReplayInjuries(CharacterB2D* character)
{
    // A copy: handleInjury may throw him off again (then the list does not change either).
    std::vector<CharacterInjury> injuries = character->qolInjuries();
    for (CharacterInjury injury : injuries)
    {
        switch (injury)
        {
        case CharacterInjuryFoot1Smash:
        case CharacterInjuryFoot2Smash:
        case CharacterInjuryShoulder1Break:
        case CharacterInjuryShoulder2Break:
        case CharacterInjuryElbow1Break:
        case CharacterInjuryElbow2Break:
        case CharacterInjuryHip1Break:
        case CharacterInjuryHip2Break:
        case CharacterInjuryKnee1Break:
        case CharacterInjuryKnee2Break:
            if (std::find(_characters.begin(), _characters.end(), character) != _characters.end())
            {
                handleInjury(injury, character);
            }
            break;
        default:
            break;
        }
    }
}
