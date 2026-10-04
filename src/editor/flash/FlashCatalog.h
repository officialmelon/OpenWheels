#pragma once
// EDITOR (browser features, PC addition): the browser (Flash 1.87) level editor's item catalog,
// so the OpenWheels editor can place and edit everything browser levels can contain.
//
// Ground truth: the decompiled Flash editor (binary/flash/decomp/.../game/editor/):
//   * AttributeReference.buildInput / buildKeyedInput: every editable attribute's label, input
//     kind, range and slider segments, and the trigger-action parameters with their defaults;
//   * specials/*Ref.as: each browser special's attribute list (setAttributes, the UI order),
//     getFullProperties (the XML p0..pN order), defaults, shape/art cost, _triggerActionList /
//     _triggerActionListProperties;
//   * RefShape / RefGroup / PinJoint / PrisJoint / RefTrigger: the action lists of shapes, groups,
//     joints and triggers.
// Units are the browser editor's: pixels (62.5 px per metre), y down, rotations in degrees
// clockwise, inside a 20000 x 10000 px canvas. Nothing here is game text from the iOS bundle.

#include <string>
#include <vector>

namespace flashed {

const float kPxPerMetre = 62.5f;
const float kCanvasWidth = 20000.0f;
const float kCanvasHeight = 10000.0f;
const float kVersion = 1.87f;       // Settings.CURRENT_VERSION of the browser editor we mirror

// How an attribute is edited (Flash TextInput / SliderInput / CheckBox / ColorInput / ListInput).
enum class Input
{
    Field,    // numeric text field
    Slider,   // slider + text field, [min, max] in `segments` steps (0 = continuous)
    Switch,   // check box (0 / 1)
    Color,    // 0xRRGGBB (r/g/b sliders)
    Choice,   // list of names (value = index)
    Text,     // free text (text box caption)
    Sound,    // a sound of soundlist.tsv (value = sound id)
};

struct Attr
{
    const char* key;          // Flash attribute name ("numSpikes", "triggeredBy", ...)
    const char* label;        // Flash's caption
    Input input;
    float min;
    float max;
    int segments;
    // Flash child inputs (CheckBox.addChildInput): shown only while `parent` == parentValue.
    const char* parent;
    int parentValue;
    const char* help;         // Flash helpCaption (shortened); "" for none
    std::vector<const char*> choices;  // Input::Choice
};

// nullptr for unknown keys.
const Attr* attribute(const std::string& key);

// A trigger action (one entry of a target's _triggerActionList) and its parameters.
struct ActionInfo
{
    const char* name;
    std::vector<const char*> params;   // AttributeReference keyed keys ("impulseX", ...)
};

struct ActionParam
{
    const char* key;
    const char* label;
    Input input;     // Slider or Field
    float min;
    float max;
    int segments;
    float def;       // newX / newY default to the target's position (see actionParamDefault)
};
const ActionParam* actionParam(const std::string& key);

// What a trigger target is, for its action list.
enum class TargetKind
{
    Shape,
    Special,
    Group,
    PinJoint,
    PrisJoint,
    Trigger,
};
// nullptr: the item cannot be a trigger target (not _triggerable). Empty list: a target without
// actions (fans, mines, boosts, homing mines, wrecking balls are just activated).
const std::vector<ActionInfo>* targetActions(TargetKind kind, int specialType);

// A browser special handled by the generic editor ref (FlashSpecialRef).
struct SpecialInfo
{
    int type;                          // Settings.specialList index (XML <sp t>)
    const char* name;                  // display name (Flash RefSprite.name)
    std::vector<const char*> props;    // getFullProperties: XML p0..pN
    std::vector<const char*> ui;       // setAttributes order (x/y/angle shown separately)
    std::vector<std::pair<const char*, float>> defaults;
    int shapes;                        // shapes used when interactive
    int art;                           // art used when not interactive
    bool rotatable;
    const char* art0;                  // editor art name (generated/flash/<art>.png), "" = drawn
    float width;                       // footprint (px) used when there is no art / for hit tests
    float height;
};
// nullptr for ids not handled by FlashSpecialRef (shapes and the iOS editor's own refs).
const SpecialInfo* specialInfo(int type);
// Ids handled by FlashSpecialRef, in catalog order.
const std::vector<int>& flashSpecialTypes();

// Browser characters (info c): 1..11; true when this build can play it (mobile or restored).
int characterCount();
const char* characterName(int id);
bool characterPlayable(int id);

// soundlist.tsv names (id order), read once. Empty when the file is missing.
const std::vector<std::string>& soundNames();

}  // namespace flashed
