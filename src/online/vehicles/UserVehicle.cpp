// ONLINE (PC addition): browser user-built vehicles. Port of Flash level/groups/Vehicle.as plus
// the vehicle parts of UserLevel.createGroups / createJoints. See UserVehicle.h.
//
// Units: converted levels use Flash's own 62.5 px/m, so masses, impulses and motor speeds are the
// browser numbers. The world is mirrored (y up): revolute motor speeds and limits arrive negated
// from LevelB2D (convertRevJointData) and the joint logic below only compares them with the
// joint's own speed/angle, so it works unchanged; the lean impulses are mirrored by hand. Flash ran
// operateKeys once per 30 Hz frame, the mobile control byte arrives once per frame of the 60 Hz
// session: per-frame increments (joint acceleration, lean impulse) are scaled by
// getTimeStepOverFlashTimeStep() (0.5).
#include "online/vehicles/UserVehicle.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "cocos2d.h"

#include "ArrowGun.h"
#include "CharacterB2D.h"
#include "DestructionListener.h"
#include "Jet.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"
#include "Settings.h"
#include "platform/compat/Box2DFloat.h"

USING_NS_CC;

// Flash Jet.firingAllowed / ArrowGun.firingAllowed + unlimitedArrows: the mobile classes keep
// these protected without setters; reach them through member pointers (no change to the classes).
namespace {
struct JetAccess : Jet
{
    static bool Jet::*firingAllowed() { return &JetAccess::_firingAllowed; }
};
struct ArrowGunAccess : ArrowGun
{
    static bool ArrowGun::*firingAllowed() { return &ArrowGunAccess::_firingAllowed; }
    static bool ArrowGun::*unlimitedArrows() { return &ArrowGunAccess::_unlimitedArrows; }
    static b2Body* ArrowGun::*targetBody() { return &ArrowGunAccess::_targetBody; }
};
}  // namespace

