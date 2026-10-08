#pragma once
// ONLINE (PC addition): browser user-built vehicles (Flash level/groups/Vehicle.as).
//
// A browser group saved with v="t" is a RefVehicle. FlashLevelConverter keeps its settings on the
// converted <g> (v sb sh ct a l cp lo), marks group shapes that are not handles with vh="f" and
// joints that are not vehicle controlled with vc="f". LevelB2D::addGroup / addJoint report them
// here (only for converted levels), and the player character attaches itself when one of its
// grabbing hands touches a handle shape (CharacterB2D::onlineGrabUserVehicle, see
// UserVehicleRider.cpp). While attached, the character's control bits drive the vehicle instead
// of its own poses: up/down (0x01/0x02) run the vehicle-controlled joint motors, lean
// (0x04/0x08) applies the lean impulse, space/shift/ctrl (0x10/0x20/0x40) trigger the vehicle's
// assigned actions (brake, jets, arrow guns) and 0x80 ejects.
//
// Everything is owned by the level: destroyUserVehicles() runs from LevelB2D's destructor.

#include <string>
#include <vector>

#include "Box2D/Box2D.h"
#include "LevelItem.h"

class ArrowGun;
class CharacterB2D;
class Jet;
class LevelB2D;
class LevelDataElement;

namespace online {

class UserVehicle : public LevelItem
{
public:
    // Flash RefVehicle action ids for space / shift / ctrl.
    enum Action { ActionNone = 0, ActionBrake = 1, ActionJets = 2, ActionArrows = 3 };

    // LevelB2D::addGroup: a vehicle for a converted <g v="t">, else nullptr.
    static UserVehicle* createForGroup(LevelB2D* level, LevelDataElement* group, b2Body* body,
                                       int groupIndex);

    // LevelB2D::addGroup, after each <sh> was added to the group body: `previousFirst` is the
    // body's fixture list head before the shape was added (Box2D prepends new fixtures).
    void shapeAdded(LevelDataElement* shape, b2Body* body, b2Fixture* previousFirst);
    // LevelB2D::addGroup, for each special created inside the group (Flash: arrow guns only).
    void groupSpecialAdded(LevelItem* special);
    // LevelB2D::addGroup once the group is complete (body null: the group got no fixture).
    void groupFinished(b2Body* body);

    // Flash Vehicle.characterPose (0 none, 1 arms forward, 2 arms overhead, 3 hold position).
    int getCharacterPose() const { return _characterPose; }

    void addCharacter(CharacterB2D* character);
    void removeCharacter(CharacterB2D* character);
    // Flash Vehicle.operateKeys with the mobile control bits (0x01 up, 0x02 down, 0x04 lean
    // forward = Flash right, 0x08 lean back = Flash left, 0x10 space, 0x20 shift, 0x40 ctrl,
    // 0x80 eject = Flash z).
    void operateKeys(unsigned int iteration, unsigned char state);

    // LevelItem
    void actions() override;
    void jointWillBeDestroyed(b2Joint* joint) override;

    // Hooks used by the level loader / character (implemented in UserVehicle.cpp).
    void checkAddSpecial(LevelItem* item);
    void checkAddVehicle(UserVehicle* vehicle);
    void addJoint(b2Joint* joint);

    UserVehicle() {}
    ~UserVehicle() override {}

private:
    void setShitUp();
    void leanBack();     // Flash leftPressedActions
    void leanForward();  // Flash rightPressedActions
    void driveJoints(bool reverse);  // Flash upPressedActions / downPressedActions
    void upAndDownActions();
    void brake();
    void setJetsFiring(bool firing);
    void setArrowsFiring(bool firing);
    void disableMotors();
    float handleSpeed(float jointSpeed, float target, int index);
    float decelerate(float jointSpeed, int index);
    void setSlowing(bool on);  // Flash actionsVector membership

    LevelB2D* _level = nullptr;
    b2Body* _body = nullptr;
    int _groupIndex = -1;
    float _acceleration = 1.0f;
    float _leaningStrength = 0.0f;
    float _leanImpulse = 0.0f;
    int _spaceAction = 0;
    int _shiftAction = 0;
    int _ctrlAction = 0;
    bool _lockJoints = false;
    int _characterPose = 0;
    bool _slowAfterEject = false;
    bool _slowing = false;
    bool _jetsFiring = false;
    bool _arrowsFiring = false;
    unsigned int _lastIteration = 0;
    bool _operated = false;

    std::vector<b2Joint*> _joints;          // null once destroyed
    std::vector<float> _jointMotorSpeeds;   // 0 = not motorised (Flash null)
    std::vector<float> _jointAccels;        // per Flash frame
    std::vector<Jet*> _jets;
    std::vector<ArrowGun*> _arrowGuns;
    std::vector<CharacterB2D*> _characters;
    std::vector<UserVehicle*> _vehicles;
    std::vector<b2Fixture*> _handles;
};

// Flash CharacterB2D.userVehicle / vehicleArm1Joint / vehicleArm2Joint of one character.
struct UserVehicleRider
{
    UserVehicle* vehicle = nullptr;
    b2RevoluteJoint* arm1Joint = nullptr;
    b2RevoluteJoint* arm2Joint = nullptr;
};
// The record of `character` in the current level (created when `create`), or nullptr.
UserVehicleRider* userVehicleRider(CharacterB2D* character, bool create);

// The user vehicle whose handle `fixture` is (current level only), or nullptr.
UserVehicle* userVehicleForHandle(b2Fixture* fixture);

// LevelB2D::addJoint for converted levels: `joint` was just created from `element`, whose b1/b2
// are the converted body references ("g<n>", "s<n>", "<n>").
void userVehicleJointCreated(LevelB2D* level, LevelDataElement* element, b2Joint* joint);

// LevelB2D::~LevelB2D: frees the level's vehicles and rider records (Box2D is already gone).
void destroyUserVehicles(LevelB2D* level);

// --- PC keyboard: keys without an on-screen button ---------------------------------------------
// Shift (0x20) and Ctrl (0x40) are the browser game's secondary action keys; Z (0x80) ejects from
// a user vehicle, whose rider uses the ejected control layout (no eject button); Space (0x10) is
// also reported, so the vehicle's space action does not depend on a 0x10 button being on screen
// (or on the key's virtual finger following a layout change). PCInput reports them,
// Gameplay::update ORs them into the control byte of converted levels.
void setPcExtraKey(unsigned char bit, bool down);
unsigned char pcExtraControlBits();

}  // namespace online
