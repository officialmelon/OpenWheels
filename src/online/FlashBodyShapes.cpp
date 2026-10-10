#include "FlashBodyShapes.h"

#include <cstdlib>

namespace online {

namespace {

struct Part {
    const char* key;
    const char* pos;   // offset from the character's start, metres (y up)
    float rot;         // radians
    const char* size;  // box half extents, or nullptr for a circle
    float radius;
};

struct Anchor {
    const char* key;
    const char* pos;
};

struct Character {
    const char* name;
    Part parts[11];
    Anchor anchors[10];  // the rest are empty
};

// Read from the browser game's world as each character is created (positions, angles and joint
// anchors on its 0.4 mm grid; sizes as the browser computes them from its art's scale).
const Character kCharacters[] = {
    {"business_guy_personal_transporter",
     {{"headShape", "{0.1136,0.2632}", -0.175f, nullptr, 0.13172f},
      {"chestShape", "{0.0256,-0.1912}", 0.066f, "{0.17108,0.33076}", 0.0f},
      {"pelvisShape", "{0.0304,-0.552}", 0.0f, "{0.17072,0.132}", 0.0f},
      {"upperArm1Shape", "{0.0184,-0.1112}", 0.249f, "{0.07632,0.22604}", 0.0f},
      {"upperArm2Shape", "{0.0168,-0.1048}", 0.21f, "{0.07196,0.20784}", 0.0f},
      {"lowerArm1Shape", "{0.2304,-0.3968}", 0.901f, "{0.06788,0.23796}", 0.0f},
      {"lowerArm2Shape", "{0.2224,-0.3736}", 0.945f, "{0.06828,0.23344}", 0.0f},
      {"upperLeg1Shape", "{-0.0008,-0.8216}", -0.101f, "{0.13912,0.34212}", 0.0f},
      {"upperLeg2Shape", "{0.0208,-0.7952}", -0.075f, "{0.1306,0.33928}", 0.0f},
      {"lowerLeg1Shape", "{-0.0504,-1.292}", -0.075f, "{0.084,0.30792}", 0.0f},
      {"lowerLeg2Shape", "{-0.04,-1.2952}", -0.105f, "{0.09168,0.31324}", 0.0f}},
     {{"headAnchor", "{0.008,-0.0344}"},
      {"upperArmAnchor", "{-0.004,0.02}"},
      {"lowerArm1Anchor", "{0.0768,-0.272}"},
      {"lowerArm2Anchor", "{0.0624,-0.2552}"},
      {"pelvisAnchor", "{0.0276,-0.4656}"},
      {"upperLegAnchor", "{0.012,-0.5944}"},
      {"lowerLeg1Anchor", "{-0.0608,-1.0464}"},
      {"lowerLeg2Anchor", "{-0.0096,-1.0424}"}}},
    {"irresponsible_dad_kid_road_bike",
     {{"headShape", "{-0.7592,-0.0288}", 0.202f, nullptr, 0.11032f},
      {"chestShape", "{-0.7504,-0.2912}", 0.223f, "{0.08632,0.14828}", 0.0f},
      {"pelvisShape", "{-0.7064,-0.488}", 0.258f, "{0.09392,0.07196}", 0.0f},
      {"upperArm1Shape", "{-0.696,-0.264}", 1.047f, "{0.0474,0.1456}", 0.0f},
      {"upperArm2Shape", "{-0.692,-0.2208}", 1.535f, "{0.0474,0.14556}", 0.0f},
      {"lowerArm1Shape", "{-0.5272,-0.2872}", 1.942f, "{0.03396,0.11748}", 0.0f},
      {"lowerArm2Shape", "{-0.5248,-0.1568}", 2.402f, "{0.03396,0.1174}", 0.0f},
      {"upperLeg1Shape", "{-0.5876,-0.4876}", 1.672f, "{0.05996,0.17192}", 0.0f},
      {"upperLeg2Shape", "{-0.5928,-0.4784}", 1.82f, "{0.05992,0.17172}", 0.0f},
      {"lowerLeg1Shape", "{-0.4256,-0.6184}", 0.157f, "{0.05236,0.1718}", 0.0f},
      {"lowerLeg2Shape", "{-0.4048,-0.5752}", 0.38f, "{0.05236,0.1718}", 0.0f}},
     {{"headAnchor", "{-0.7696,-0.2368}"},
      {"upperArmAnchor", "{-0.7752,-0.22}"},
      {"lowerArm1Anchor", "{-0.604,-0.3232}"},
      {"lowerArm2Anchor", "{-0.576,-0.2208}"},
      {"pelvisAnchor", "{-0.7224,-0.4156}"},
      {"upperLegAnchor", "{-0.7072,-0.5048}"},
      {"lowerLeg1Anchor", "{-0.4392,-0.4752}"},
      {"lowerLeg2Anchor", "{-0.4512,-0.4432}"}}},
    {"irresponsible_dad_road_bike",
     {{"headShape", "{0.3272,0.268}", -0.713f, nullptr, 0.1318f},
      {"chestShape", "{0.0152,-0.0088}", -0.577f, "{0.15576,0.2756}", 0.0f},
      {"pelvisShape", "{-0.2104,-0.3368}", -0.577f, "{0.14376,0.1278}", 0.0f},
      {"upperArm1Shape", "{0.1272,-0.0152}", 0.262f, "{0.07988,0.21968}", 0.0f},
      {"upperArm2Shape", "{0.136,-0.0136}", 0.262f, "{0.07996,0.21988}", 0.0f},
      {"lowerArm1Shape", "{0.356,-0.2928}", 0.993f, "{0.07988,0.2436}", 0.0f},
      {"lowerArm2Shape", "{0.3544,-0.288}", 0.902f, "{0.0798,0.24332}", 0.0f},
      {"upperLeg1Shape", "{0.0128,-0.4456}", 1.073f, "{0.10984,0.33948}", 0.0f},
      {"upperLeg2Shape", "{-0.024,-0.5112}", 0.783f, "{0.1098,0.3394}", 0.0f},
      {"lowerLeg1Shape", "{0.1848,-0.8384}", -0.192f, "{0.07992,0.31964}", 0.0f},
      {"lowerLeg2Shape", "{-0.0016,-0.8928}", -0.608f, "{0.07988,0.31948}", 0.0f}},
     {{"headAnchor", "{0.0888,0.1056}"},
      {"upperArmAnchor", "{0.0984,0.1176}"},
      {"lowerArm1Anchor", "{0.1752,-0.1904}"},
      {"lowerArm2Anchor", "{0.1952,-0.1584}"},
      {"pelvisAnchor", "{-0.1404,-0.2336}"},
      {"upperLegAnchor", "{-0.2216,-0.3224}"},
      {"lowerLeg1Anchor", "{0.2488,-0.572}"},
      {"lowerLeg2Anchor", "{0.1608,-0.6816}"}}},
    {"moped_girl_moped",
     {{"headShape", "{-0.2008,0.3272}", -0.158f, nullptr, 0.11992f},
      {"chestShape", "{-0.2968,-0.0824}", -0.158f, "{0.10648,0.27232}", 0.0f},
      {"pelvisShape", "{-0.3704,-0.4144}", -0.158f, "{0.10528,0.10676}", 0.0f},
      {"upperArm1Shape", "{-0.2712,-0.0184}", 0.101f, "{0.05836,0.17788}", 0.0f},
      {"upperArm2Shape", "{-0.2808,-0.0184}", 0.009f, "{0.0584,0.178}", 0.0f},
      {"lowerArm1Shape", "{-0.08,-0.2088}", 1.317f, "{0.05236,0.21904}", 0.0f},
      {"lowerArm2Shape", "{-0.1056,-0.2152}", 1.287f, "{0.05236,0.219}", 0.0f},
      {"upperLeg1Shape", "{-0.2048,-0.5544}", 0.84f, "{0.09264,0.29712}", 0.0f},
      {"upperLeg2Shape", "{-0.1984,-0.5464}", 0.893f, "{0.09264,0.29712}", 0.0f},
      {"lowerLeg1Shape", "{-0.2136,-0.8944}", -0.731f, "{0.06788,0.30192}", 0.0f},
      {"lowerLeg2Shape", "{-0.1864,-0.888}", -0.665f, "{0.06788,0.30192}", 0.0f}},
     {{"headAnchor", "{-0.268,0.074}"},
      {"upperArmAnchor", "{-0.28,0.0836}"},
      {"lowerArm1Anchor", "{-0.2544,-0.1632}"},
      {"lowerArm2Anchor", "{-0.278,-0.1644}"},
      {"pelvisAnchor", "{-0.3468,-0.33}"},
      {"upperLegAnchor", "{-0.3724,-0.424}"},
      {"lowerLeg1Anchor", "{-0.0328,-0.7168}"},
      {"lowerLeg2Anchor", "{-0.0176,-0.6984}"},
      {"handleAnchor", "{0.058,-0.256}"},
      {"footAnchor", "{-0.362,-1.0956}"}}},
    {"moped_guy_moped",
     {{"headShape", "{0.1608,0.3648}", -0.148f, nullptr, 0.132f},
      {"chestShape", "{0.0152,-0.0464}", -0.148f, "{0.156,0.276}", 0.0f},
      {"pelvisShape", "{-0.0464,-0.4336}", -0.148f, "{0.144,0.128}", 0.0f},
      {"upperArm1Shape", "{0.0904,-0.0136}", 0.48f, "{0.07988,0.21968}", 0.0f},
      {"upperArm2Shape", "{0.0808,-0.004}", 0.0f, "{0.07984,0.2196}", 0.0f},
      {"lowerArm1Shape", "{0.3352,-0.2352}", 1.147f, "{0.07992,0.24368}", 0.0f},
      {"lowerArm2Shape", "{0.3496,-0.236}", 1.147f, "{0.0798,0.2434}", 0.0f},
      {"upperLeg1Shape", "{0.1808,-0.5632}", 1.117f, "{0.11,0.34}", 0.0f},
      {"upperLeg2Shape", "{0.1816,-0.548}", 1.161f, "{0.11,0.33996}", 0.0f},
      {"lowerLeg1Shape", "{0.2472,-0.9144}", -0.569f, "{0.07988,0.31948}", 0.0f},
      {"lowerLeg2Shape", "{0.2528,-0.8936}", -0.564f, "{0.07988,0.31948}", 0.0f}},
     {{"headAnchor", "{0.0392,0.0968}"},
      {"upperArmAnchor", "{0.0376,0.106}"},
      {"lowerArm1Anchor", "{0.1656,-0.1608}"},
      {"lowerArm2Anchor", "{0.1732,-0.1468}"},
      {"pelvisAnchor", "{-0.026,-0.3136}"},
      {"upperLegAnchor", "{-0.0508,-0.4528}"},
      {"lowerLeg1Anchor", "{0.4016,-0.676}"},
      {"lowerLeg2Anchor", "{0.4068,-0.6536}"},
      {"handleAnchor", "{0.52,-0.304}"},
      {"footAnchor", "{0.098,-1.1156}"}}},
    {"pogo_stick_guy_pogo_stick",
     {{"headShape", "{0.21,0.3828}", -0.36f, nullptr, 0.128f},
      {"chestShape", "{0,0}", -0.36f, "{0.1284,0.28}", 0.0f},
      {"pelvisShape", "{-0.148,-0.3676}", -0.359f, "{0.14,0.132}", 0.0f},
      {"upperArm1Shape", "{0.0428,0.0056}", -0.049f, "{0.072,0.22}", 0.0f},
      {"upperArm2Shape", "{0.0724,0.0072}", 0.084f, "{0.072,0.216}", 0.0f},
      {"lowerArm1Shape", "{0.204,-0.3016}", 0.871f, "{0.06,0.2396}", 0.0f},
      {"lowerArm2Shape", "{0.2296,-0.326}", 0.756f, "{0.06,0.24}", 0.0f},
      {"upperLeg1Shape", "{0.048,-0.532}", 0.848f, "{0.104,0.328}", 0.0f},
      {"upperLeg2Shape", "{0.0496,-0.5312}", 0.857f, "{0.104,0.328}", 0.0f},
      {"lowerLeg1Shape", "{0.1736,-0.98}", -0.185f, "{0.08,0.304}", 0.0f},
      {"lowerLeg2Shape", "{0.1736,-0.9596}", -0.22f, "{0.08,0.304}", 0.0f}},
     {{"headAnchor", "{0.0472,0.126}"},
      {"upperArmAnchor", "{0.0548,0.146}"},
      {"lowerArm1Anchor", "{0.0344,-0.1744}"},
      {"lowerArm2Anchor", "{0.0744,-0.1636}"},
      {"pelvisAnchor", "{-0.0944,-0.2456}"},
      {"upperLegAnchor", "{-0.1528,-0.3592}"},
      {"lowerLeg1Anchor", "{0.222,-0.7036}"},
      {"lowerLeg2Anchor", "{0.2308,-0.694}"}}},
    {"wheelchair_guy_wheelchair",
     {{"headShape", "{0.0432,0.412}", 0.0f, nullptr, 0.132f},
      {"chestShape", "{0,-0.0008}", 0.0f, "{0.156,0.276}", 0.0f},
      {"pelvisShape", "{-0.004,-0.3928}", 0.0f, "{0.144,0.128}", 0.0f},
      {"upperArm1Shape", "{0.1056,0.0776}", 1.011f, "{0.07988,0.21968}", 0.0f},
      {"upperArm2Shape", "{0.1176,0.168}", 1.741f, "{0.07996,0.21988}", 0.0f},
      {"lowerArm1Shape", "{0.4296,0.0112}", 1.71f, "{0.08,0.244}", 0.0f},
      {"lowerArm2Shape", "{0.3648,0.3984}", 2.726f, "{0.07992,0.24372}", 0.0f},
      {"upperLeg1Shape", "{0.2528,-0.4112}", 1.571f, "{0.11,0.34}", 0.0f},
      {"upperLeg2Shape", "{0.2472,-0.408}", 1.571f, "{0.11,0.34}", 0.0f},
      {"lowerLeg1Shape", "{0.6424,-0.6624}", 0.524f, "{0.08,0.32}", 0.0f},
      {"lowerLeg2Shape", "{0.636,-0.6624}", 0.524f, "{0.08,0.32}", 0.0f}},
     {{"headAnchor", "{0.0032,0.128}"},
      {"upperArmAnchor", "{-0.0004,0.1536}"},
      {"lowerArm1Anchor", "{0.2456,-0.01}"},
      {"lowerArm2Anchor", "{0.278,0.22}"},
      {"pelvisAnchor", "{-0.0024,-0.2656}"},
      {"upperLegAnchor", "{-0.004,-0.3924}"},
      {"lowerLeg1Anchor", "{0.5008,-0.414}"},
      {"lowerLeg2Anchor", "{0.4956,-0.4108}"}}},
};

}  // namespace

void applyFlashCharacterShapes(const std::string& name, cocos2d::ValueMap& bodiesDict)
{
    using cocos2d::Value;
    using cocos2d::ValueMap;
    for (const Character& character : kCharacters) {
        if (name != character.name) {
            continue;
        }
        ValueMap& bodies = bodiesDict["bodies"].asValueMap();
        for (const Part& part : character.parts) {
            ValueMap& shape = bodies[part.key].asValueMap();
            shape["pos"] = Value(part.pos);
            shape["rot"] = Value(part.rot);
            if (part.size != nullptr) {
                shape["size"] = Value(part.size);
            } else {
                shape["radius"] = Value(part.radius);
            }
        }
        ValueMap& joints = bodiesDict["joints"].asValueMap();
        for (const Anchor& anchor : character.anchors) {
            if (anchor.key == nullptr) {
                break;
            }
            joints[anchor.key] = Value(anchor.pos);
        }
        return;
    }
}

namespace {

struct VehicleValue {
    const char* vehicle;
    const char* body;   // a "bodies" entry, or nullptr for a "joints" anchor
    const char* field;  // pos, size, rot, radius, verts (or the anchor's name)
    const char* value;
};

// Read from the browser game's world as each vehicle is created (relative to the character's start,
// y up).
const VehicleValue kVehicleValues[] = {
    {"moped", "frontWheelShape", "pos", "{1,-1.128}"},
    {"moped", "frontWheelShape", "radius", "0.36"},
    {"moped", "backWheelShape", "pos", "{-0.488,-1.128}"},
    {"moped", "backWheelShape", "radius", "0.36"},
    {"moped", "fork", "verts", "{1.016,-1.1656}:{0.682,-0.3576}:{0.458,-0.3336}"},
    {"moped", "tank", "verts", "{0.084,-0.9916}:{0.8524,-0.638}:{0.8704,-0.478}:{0.686,-0.478}:{0.084,-0.8936}"},
    {"moped", "engine", "verts", "{-0.008,-1.1776}:{0.204,-1.3196}:{0.462,-1.2936}:{0.462,-0.9996}:{0.004,-0.8776}"},
    {"moped", "middle", "verts", "{-0.518,-1.1536}:{0.156,-1.162}:{-0.0292,-0.6564}:{-0.276,-0.6564}"},
    {"moped", "rear", "verts", "{-0.87,-0.9588}:{-0.1524,-0.6564}:{-0.7964,-0.6564}"},
    {"moped", "seat", "verts", "{-0.672,-0.638}:{0.046,-0.648}:{0.114,-0.474}:{-0.68,-0.462}"},
    {"moped", nullptr, "handleAnchor", "{0.52,-0.304}"},
    {"moped", nullptr, "footAnchor", "{0.098,-1.1156}"},
    {"personal_transporter", "standShape", "pos", "{0.072,-1.5496}"},
    {"personal_transporter", "standShape", "size", "{0.17472,0.09196}"},
    {"personal_transporter", "standShape", "rot", "0"},
    {"personal_transporter", "frameShape", "pos", "{0.2772,-1.0028}"},
    {"personal_transporter", "frameShape", "size", "{0.03988,0.58664}"},
    {"personal_transporter", "frameShape", "rot", "-0.20096"},
    {"personal_transporter", "handleShape", "pos", "{0.3396,-0.4972}"},
    {"personal_transporter", "handleShape", "size", "{0.02804,0.14152}"},
    {"personal_transporter", "handleShape", "rot", "1.632"},
    {"personal_transporter", "wheelShape", "pos", "{0.0688,-1.5496}"},
    {"personal_transporter", "wheelShape", "radius", "0.23976"},
    {"personal_transporter", nullptr, "handleAnchor", "{0.3792,-0.4744}"},
    {"personal_transporter", nullptr, "frameAnchor", "{0.1656,-1.5496}"},
    {"personal_transporter", nullptr, "footAnchor", "{-0.0208,-1.5784}"},
    {"pogo_stick", "rodShape", "pos", "{0.1476,-1.1776}"},
    {"pogo_stick", "rodShape", "size", "{0.04,0.5}"},
    {"pogo_stick", "rodShape", "rot", "-0.257"},
    {"pogo_stick", "frameShape", "pos", "{0.2308,-0.8596}"},
    {"pogo_stick", "frameShape", "size", "{0.08,0.5}"},
    {"pogo_stick", "frameShape", "rot", "-0.257"},
    {"pogo_stick", nullptr, "handleAnchor", "{0.3452,-0.4284}"},
    {"pogo_stick", nullptr, "footAnchor", "{0.1216,-1.2744}"},
    {"road_bike", "frontWheelShape", "pos", "{0.6812,-1.1104}"},
    {"road_bike", "frontWheelShape", "radius", "0.4"},
    {"road_bike", "backWheelShape", "pos", "{-0.5188,-1.1104}"},
    {"road_bike", "backWheelShape", "radius", "0.4"},
    {"road_bike", "gearShape", "pos", "{-0.0384,-1.1104}"},
    {"road_bike", "gearShape", "radius", "0.124"},
    {"road_bike", "frame", "verts", "{-0.57,-1.1376}:{-0.014,-1.2256}:{0.57,-0.5376}:{-0.218,-0.5376}"},
    {"road_bike", "seat", "verts", "{-0.11,-0.8176}:{-0.07,-0.3776}:{-0.35,-0.3776}"},
    {"road_bike", "fork", "verts", "{0.698,-1.1536}:{0.542,-0.3776}:{0.386,-0.4896}"},
    {"road_bike", "seat1", "verts", "{-0.978,-0.0868}:{-0.762,-0.7128}:{-0.6096,-0.6508}:{-0.9572,0.0992}"},
    {"road_bike", "seat2", "verts", "{-0.808,-0.5888}:{-0.762,-0.7128}:{-0.394,-0.6328}:{-0.484,-0.3928}"},
    {"road_bike", "seat3", "verts", "{-0.524,-0.8488}:{-0.446,-0.8568}:{-0.394,-0.6328}:{-0.484,-0.3928}"},
    {"road_bike", nullptr, "handleAnchor", "{0.4992,-0.3824}"},
    {"road_bike", nullptr, "gear1Anchor", "{0.1416,-1.1104}"},
    {"road_bike", nullptr, "gear2Anchor", "{-0.2184,-1.1104}"},
    {"road_bike", nullptr, "frameSeatAnchor", "{-0.5184,-1.1104}"},
    {"wheelchair", "chair1Shape", "pos", "{0.072,-0.496}"},
    {"wheelchair", "chair1Shape", "size", "{0.35984,0.41984}"},
    {"wheelchair", "chair1Shape", "rot", "0"},
    {"wheelchair", "chair2Shape", "pos", "{-0.24,-0.368}"},
    {"wheelchair", "chair2Shape", "size", "{0.0478,0.54816}"},
    {"wheelchair", "chair2Shape", "rot", "0"},
    {"wheelchair", "chair3Shape", "pos", "{0.072,-0.724}"},
    {"wheelchair", "chair3Shape", "size", "{0.35984,0.19204}"},
    {"wheelchair", "chair3Shape", "rot", "0"},
    {"wheelchair", "bigWheelShape", "pos", "{-0.2032,-0.6808}"},
    {"wheelchair", "bigWheelShape", "radius", "0.4"},
    {"wheelchair", "smallWheelShape", "pos", "{0.4208,-0.9568}"},
    {"wheelchair", "smallWheelShape", "radius", "0.12"},
};

}  // namespace

void applyFlashVehicleShapes(const std::string& name, cocos2d::ValueMap& bodiesDict)
{
    using cocos2d::Value;
    using cocos2d::ValueMap;
    for (const VehicleValue& entry : kVehicleValues) {
        if (name != entry.vehicle) {
            continue;
        }
        if (entry.body == nullptr) {
            bodiesDict["joints"].asValueMap()[entry.field] = Value(entry.value);
            continue;
        }
        ValueMap& body = bodiesDict["bodies"].asValueMap()[entry.body].asValueMap();
        const std::string field = entry.field;
        if (field == "rot" || field == "radius") {
            body[field] = Value(static_cast<float>(std::atof(entry.value)));
        } else {
            body[field] = Value(entry.value);
        }
    }
}

}  // namespace online