namespace online {

namespace {

const float kMaxSpinAV = 3.5f;      // Vehicle.maxSpinAV
const float kImpulseOffset = 1.0f;  // Vehicle.impulseOffset (m)

struct LevelVehicles
{
    std::vector<UserVehicle*> vehicles;
    std::map<int, UserVehicle*> byGroup;
    std::map<b2Fixture*, UserVehicle*> handles;
    std::map<CharacterB2D*, UserVehicleRider> riders;
};

std::map<LevelB2D*, LevelVehicles>& registry()
{
    static std::map<LevelB2D*, LevelVehicles> levels;
    return levels;
}

LevelB2D* currentLevel()
{
    Session* session = Settings::getInstance()->getCurrentSession();
    return session ? session->getLevel() : nullptr;
}

LevelVehicles* levelVehicles(LevelB2D* level)
{
    auto it = registry().find(level);
    return it == registry().end() ? nullptr : &it->second;
}

int clampInt(int value, int low, int high) { return std::max(low, std::min(high, value)); }

unsigned char g_pcExtraBits = 0;

}  // namespace

// ---------------------------------------------------------------------------------------------
// Loading (UserLevel.createGroups / createJoints)

UserVehicle* UserVehicle::createForGroup(LevelB2D* level, LevelDataElement* group, b2Body* body,
                                         int groupIndex)
{
    bool isVehicle = false;
    if (!level || !group->boolAttribute("v", &isVehicle) || !isVehicle) {
        return nullptr;
    }
    // UserLevelLoader: int(@sb) etc. through the RefVehicle setters' clamps.
    int value = 0;
    UserVehicle* vehicle = new (std::nothrow) UserVehicle();
    if (!vehicle) return nullptr;
    vehicle->_level = level;
    vehicle->_body = body;
    vehicle->_groupIndex = groupIndex;
    value = 0; group->intAttribute("sb", &value); vehicle->_spaceAction = clampInt(value, 0, 3);
    value = 0; group->intAttribute("sh", &value); vehicle->_shiftAction = clampInt(value, 0, 3);
    value = 0; group->intAttribute("ct", &value); vehicle->_ctrlAction = clampInt(value, 0, 3);
    value = 0; group->intAttribute("a", &value); vehicle->_acceleration = (float)clampInt(value, 1, 10);
    value = 0; group->intAttribute("l", &value); vehicle->_leaningStrength = (float)clampInt(value, 0, 10);
    value = 0; group->intAttribute("cp", &value); vehicle->_characterPose = clampInt(value, 0, 3);
    bool lock = false;
    group->boolAttribute("lo", &lock);
    vehicle->_lockJoints = lock;

    LevelVehicles& vehicles = registry()[level];
    vehicles.vehicles.push_back(vehicle);
    vehicles.byGroup[groupIndex] = vehicle;
    return vehicle;
}

void UserVehicle::shapeAdded(LevelDataElement* shape, b2Body* body, b2Fixture* previousFirst)
{
    // Flash: every interactive shape of a vehicle is a handle unless saved with h="f".
    bool handle = true;
    shape->boolAttribute("vh", &handle);
    b2Fixture* fixture = body ? body->GetFixtureList() : nullptr;
    if (!handle || !fixture || fixture == previousFirst) {
        return;
    }
    _handles.push_back(fixture);
    if (LevelVehicles* vehicles = levelVehicles(_level)) {
        vehicles->handles[fixture] = this;
    }
}

void UserVehicle::groupSpecialAdded(LevelItem* special)
{
    // createGroups only offers arrow guns of the group itself to the vehicle.
    if (dynamic_cast<ArrowGun*>(special)) {
        checkAddSpecial(special);
    }
}

void UserVehicle::groupFinished(b2Body* body)
{
    _body = body;
    if (_body) {
        setShitUp();
    } else {
        // Flash only builds a Vehicle for a group with shapes (shapesUsed > 0): forget it.
        if (LevelVehicles* vehicles = levelVehicles(_level)) {
            vehicles->byGroup.erase(_groupIndex);
            for (b2Fixture* fixture : _handles) vehicles->handles.erase(fixture);
        }
        _handles.clear();
    }
}

void UserVehicle::setShitUp()
{
    _leanImpulse = _body->GetMass() * (_leaningStrength * 0.1f);
}

void UserVehicle::checkAddSpecial(LevelItem* item)
{
    if (Jet* jet = dynamic_cast<Jet*>(item)) {
        if (std::find(_jets.begin(), _jets.end(), jet) == _jets.end()) {
            jet->*JetAccess::firingAllowed() = false;
            _jets.push_back(jet);
        }
    } else if (ArrowGun* gun = dynamic_cast<ArrowGun*>(item)) {
        if (std::find(_arrowGuns.begin(), _arrowGuns.end(), gun) == _arrowGuns.end()) {
            gun->*ArrowGunAccess::firingAllowed() = false;
            gun->*ArrowGunAccess::targetBody() = nullptr;
            gun->*ArrowGunAccess::unlimitedArrows() = true;
            _arrowGuns.push_back(gun);
        }
    }
}

void UserVehicle::checkAddVehicle(UserVehicle* vehicle)
{
    if (vehicle && std::find(_vehicles.begin(), _vehicles.end(), vehicle) == _vehicles.end()) {
        _vehicles.push_back(vehicle);
        vehicle->checkAddVehicle(this);
    }
}

void UserVehicle::addJoint(b2Joint* joint)
{
    _joints.push_back(joint);
    float speed = 0.0f;
    bool motor = false;
    if (joint->GetType() == e_revoluteJoint) {
        b2RevoluteJoint* revolute = static_cast<b2RevoluteJoint*>(joint);
        motor = revolute->IsMotorEnabled();
        speed = revolute->GetMotorSpeed();
        revolute->EnableMotor(false);
    } else if (joint->GetType() == e_prismaticJoint) {
        b2PrismaticJoint* prismatic = static_cast<b2PrismaticJoint*>(joint);
        motor = prismatic->IsMotorEnabled();
        speed = prismatic->GetMotorSpeed();
        prismatic->EnableMotor(false);
    }
    float accel = 0.0f;
    if (motor) {
        accel = _acceleration < 10.0f ? std::fabs(speed / (100.0f - _acceleration * 10.0f))
                                      : std::fabs(speed);
        if (_lockJoints) {
            _slowAfterEject = true;
            setSlowing(true);
        }
    } else {
        speed = 0.0f;  // Flash stores null
    }
    _jointMotorSpeeds.push_back(speed);
    _jointAccels.push_back(accel);
    if (Session* session = getSession()) {
        session->getDestructionListener()->addJointListener(joint, this);
    }
}

void UserVehicle::jointWillBeDestroyed(b2Joint* joint)
{
    for (size_t i = 0; i < _joints.size(); i++) {
        if (_joints[i] == joint) {
            _joints[i] = nullptr;
        }
    }
}

void userVehicleJointCreated(LevelB2D* level, LevelDataElement* element, b2Joint* joint)
{
    LevelVehicles* vehicles = levelVehicles(level);
    if (!vehicles || !joint) {
        return;
    }
    const char* b1 = element->stringAttribute("b1");
    const char* b2 = element->stringAttribute("b2");
    std::string ref1 = b1 ? b1 : "";
    std::string ref2 = b2 ? b2 : "";
    auto vehicleOf = [&](const std::string& ref) -> UserVehicle* {
        if (ref.size() < 2 || ref[0] != 'g') return nullptr;
        auto it = vehicles->byGroup.find(atoi(ref.c_str() + 1));
        return it == vehicles->byGroup.end() ? nullptr : it->second;
    };
    // The other body: a special is offered to checkAddSpecial, a vehicle to checkAddVehicle.
    auto offer = [&](UserVehicle* vehicle, const std::string& other) {
        if (other.size() >= 2 && other[0] == 's') {
            if (LevelItem* special = level->getSpecial((unsigned int)atoi(other.c_str() + 1))) {
                vehicle->checkAddSpecial(special);
            }
        } else if (UserVehicle* otherVehicle = vehicleOf(other)) {
            vehicle->checkAddVehicle(otherVehicle);
        }
    };
    UserVehicle* vehicle1 = vehicleOf(ref1);
    UserVehicle* vehicle2 = vehicleOf(ref2);
    if (vehicle1) offer(vehicle1, ref2);
    if (vehicle2) offer(vehicle2, ref1);
    bool controlled = true;  // RefJoint.vehicleControlled defaults to true, saved v="f" when off
    element->boolAttribute("vc", &controlled);
    if (controlled) {
        if (vehicle1) vehicle1->addJoint(joint);
        if (vehicle2) vehicle2->addJoint(joint);
    }
}

// ---------------------------------------------------------------------------------------------
// Riders

void UserVehicle::addCharacter(CharacterB2D* character)
{
    if (std::find(_characters.begin(), _characters.end(), character) == _characters.end()) {
        _characters.push_back(character);
        setSlowing(false);
    }
}

void UserVehicle::removeCharacter(CharacterB2D* character)
{
    auto it = std::find(_characters.begin(), _characters.end(), character);
    if (it == _characters.end()) {
        return;
    }
    _characters.erase(it);
    if (_characters.empty()) {
        if (_slowAfterEject) {
            setSlowing(true);
        } else {
            disableMotors();
        }
        setArrowsFiring(false);
    }
}

void UserVehicle::setSlowing(bool on)
{
    if (on == _slowing || !_level) {
        return;
    }
    _slowing = on;
    if (on) {
        _level->addToActions(this);
    } else {
        _level->removeFromActions(this);
    }
}

// ---------------------------------------------------------------------------------------------
// Controls (Vehicle.operateKeys)

void UserVehicle::operateKeys(unsigned int iteration, unsigned char state)
{
    if (_operated && iteration <= _lastIteration) {
        return;
    }
    _operated = true;
    _lastIteration = iteration;
    if (!_body) {
        return;
    }
    const bool left = (state & 0x08) != 0;   // lean back
    const bool right = (state & 0x04) != 0;  // lean forward
    const bool up = (state & 0x01) != 0;
    const bool down = (state & 0x02) != 0;
    if (left) {
        if (!right) leanBack();
    } else if (right) {
        leanForward();
    }
    if (up) {
        if (down) upAndDownActions();
        else driveJoints(false);
    } else if (down) {
        driveJoints(true);
    } else {
        upAndDownActions();
    }
    int actions[4] = {0, 0, 0, 0};
    if (state & 0x10) actions[_spaceAction] += 1;
    if (state & 0x20) actions[_shiftAction] += 1;
    if (state & 0x40) actions[_ctrlAction] = actions[_shiftAction] + 1;  // (sic) Flash reads shift's slot
    if (actions[ActionBrake] > 0) brake();
    setJetsFiring(actions[ActionJets] > 0);
    setArrowsFiring(actions[ActionArrows] > 0);
    std::vector<UserVehicle*> linked = _vehicles;
    for (UserVehicle* vehicle : linked) {
        vehicle->operateKeys(iteration, state);
    }
    if (state & 0x80) {
        // zPressedActions: every rider leaves (CharacterB2D.userVehicleEject).
        std::vector<CharacterB2D*> riders = _characters;
        for (CharacterB2D* character : riders) {
            character->onlineUserVehicleEject();
        }
    }
}

// Flash leftPressedActions: counter-clockwise on screen (positive torque in the y-up world).
// Flash's spin factor (av + max) / max with its clockwise av = -av here.
void UserVehicle::leanBack()
{
    if (_leaningStrength <= 0.0f) return;
    float angle = _body->GetAngle();
    float factor = (kMaxSpinAV - _body->GetAngularVelocity()) / kMaxSpinAV;
    factor = std::max(0.0f, std::min(1.0f, factor));
    float impulse = _leanImpulse * factor * getTimeStepOverFlashTimeStep();
    float c = cosf(angle) * impulse;
    float s = sinf(angle) * impulse;
    b2Vec2 center = _body->GetLocalCenter();
    _body->ApplyLinearImpulse(b2Vec2(-s, c), _body->GetWorldPoint(b2Vec2(center.x + kImpulseOffset, center.y)), true);
    _body->ApplyLinearImpulse(b2Vec2(s, -c), _body->GetWorldPoint(b2Vec2(center.x - kImpulseOffset, center.y)), true);
}

// Flash rightPressedActions: clockwise on screen.
void UserVehicle::leanForward()
{
    if (_leaningStrength <= 0.0f) return;
    float angle = _body->GetAngle();
    float factor = (kMaxSpinAV + _body->GetAngularVelocity()) / kMaxSpinAV;
    factor = std::max(0.0f, std::min(1.0f, factor));
    float impulse = _leanImpulse * factor * getTimeStepOverFlashTimeStep();
    float c = cosf(angle) * impulse;
    float s = sinf(angle) * impulse;
    b2Vec2 center = _body->GetLocalCenter();
    _body->ApplyLinearImpulse(b2Vec2(s, -c), _body->GetWorldPoint(b2Vec2(center.x + kImpulseOffset, center.y)), true);
    _body->ApplyLinearImpulse(b2Vec2(-s, c), _body->GetWorldPoint(b2Vec2(center.x - kImpulseOffset, center.y)), true);
}

// handlePositiveSpeed / handleNegativeSpeed (+Pris): ramp the joint's actual speed towards the
// stored motor speed by the acceleration step; a joint turning the wrong way is stopped first.
float UserVehicle::handleSpeed(float jointSpeed, float target, int index)
{
    const float accel = _jointAccels[index] * getTimeStepOverFlashTimeStep();
    if (target >= 0.0f) {
        if (jointSpeed < 0.0f) return 0.0f;
        return jointSpeed < target ? std::min(jointSpeed + accel, target) : jointSpeed;
    }
    if (jointSpeed > 0.0f) return 0.0f;
    return jointSpeed > target ? std::max(jointSpeed - accel, target) : jointSpeed;
}

float UserVehicle::decelerate(float jointSpeed, int index)
{
    const float accel = _jointAccels[index] * getTimeStepOverFlashTimeStep();
    if (jointSpeed > 0.0f) return std::max(jointSpeed - accel, 0.0f);
    if (jointSpeed < 0.0f) return std::min(jointSpeed + accel, 0.0f);
    return 0.0f;
}

void UserVehicle::driveJoints(bool reverse)
{
    for (size_t i = 0; i < _joints.size(); i++) {
        b2Joint* joint = _joints[i];
        if (!joint || _jointMotorSpeeds[i] == 0.0f) continue;
        const float target = reverse ? -_jointMotorSpeeds[i] : _jointMotorSpeeds[i];
        if (joint->GetType() == e_revoluteJoint) {
            b2RevoluteJoint* revolute = static_cast<b2RevoluteJoint*>(joint);
            if (!revolute->IsMotorEnabled()) revolute->EnableMotor(true);
            float speed = handleSpeed(owb2::jointSpeed(revolute), target, (int)i);
            revolute->SetMotorSpeed(speed);
            if (revolute->IsLimitEnabled()) {
                float angle = owb2::jointAngle(revolute);
                if (angle > revolute->GetUpperLimit() && speed > 0.0f) revolute->SetMotorSpeed(0.0f);
                if (angle < revolute->GetLowerLimit() && speed < 0.0f) revolute->SetMotorSpeed(0.0f);
            }
        } else {
            b2PrismaticJoint* prismatic = static_cast<b2PrismaticJoint*>(joint);
            if (!prismatic->IsMotorEnabled()) prismatic->EnableMotor(true);
            float speed = handleSpeed(prismatic->GetJointSpeed(), target, (int)i);
            prismatic->SetMotorSpeed(speed);
            if (prismatic->IsLimitEnabled()) {
                float translation = prismatic->GetJointTranslation();
                if (translation > prismatic->GetUpperLimit() && speed > 0.0f) prismatic->SetMotorSpeed(0.0f);
                if (translation < prismatic->GetLowerLimit() && speed < 0.0f) prismatic->SetMotorSpeed(0.0f);
            }
        }
    }
}

// Neither (or both) of up/down: locked joints brake to a stop, free ones coast (motor off).
void UserVehicle::upAndDownActions()
{
    if (_lockJoints) {
        for (size_t i = 0; i < _joints.size(); i++) {
            b2Joint* joint = _joints[i];
            if (!joint || _jointMotorSpeeds[i] == 0.0f) continue;
            if (joint->GetType() == e_revoluteJoint) {
                b2RevoluteJoint* revolute = static_cast<b2RevoluteJoint*>(joint);
                if (!revolute->IsMotorEnabled()) revolute->EnableMotor(true);
                revolute->SetMotorSpeed(decelerate(owb2::jointSpeed(revolute), (int)i));
            } else {
                b2PrismaticJoint* prismatic = static_cast<b2PrismaticJoint*>(joint);
                if (!prismatic->IsMotorEnabled()) prismatic->EnableMotor(true);
                prismatic->SetMotorSpeed(decelerate(prismatic->GetJointSpeed(), (int)i));
            }
        }
    } else {
        disableMotors();
    }
}

void UserVehicle::disableMotors()
{
    for (size_t i = 0; i < _joints.size(); i++) {
        b2Joint* joint = _joints[i];
        if (!joint || _jointMotorSpeeds[i] == 0.0f) continue;
        if (joint->GetType() == e_revoluteJoint) {
            b2RevoluteJoint* revolute = static_cast<b2RevoluteJoint*>(joint);
            if (revolute->IsMotorEnabled()) revolute->EnableMotor(false);
        } else {
            b2PrismaticJoint* prismatic = static_cast<b2PrismaticJoint*>(joint);
            if (prismatic->IsMotorEnabled()) prismatic->EnableMotor(false);
        }
    }
}

void UserVehicle::brake()
{
    for (size_t i = 0; i < _joints.size(); i++) {
        b2Joint* joint = _joints[i];
        if (!joint || _jointMotorSpeeds[i] == 0.0f) continue;
        if (joint->GetType() == e_revoluteJoint) {
            b2RevoluteJoint* revolute = static_cast<b2RevoluteJoint*>(joint);
            if (!revolute->IsMotorEnabled()) revolute->EnableMotor(true);
            revolute->SetMotorSpeed(0.0f);
        } else {
            b2PrismaticJoint* prismatic = static_cast<b2PrismaticJoint*>(joint);
            if (!prismatic->IsMotorEnabled()) prismatic->EnableMotor(true);
            prismatic->SetMotorSpeed(0.0f);
        }
    }
}

void UserVehicle::setJetsFiring(bool firing)
{
    if (firing == _jetsFiring) return;
    _jetsFiring = firing;
    for (Jet* jet : _jets) {
        jet->*JetAccess::firingAllowed() = firing;
    }
}

void UserVehicle::setArrowsFiring(bool firing)
{
    if (firing == _arrowsFiring) return;
    _arrowsFiring = firing;
    for (ArrowGun* gun : _arrowGuns) {
        gun->*ArrowGunAccess::firingAllowed() = firing;
        if (!firing) {
            gun->*ArrowGunAccess::targetBody() = nullptr;  // ArrowGun.set firingAllowed(false)
        }
    }
}

// Vehicle.actions: while in the actions list (lock joints, nobody driving) the motorised joints
// brake to a stop and are then held there; leaves the list once they all stand still.
void UserVehicle::actions()
{
    float total = 0.0f;
    for (size_t i = 0; i < _joints.size(); i++) {
        b2Joint* joint = _joints[i];
        if (!joint || _jointMotorSpeeds[i] == 0.0f) continue;
        float speed;
        if (joint->GetType() == e_revoluteJoint) {
            b2RevoluteJoint* revolute = static_cast<b2RevoluteJoint*>(joint);
            if (!revolute->IsMotorEnabled()) revolute->EnableMotor(true);
            speed = decelerate(owb2::jointSpeed(revolute), (int)i);
            revolute->SetMotorSpeed(speed);
        } else {
            b2PrismaticJoint* prismatic = static_cast<b2PrismaticJoint*>(joint);
            if (!prismatic->IsMotorEnabled()) prismatic->EnableMotor(true);
            speed = decelerate(prismatic->GetJointSpeed(), (int)i);
            prismatic->SetMotorSpeed(speed);
        }
        total += std::fabs(speed);
    }
    if (total == 0.0f) {
        setSlowing(false);
    }
}

// ---------------------------------------------------------------------------------------------
// Registry

UserVehicle* userVehicleForHandle(b2Fixture* fixture)
{
    LevelVehicles* vehicles = levelVehicles(currentLevel());
    if (!vehicles || !fixture) return nullptr;
    auto it = vehicles->handles.find(fixture);
    return it == vehicles->handles.end() ? nullptr : it->second;
}

UserVehicleRider* userVehicleRider(CharacterB2D* character, bool create)
{
    LevelB2D* level = currentLevel();
    LevelVehicles* vehicles = levelVehicles(level);
    if (!vehicles) {
        if (!create || !level) return nullptr;
        vehicles = &registry()[level];
    }
    auto it = vehicles->riders.find(character);
    if (it != vehicles->riders.end()) return &it->second;
    return create ? &vehicles->riders[character] : nullptr;
}

void destroyUserVehicles(LevelB2D* level)
{
    auto it = registry().find(level);
    if (it == registry().end()) return;
    // Box2D and the destruction listener are already gone (Session::die deletes them first).
    for (UserVehicle* vehicle : it->second.vehicles) {
        vehicle->release();
    }
    registry().erase(it);
}

void setPcExtraKey(unsigned char bit, bool down)
{
    if (down) g_pcExtraBits |= bit;
    else g_pcExtraBits &= (unsigned char)~bit;
}

unsigned char pcExtraControlBits() { return g_pcExtraBits; }

}  // namespace online
