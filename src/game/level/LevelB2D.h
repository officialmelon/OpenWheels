#pragma once

// LevelB2D: builds a level's Box2D world and art from the level XML (via LevelXMLParser) and runs
// the per-frame action lists of the level items. Not a cocos2d::Ref; owned by the Session
// (`new LevelB2D(); init(xml)` in Session::setupLevel, sizeof 0x228).
//
// Bases: LevelXMLParserDelegateProtocol (+0x00, primary), ShapeItemDelegate (+0x08).
// Primary vtable: addInfo, addSpecial, addShape, addGroup, addJoint, addTrigger,
// addTriggersComplete, ~LevelB2D (D1, D0), removeShapeItem; secondary (ShapeItemDelegate):
// thunks to ~LevelB2D (D1, D0) and removeShapeItem.
//
// Units: XML positions/lengths are in "source" pixels (info "ptm", _sourcePtmRatio) with y down;
// they are converted to Box2D metres by dividing by _sourcePtmRatio and flipping y against the
// stage height (y' = _stageHeight - y / ptm). Art is placed in points: metres * _ptmRatio
// (the session's PTM ratio).

#include <map>
#include <string>
#include <vector>

#include "Box2D/Box2D.h"
#include "base/ccTypes.h"
#include "math/Vec2.h"

#include "LevelXMLParserDelegateProtocol.h"
#include "ShapeItemDelegate.h"

class CharacterB2D;
class GroupItem;
class LevelDataElement;
class LevelItem;
class ShapeItem;
class TargetAction;
class TargetActionGroup;
class TargetActionPrisJoint;
class TargetActionRevJoint;
class Trigger;

// Level <info c="..."> / Settings selected character. Values follow the Flash game's character
// ids; enumerator names are descriptive. 6, 7 and 8 have no character on Android (createCharacter
// returns nullptr for them).
enum CharacterId
{
    CharacterIdWheelchairGuy = 1,      // "wheelchair_guy" + "wheelchair"
    CharacterIdBusinessGuy = 2,        // "business_guy" + "personal_transporter"
    CharacterIdIrresponsibleDad = 3,   // "irresponsible_dad" + "road_bike"
    CharacterIdEffectiveShopper = 4,   // "effective_shopper" + "motor_cart"
    CharacterIdMopedCouple = 5,        // "moped_guy" + "moped"
    CharacterIdPogostickGuy = 9,       // "pogo_stick_guy" + "pogo_stick"
};

// Only VehicleIdDefault (0) is ever passed (LevelB2D::addInfo). In createCharacter any other value
// creates a bare CharacterB2D (no vehicle class) instead of the character's own class.
enum VehicleId
{
    VehicleIdDefault = 0,
};

// One terrain vertex (element of a terrain shape's vertex list), sizeof 0x20.
// Parsed by LevelB2D::stringToTerrainVert from XML "x_y_<t|f>_segments[_inX_inY_outX_outY]"
// (e.g. "170_-1.5_t_10_0_0_70_0"); the <t|f> field is ignored on Android (active = true).
// The cubic segment from vertex A to the next vertex B uses the control points
// A.point + A.controlPointOut and B.point + B.controlPointIn (iOS encoding {TerrainVert=ffffffIB}).
// Members are b2Vec2 (no-op default constructor): addShape's local TerrainVert[2000] buffer is not
// zero-filled in the original, unlike its cocos2d::Vec2 buffers.
struct TerrainVert
{
    b2Vec2 point;                   // +0x00
    b2Vec2 controlPointIn;          // +0x08  relative to point
    b2Vec2 controlPointOut;         // +0x10  relative to point
    unsigned int segments;          // +0x18  bezier subdivisions (clamped to 20), <= 1 => straight
    bool active;                    // +0x1c  vertex is used (always true on Android)
};

class LevelB2D : public LevelXMLParserDelegateProtocol, public ShapeItemDelegate
{
public:
    LevelB2D();
    virtual ~LevelB2D();

    bool init(std::string xml);  // empty xml => Settings::getLevelXMLData()
    void die();
    void update(float dt);
    void paint();

    bool addToActions(LevelItem* item);  // false when already queued
    void addToSingleActions(LevelItem* item);
    bool singleActionsContainsLevelItem(LevelItem* item);
    bool actionsContainsLevelItem(LevelItem* item);
    void removeFromSingleActions(LevelItem* item);
    void removeFromActions(LevelItem* item);
    void addToPaintItem(LevelItem* item);
    void removeFromPaintItem(LevelItem* item);
    void addToPaintBody(b2Body* body);
    void removeFromPaintBody(b2Body* body);
    void addToFrameActions(LevelItem* item);
    void removeFromFrameActions(LevelItem* item);

    float getVersion();
    float getSourcePtmRatio();
    void levelCompleted();
    void setTimeStep(float timeStep);
    bool getLevelComplete();
    bool getForcedChar();

    // addCharacter always passes hideVehicle = false on to createCharacter.
    CharacterB2D* addCharacter(float x, float y, CharacterId characterId, VehicleId vehicleId,
                               bool hideVehicle, int groupIndex);
    CharacterB2D* createCharacter(float x, float y, CharacterId characterId, VehicleId vehicleId,
                                  bool hideVehicle, int groupIndex);
    CharacterB2D* getCharacter();  // first character or nullptr
    float convertYMetersToLevelMeters(float y);  // identity

