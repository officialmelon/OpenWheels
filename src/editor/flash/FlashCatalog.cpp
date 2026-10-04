// EDITOR (browser features, PC addition): see FlashCatalog.h.
#include "FlashCatalog.h"

#include <map>
#include <sstream>

#include "cocos2d.h"
#include "restored/Restored.h"

namespace flashed {

namespace {

const char* const kCollisionHelp =
    "1 everything, 2 everything but characters, 3 nothing, 4 everything but other 4s, "
    "5 only fixed shapes, 6 fixed shapes and other 6s, 7 only characters";
const char* const kPoseHelp = "pose of the character until it is hit";
const char* const kActionHelp =
    "0 nothing, 1 brake attached joints, 2 fire attached jets, 3 fire attached arrow guns";

// AttributeReference.buildInput, in the order of the Flash switch.
const std::vector<Attr>& attrs()
{
    static const std::vector<Attr> list = {
        {"x", "x", Input::Field, 0, kCanvasWidth, 0, nullptr, 0, ""},
        {"y", "y", Input::Field, 0, kCanvasHeight, 0, nullptr, 0, ""},
        {"shapeWidth", "width", Input::Field, 1, 10000, 0, nullptr, 0, ""},
        {"shapeHeight", "height", Input::Field, 1, 10000, 0, nullptr, 0, ""},
        {"angle", "rotation", Input::Field, -180, 180, 0, nullptr, 0, ""},
        {"interactive", "interactive", Input::Switch, 0, 1, 0, nullptr, 0,
         "off: flat artwork, no physics; moves only inside a group"},
        {"immovable", "fixed", Input::Switch, 0, 1, 0, nullptr, 0, "never moves, supports any weight"},
        {"sleeping", "sleeping", Input::Switch, 0, 1, 0, nullptr, 0, "frozen until touched"},
        {"density", "density", Input::Field, 0.1f, 100, 0, nullptr, 0, ""},
        {"color", "color", Input::Color, 0, 0xffffff, 0, nullptr, 0, ""},
        {"outlineColor", "outline color", Input::Color, -1, 0xffffff, 0, nullptr, 0, ""},
        {"opacity", "opacity", Input::Slider, 0, 100, 100, nullptr, 0, ""},
        {"collision", "collision", Input::Slider, 1, 7, 6, nullptr, 0, kCollisionHelp},
        {"immovable2", "fixed", Input::Switch, 0, 1, 0, nullptr, 0, "never moves, supports any weight"},
        {"immovable3", "fixed", Input::Switch, 0, 1, 0, nullptr, 0, "never moves, supports any weight"},
        {"limit", "limit rotation", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"upperAngle", "upper angle", Input::Slider, 0, 180, 180, "limit", 1, ""},
        {"lowerAngle", "lower angle", Input::Slider, -180, 0, 180, "limit", 1, ""},
        {"motor", "enable motor", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"torque", "motor torque", Input::Field, 0, 100000000, 0, "motor", 1, "force used to turn the joint"},
        {"speed", "motor speed", Input::Slider, -20, 20, 40, "motor", 1, "negative turns the other way"},
        {"axisAngle", "axis angle", Input::Slider, -180, 180, 360, nullptr, 0, "direction the bodies slide along"},
        {"limitPris", "limit range", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"upperLimit", "upper limit", Input::Slider, 0, 3000, 300, "limitPris", 1, ""},
        {"lowerLimit", "lower limit", Input::Slider, -3000, 0, 300, "limitPris", 1, ""},
        {"motorPris", "enable motor", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"force", "motor force", Input::Field, 0, 100000000, 0, "motorPris", 1, "force used to move the joint"},
        {"speedPris", "motor speed", Input::Slider, -50, 50, 100, "motorPris", 1, "negative moves the other way"},
        {"collideSelf", "collide connected", Input::Switch, 0, 1, 0, nullptr, 0,
         "the two joined objects collide with each other"},
        {"ropeLength", "rope length", Input::Slider, 200, 1000, 80, nullptr, 0, ""},
        {"ballSpeed", "ball speed", Input::Slider, 0, 7, 7, nullptr, 0, ""},
        {"springDelay", "delay", Input::Slider, 0, 2, 4, nullptr, 0, ""},
        {"numSpikes", "spikes", Input::Slider, 20, 150, 130, nullptr, 0, ""},
        {"numPanels", "panels", Input::Slider, 1, 6, 5, nullptr, 0, ""},
        {"forceChar", "force character", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"numFloors", "floors", Input::Slider, 3, 50, 47, nullptr, 0, ""},
        {"floorWidth", "floor width", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"useAnchor", "anchor", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"foreground", "foreground", Input::Switch, 0, 1, 0, nullptr, 0, "drawn in front of the player"},
        {"font", "font", Input::Choice, 1, 5, 4, nullptr, 0, "",
         {"Helvetica", "Helvetica medium", "Helvetica bold", "Clarendon", "Clarendon bold"}},
        {"fontSize", "font size", Input::Slider, 10, 100, 90, nullptr, 0, ""},
        {"align", "alignment", Input::Choice, 1, 3, 2, nullptr, 0, "", {"left", "center", "right"}},
        {"caption", "text", Input::Text, 0, 0, 0, nullptr, 0, ""},
        {"charIndex", "character type", Input::Slider, 1, 16, 15, nullptr, 0, ""},
        {"neckAngle", "neck angle", Input::Slider, -20, 20, 40, nullptr, 0, kPoseHelp},
        {"shoulder1Angle", "arm 1 angle", Input::Slider, -180, 60, 240, nullptr, 0, kPoseHelp},
        {"shoulder2Angle", "arm 2 angle", Input::Slider, -180, 60, 240, nullptr, 0, kPoseHelp},
        {"elbow1Angle", "elbow 1 angle", Input::Slider, -160, 0, 160, nullptr, 0, kPoseHelp},
        {"elbow2Angle", "elbow 2 angle", Input::Slider, -160, 0, 160, nullptr, 0, kPoseHelp},
        {"hip1Angle", "leg 1 angle", Input::Slider, -150, 10, 160, nullptr, 0, kPoseHelp},
        {"hip2Angle", "leg 2 angle", Input::Slider, -150, 10, 160, nullptr, 0, kPoseHelp},
        {"knee1Angle", "knee 1 angle", Input::Slider, 0, 150, 150, nullptr, 0, kPoseHelp},
        {"knee2Angle", "knee 2 angle", Input::Slider, 0, 150, 150, nullptr, 0, kPoseHelp},
        {"reverse", "reverse", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"holdPose", "hold pose", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"shatterStrength", "strength", Input::Slider, 1, 10, 9, nullptr, 0, "force needed to shatter the pane"},
        {"stabbing", "stabbing", Input::Switch, 0, 1, 0, nullptr, 0, "broken glass can stab the character"},
        {"bottleType", "bottle type", Input::Slider, 1, 4, 3, nullptr, 0, ""},
        {"signPostType", "sign type", Input::Slider, 1, 13, 12, nullptr, 0, ""},
        {"signPost", "show sign post", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"containsTrash", "contains trash", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"seekSpeed", "speed", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"explosionDelay", "delay", Input::Slider, 0, 5, 5, nullptr, 0, ""},
        {"fixedRotation", "fixed angle", Input::Switch, 0, 1, 0, nullptr, 0, "the object can't rotate"},
        {"rateOfFire", "rate of fire", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"dontShootPlayer", "don't shoot player", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"linkCount", "link count", Input::Slider, 2, 40, 19, nullptr, 0, ""},
        {"linkScale", "link scale", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"linkAngle", "chain curve", Input::Slider, -10, 10, 40, nullptr, 0, ""},
        {"tokenType", "token type", Input::Slider, 1, 6, 5, nullptr, 0, ""},
        {"foodItemType", "food type", Input::Slider, 1, 3, 2, nullptr, 0, ""},
        {"startRotation", "start rotation", Input::Slider, -90, 90, 180, nullptr, 0, "muzzle angle at rest"},
        {"firingRotation", "firing rotation", Input::Slider, -90, 90, 180, nullptr, 0, "muzzle angle when firing"},
        {"muzzleScale", "muzzle scale", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"cannonPower", "power", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"cannonDelay", "delay", Input::Slider, 1, 10, 9, nullptr, 0, "seconds before it fires"},
        {"cannonType", "type", Input::Slider, 1, 2, 1, nullptr, 0, ""},
        {"spaceAction", "spacebar action", Input::Slider, 0, 3, 3, nullptr, 0, kActionHelp},
        {"shiftAction", "shift action", Input::Slider, 0, 3, 3, nullptr, 0, kActionHelp},
        {"ctrlAction", "ctrl action", Input::Slider, 0, 3, 3, nullptr, 0, kActionHelp},
        {"acceleration", "acceleration", Input::Slider, 1, 10, 9, nullptr, 0,
         "how fast attached motors reach their speed"},
        {"leaningStrength", "leaning strength", Input::Slider, 0, 10, 10, nullptr, 0, "0 disables leaning"},
        {"vehicleControlled", "vehicle controlled", Input::Switch, 0, 1, 0, nullptr, 0,
         "off: a normal joint, not driven by the vehicle"},
        {"characterPose", "grabbing pose", Input::Slider, 0, 3, 3, nullptr, 0,
         "0 limp, 1 arms forward, 2 arms overhead, 3 keep the pose"},
        {"lockJoints", "lock joints", Input::Switch, 0, 1, 0, nullptr, 0, "motors slow to 0 when not driving"},
        {"vehicleHandle", "vehicle handle", Input::Switch, 0, 1, 0, nullptr, 0, "the character can grab it"},
        {"innerCutout", "inner cutout", Input::Slider, 0, 100, 100, nullptr, 0, ""},
        {"destroyJointsUponDeath", "release on death", Input::Switch, 0, 1, 0, nullptr, 0,
         "joints and their trigger actions go when it dies"},
        {"paddleAngle", "max angle", Input::Slider, 15, 90, 75, nullptr, 0, ""},
        {"paddleSpeed", "speed", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"triggeredBy", "triggered by", Input::Choice, 1, 6, 5, nullptr, 0, "",
         {"main character", "any character", "any non-fixed shape", "its targets", "other triggers",
          "mouse click"}},
        {"triggerDelay", "delay", Input::Slider, 0, 30, 60, nullptr, 0, "seconds"},
        {"soundEffect", "sound effect", Input::Sound, 0, 0, 0, nullptr, 0, ""},
        {"panning", "panning", Input::Slider, -1, 1, 20, nullptr, 0, ""},
        {"volume", "volume", Input::Slider, 0, 1, 10, nullptr, 0, ""},
        {"triggerType", "action", Input::Choice, 1, 3, 2, nullptr, 0, "",
         {"activate object", "play sound effect", "level victory"}},
        {"repeatType", "repeat type", Input::Choice, 1, 4, 3, nullptr, 0, "",
         {"once", "each time touched", "continuously while touched", "continuously once triggered"}},
        {"repeatInterval", "repeat interval", Input::Slider, 0.1f, 30, 0, nullptr, 0, "seconds between activations"},
        {"soundLocation", "sound location", Input::Choice, 1, 2, 1, nullptr, 0, "", {"global", "at the trigger"}},
        {"startDisabled", "start disabled", Input::Switch, 0, 1, 0, nullptr, 0, "works only once enabled by a trigger"},
        {"fixedAngleTurret", "fixed turret", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"turretAngle", "turret angle", Input::Slider, -110, 110, 220, nullptr, 0, ""},
        {"triggerFiring", "trigger firing", Input::Switch, 0, 1, 0, nullptr, 0, "fires only when triggered"},
        {"startDeactivated", "start deactivated", Input::Switch, 0, 1, 0, nullptr, 0, ""},
        {"boostPower", "boost power", Input::Slider, 10, 100, 90, nullptr, 0, ""},
        {"power", "power", Input::Slider, 1, 10, 9, nullptr, 0, ""},
        {"fireTime", "firing time", Input::Slider, 0, 50, 50, nullptr, 0, "0 never shuts off"},
        {"accelTime", "acceleration time", Input::Slider, 0, 5, 5, nullptr, 0, ""},
        {"bladeWeaponType", "type", Input::Slider, 1, 12, 11, nullptr, 0, ""},
        {"wheelType", "wheel type", Input::Slider, 1, 10, 9, nullptr, 0, ""},
    };
    return list;
}

const std::vector<ActionParam>& params()
{
    // AttributeReference.buildKeyedInput (+ getDefaultValue).
    static const std::vector<ActionParam> list = {
        {"newOpacities", "new opacity", Input::Slider, 0, 100, 100, 100},
        {"opacityTimes", "duration", Input::Slider, 0, 5, 50, 1},
        {"impulseX", "impulse x", Input::Slider, -50, 50, 100, 10},
        {"impulseY", "impulse y", Input::Slider, -50, 50, 100, -10},
        {"spin", "spin", Input::Slider, -20, 20, 40, 0},
        {"slideTimes", "duration", Input::Slider, 0, 10, 100, 1},
        {"newX", "new x", Input::Field, 0, kCanvasWidth, 0, 0},
        {"newY", "new y", Input::Field, 0, kCanvasHeight, 0, 0},
        {"newMotorSpeeds", "new speed", Input::Slider, -20, 20, 40, 0},
        {"motorSpeedTimes", "duration", Input::Slider, 0, 5, 50, 1},
        {"newMotorSpeedsPris", "new speed", Input::Slider, -50, 50, 100, 0},
        {"newUpperLimits", "new upper limit", Input::Slider, 0, 3000, 300, 100},
        {"newLowerLimits", "new lower limit", Input::Slider, -3000, 0, 300, -100},
        {"newUpperAngles", "new upper angle", Input::Slider, 0, 180, 180, 90},
        {"newLowerAngles", "new lower angle", Input::Slider, -180, 0, 180, -90},
        {"newCollisions", "new collision", Input::Slider, 1, 7, 6, 1},
    };
    return list;
}

const std::vector<ActionInfo> kWakeImpulse = {{"wake from sleep", {}}, {"apply impulse", {"impulseX", "impulseY", "spin"}}};
const std::vector<ActionInfo> kNoActions = {};

std::vector<SpecialInfo> buildSpecials()
{
    // The browser specials the iOS editor has no ref for. Defaults are the Flash refs' field
    // initialisers; footprints approximate the Flash editor art (used when it isn't generated).
    std::vector<SpecialInfo> s = {
        {1, "table", {"x", "y", "angle", "sleeping", "interactive"}, {"sleeping", "interactive"},
         {{"interactive", 1}}, 4, 1, true, "ed_table", 156, 60},
        {11, "meteor", {"x", "y", "shapeWidth", "shapeHeight", "immovable2", "sleeping"},
         {"shapeWidth", "immovable2", "sleeping"}, {{"shapeWidth", 400}, {"shapeHeight", 400}}, 1, 0, false,
         "ed_meteor", 400, 400},
        {13, "building 1", {"x", "y", "floorWidth", "numFloors"}, {"floorWidth", "numFloors"},
         {{"floorWidth", 1}, {"numFloors", 3}}, 1, 0, false, "", 300, 595},
        {14, "building 2", {"x", "y", "floorWidth", "numFloors"}, {"floorWidth", "numFloors"},
         {{"floorWidth", 1}, {"numFloors", 3}}, 1, 0, false, "", 300, 595},
        {16, "text box", {"x", "y", "angle", "color", "font", "fontSize", "align", "caption", "opacity"},
         {"caption", "color", "opacity", "font", "fontSize", "align"},
         {{"font", 2}, {"fontSize", 15}, {"align", 1}, {"opacity", 100}}, 0, 0, true, "", 150, 20},
        {17, "non-player character",
         {"x", "y", "angle", "charIndex", "sleeping", "reverse", "holdPose", "interactive", "neckAngle",
          "shoulder1Angle", "shoulder2Angle", "elbow1Angle", "elbow2Angle", "hip1Angle", "hip2Angle", "knee1Angle",
          "knee2Angle", "destroyJointsUponDeath"},
         {"charIndex", "sleeping", "reverse", "holdPose", "interactive", "neckAngle", "shoulder1Angle",
          "shoulder2Angle", "elbow1Angle", "elbow2Angle", "hip1Angle", "hip2Angle", "knee1Angle", "knee2Angle",
          "destroyJointsUponDeath"},
         {{"charIndex", 1}, {"interactive", 1}}, 24, 10, true, "ed_npc_1", 40, 190},
        {18, "glass panel", {"x", "y", "shapeWidth", "shapeHeight", "angle", "sleeping", "shatterStrength", "stabbing"},
         {"shapeWidth", "shapeHeight", "sleeping", "shatterStrength", "stabbing"},
         {{"shapeWidth", 10}, {"shapeHeight", 100}, {"shatterStrength", 10}, {"stabbing", 1}}, 16, 0, true, "", 10,
         100},
        {19, "chair", {"x", "y", "angle", "reverse", "sleeping", "interactive"}, {"reverse", "sleeping", "interactive"},
         {{"interactive", 1}}, 4, 1, true, "ed_chair", 42, 88},
        {21, "TV", {"x", "y", "angle", "sleeping", "interactive"}, {"sleeping", "interactive"}, {{"interactive", 1}}, 3,
         1, true, "ed_tv", 55, 40},
        {22, "boombox", {"x", "y", "angle", "sleeping", "interactive"}, {"sleeping", "interactive"},
         {{"interactive", 1}}, 2, 1, true, "ed_boombox", 48, 28},
        {23, "sign", {"x", "y", "angle", "signPostType", "signPost"}, {"signPostType", "signPost"},
         {{"signPostType", 1}, {"signPost", 1}}, 0, 1, true, "ed_sign_1", 110, 90},
        {24, "toilet", {"x", "y", "angle", "reverse", "sleeping", "interactive"}, {"reverse", "sleeping", "interactive"},
         {{"interactive", 1}}, 5, 1, true, "ed_toilet", 62, 78},
        {26, "trash can", {"x", "y", "angle", "sleeping", "interactive", "containsTrash"},
         {"sleeping", "interactive", "containsTrash"}, {{"interactive", 1}, {"containsTrash", 1}}, 13, 1, true,
         "ed_trashcan", 46, 64},
        {27, "rail", {"x", "y", "shapeWidth", "shapeHeight", "angle"}, {"shapeWidth"},
         {{"shapeWidth", 250}, {"shapeHeight", 18}}, 2, 0, true, "ed_rail", 250, 18},
        {30, "chain", {"x", "y", "angle", "sleeping", "interactive", "linkCount", "linkScale", "linkAngle"},
         {"sleeping", "interactive", "linkCount", "linkScale", "linkAngle"},
         {{"interactive", 1}, {"linkCount", 20}, {"linkScale", 1}}, 20, 20, true, "", 10, 180},
        {31, "token", {"x", "y", "tokenType"}, {"tokenType"}, {{"tokenType", 1}}, 1, 0, false, "coin_face_1", 40,
         40},
        {32, "food item", {"x", "y", "angle", "sleeping", "interactive", "foodItemType"},
         {"sleeping", "interactive", "foodItemType"}, {{"interactive", 1}, {"foodItemType", 1}}, 3, 1, true,
         "ed_food_1", 50, 40},
        {33, "cannon",
         {"x", "y", "angle", "startRotation", "firingRotation", "cannonType", "cannonDelay", "muzzleScale",
          "cannonPower"},
         {"startRotation", "firingRotation", "cannonType", "cannonDelay", "muzzleScale", "cannonPower"},
         {{"cannonType", 1}, {"cannonDelay", 1}, {"muzzleScale", 1}, {"cannonPower", 5}}, 3, 0, true, "ed_cannon",
         110, 200},
        {35, "paddle", {"x", "y", "angle", "springDelay", "reverse", "paddleAngle", "paddleSpeed"},
         {"springDelay", "reverse", "paddleAngle", "paddleSpeed"}, {{"paddleAngle", 90}, {"paddleSpeed", 10}}, 3, 0,
         true, "ed_paddle", 350, 40},
    };
    return s;
}

const std::vector<SpecialInfo>& specials()
{
    static const std::vector<SpecialInfo> list = buildSpecials();
    return list;
}

}  // namespace

const Attr* attribute(const std::string& key)
{
    static std::map<std::string, const Attr*> index;
    if (index.empty())
        for (const Attr& a : attrs()) index[a.key] = &a;
    auto it = index.find(key);
    return it == index.end() ? nullptr : it->second;
}

const ActionParam* actionParam(const std::string& key)
{
    for (const ActionParam& p : params())
        if (key == p.key) return &p;
    return nullptr;
}

const std::vector<ActionInfo>* targetActions(TargetKind kind, int type)
{
    static const std::vector<ActionInfo> shape = {
        {"wake from sleep", {}},
        {"set to fixed", {}},
        {"set to non fixed", {}},
        {"change opacity", {"newOpacities", "opacityTimes"}},
        {"apply impulse", {"impulseX", "impulseY", "spin"}},
        {"delete shape", {}},
        {"delete self", {}},
        {"change collision", {"newCollisions"}},
    };
    static const std::vector<ActionInfo> group = {
        {"wake from sleep", {}},
        {"change opacity", {"newOpacities", "opacityTimes"}},
        {"apply impulse", {"impulseX", "impulseY", "spin"}},
        {"set to fixed", {}},
        {"set to non fixed", {}},
        {"delete shapes", {}},
        {"delete self", {}},
        {"change collision", {"newCollisions"}},
    };
    static const std::vector<ActionInfo> pin = {
        {"disable motor", {}},
        {"change motor speed", {"newMotorSpeeds", "motorSpeedTimes"}},
        {"delete self", {}},
        {"disable limits", {}},
        {"change limits", {"newUpperAngles", "newLowerAngles"}},
    };
    static const std::vector<ActionInfo> pris = {
        {"disable motor", {}},
        {"change motor speed", {"newMotorSpeedsPris", "motorSpeedTimes"}},
        {"delete self", {}},
        {"disable limits", {}},
        {"change limits", {"newUpperLimits", "newLowerLimits"}},
    };
    static const std::vector<ActionInfo> trigger = {{"activate trigger", {}}, {"disable", {}}, {"enable", {}}};
    static const std::vector<ActionInfo> glass = {
        {"shatter", {}}, {"wake from sleep", {}}, {"apply impulse", {"impulseX", "impulseY", "spin"}}};
    static const std::vector<ActionInfo> harpoon = {{"fire harpoon", {}}, {"deactivate", {}}, {"activate", {}}};
    static const std::vector<ActionInfo> text = {{"change opacity", {"newOpacities", "opacityTimes"}},
                                                 {"slide", {"slideTimes", "newX", "newY"}}};
    static const std::vector<ActionInfo> npc = {{"wake from sleep", {}},
                                                {"apply impulse", {"impulseX", "impulseY", "spin"}},
                                                {"hold pose", {}},
                                                {"release pose", {}}};
    static const std::vector<ActionInfo> chain = {{"wake from sleep", {}}, {"apply impulse", {"impulseX", "impulseY"}}};
    switch (kind)
    {
    case TargetKind::Shape: return &shape;
    case TargetKind::Group: return &group;
    case TargetKind::PinJoint: return &pin;
    case TargetKind::PrisJoint: return &pris;
    case TargetKind::Trigger: return &trigger;
    case TargetKind::Special: break;
    }
    switch (type)
    {
    case 0: case 1: case 3: case 4: case 6: case 11: case 19: case 20: case 21: case 22: case 24: case 26: case 32:
    case 34:
        return &kWakeImpulse;
    case 2: case 7: case 8: case 12: case 25:
        return &kNoActions;   // triggerable without an action list: the target is activated
    case 15: return &harpoon;
    case 16: return &text;
    case 17: return &npc;
    case 18: return &glass;
    case 30: return &chain;
    default: return nullptr;
    }
}

const SpecialInfo* specialInfo(int type)
{
    for (const SpecialInfo& s : specials())
        if (s.type == type) return &s;
    return nullptr;
}

const std::vector<int>& flashSpecialTypes()
{
    static std::vector<int> types;
    if (types.empty())
        for (const SpecialInfo& s : specials()) types.push_back(s.type);
    return types;
}

int characterCount() { return 11; }

const char* characterName(int id)
{
    static const char* const names[] = {"",
                                        "Wheelchair Guy",
                                        "Segway Guy",
                                        "Irresponsible Dad",
                                        "Effective Shopper",
                                        "Moped Couple",
                                        "Lawnmower Man",
                                        "Explorer Guy",
                                        "Santa Claus",
                                        "Pogostick Man",
                                        "Irresponsible Mom",
                                        "Helicopter Man"};
    return id >= 1 && id <= 11 ? names[id] : "";
}

bool characterPlayable(int id)
{
    switch (id)
    {
    case 1: case 2: case 3: case 4: case 5: case 9: return true;
    default: return id >= 1 && id <= 11 && restored::hasCharacter(id);
    }
}

const std::vector<std::string>& soundNames()
{
    static std::vector<std::string> names;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        std::istringstream in(cocos2d::FileUtils::getInstance()->getStringFromFile("soundlist.tsv"));
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const size_t a = line.find('\t');
            if (a == std::string::npos) continue;
            const size_t b = line.find('\t', a + 1);
            const int id = std::atoi(line.substr(0, a).c_str());
            if (id < 0 || id > 100000) continue;
            if ((int)names.size() <= id) names.resize(id + 1);
            names[id] = line.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
        }
    }
    return names;
}

}  // namespace flashed