    // ---- LevelXMLParserDelegateProtocol ----
    void addInfo(LevelDataElement* info) override;
    void convertLengthData(float* length);
    void convertYMeterPositionData(float* y);
    void addSpecial(LevelDataElement* special, int index) override;
    LevelItem* addSpecial(LevelDataElement* special, int index, b2Body* groupBody,
                          b2Vec2 groupOffset);
    void setShapeFilter(int collision, bool immovable, b2FixtureDef* fixtureDef);
    void addShape(LevelDataElement* shape, int index) override;
    ShapeItem* addShape(LevelDataElement* shape, GroupItem* groupItem, cocos2d::Vec2 groupOffset,
                        unsigned int index, bool foreground);
    void addShapeItem(ShapeItem* shapeItem);
    void convertPositionAndRotationData(float* x, float* y, float* rotation);  // rotation unused
    cocos2d::Color4F ccColorFromRGB(long rgb);
    cocos2d::Vec2 stringToVec(const char* str);  // "x_y" (or "x.y" for old versions), atoi
    void convertVerts(cocos2d::Vec2* verts, int count);
    TerrainVert stringToTerrainVert(const char* str);
    void convertTerrainVerts(TerrainVert* verts, int count);
    float calculateBezier(float t, float value0, float value1, float value2, float value3);
    void addJoint(LevelDataElement* joint, int index) override;
    GroupItem* groupItemWithId(int index);
    void convertRevJointData(float* motorSpeed, float* lowerAngle, float* upperAngle);
    void convertRotationData(float* rotation);
    void addTrigger(LevelDataElement* trigger, int index) override;
    void addTriggersComplete() override;
    ShapeItem* getShapeItem(int index);
    LevelItem* getSpecial(unsigned int index);
    void addGroup(LevelDataElement* group, int index) override;
    void removeGroupItem(GroupItem* groupItem);  // erase + delete
    GroupItem* getGroupItem(unsigned int index);

    // ---- ShapeItemDelegate ----
    void removeShapeItem(ShapeItem* shapeItem, bool deleteItem) override;

    void removeSpecial(LevelItem* special);  // erase + release
    void convertPositionData(float* x, float* y);
    void convertDirectionIfNecessaryBasedOnRegistration(float* value);
    std::vector<LevelItem*> getActionsVector();
    std::vector<CharacterB2D*> getCharacters();

    // Notify every target action registered under `index` (except `caller`).
    void updateTargetActionRevJoint(unsigned int index, b2Joint* joint,
                                    TargetActionRevJoint* caller);
    void updateTargetActionPrisJoint(unsigned int index, b2Joint* joint,
                                     TargetActionPrisJoint* caller);
    void updateTargetActionsFor(unsigned int index, ShapeItem* shapeItem, b2Fixture* currentShape,
                                b2Fixture* newShape, TargetAction* caller);
    void updateTargetActionGroupsFor(unsigned int index, GroupItem* groupItem,
                                     TargetActionGroup* caller);

    void addFixtureMaterial(b2Fixture* fixture, int material);
    void removeFixtureMaterial(b2Fixture* fixture);
    int getFixtureMaterial(b2Fixture* fixture);  // 0 when unknown

private:
    // +0x00 vptr LevelXMLParserDelegateProtocol, +0x08 vptr ShapeItemDelegate
    std::vector<CharacterB2D*> _characters;                         // +0x010  deleted in dtor
    std::map<b2Fixture*, int> _fixtureMaterials;                    // +0x028
    std::map<int, std::vector<LevelItem*>> _targetActions;          // +0x040  by target index
    std::vector<LevelItem*> _singleActionVector;                    // +0x058  singleAction(), once
    std::vector<LevelItem*> _actionsVector;                         // +0x070  actions(), each frame
    std::vector<LevelItem*> _actionsToAdd;                          // +0x088
    std::vector<LevelItem*> _actionsToRemove;                       // +0x0a0
    std::vector<LevelItem*> _frameActions;                          // +0x0b8  frameAction(), 60 Hz
    std::vector<LevelItem*> _frameActionsToRemove;                  // +0x0d0
    std::vector<LevelItem*> _specials;                              // +0x0e8  retained
    std::vector<ShapeItem*> _shapeItems;                            // +0x100  deleted in dtor
    std::vector<GroupItem*> _groupItems;                            // +0x118  deleted in dtor
    std::vector<GroupItem*> _foregroundGroupItems;                  // +0x130  deleted in dtor
    std::vector<b2Joint*> _joints;                                  // +0x148
    std::vector<Trigger*> _triggers;                                // +0x160  retained
    std::vector<LevelDataElement*> _triggerElements;                // +0x178  retained, pending
    std::vector<LevelItem*> _paintItems;                            // +0x190  paint(), each frame
    std::vector<b2Body*> _paintBodies;                              // +0x1a8  user data Node synced
    std::map<int, std::vector<cocos2d::Vec2>> _polygonVerts;        // +0x1c0  shape t=3, by "id"
    std::map<int, std::vector<cocos2d::Vec2>> _artVerts;            // +0x1d8  shape t=4, by "id"
    std::map<int, std::vector<TerrainVert>> _terrainVerts;          // +0x1f0  shape t=5, by "id"
    float _ptmRatio;                                                // +0x208  session PTM ratio
    float _version;                                                 // +0x20c  info "v", ctor -1
    float _stageWidth;                                              // +0x210  info "sw" / ptm
    float _stageHeight;                                             // +0x214  info "sh" / ptm
    float _sourcePtmRatio;                                          // +0x218  info "ptm", ctor 1
    bool _registration;                                             // +0x21c  info "r"
    bool _clockwise;                                                // +0x21d  info "cw": negate y
    bool _levelComplete;                                            // +0x21e
    float _frameActionTimer;                                        // +0x220
    bool _forcedChar;                                               // +0x224  info "f"
};
