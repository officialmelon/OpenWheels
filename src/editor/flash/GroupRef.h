#pragma once
// EDITOR (browser features, PC addition): a browser group (Flash editor RefGroup, XML
// <groups><g x y r ox oy s f o im fr>{<sh>|<sp>}</g>) or user-built vehicle (RefVehicle,
// <g v="t" sb sh ct a l cp lo>).
//
// Editor model: members stay ordinary refs on the stage, flagged with Special::group(); tapping
// any member selects the whole group (the GroupRef and its members), dragging / rotating moves
// them together. The GroupRef itself is invisible: it sits at the centre of its members, draws a
// dashed outline and holds the group's properties. It is written with r = 0 and members at their
// world positions (ox/oy = -x/-y), which places them exactly where they are in the editor.

#include <set>

#include "Special.h"

class GroupRef : public Special
{
public:
    static const int kLevelItemID = 7003;
    CREATE_FUNC(GroupRef);
    bool init() override;

    const cocos2d::Vector<Special*>& members() const { return _members; }
    void setMembers(const cocos2d::Vector<Special*>& members);  // sets each member's group()
    // Members that are on the stage.
    cocos2d::Vector<Special*> liveMembers() const;
    // Moves the group's position to the centre of its live members.
    void recenter();
    // Whether `ref` may join a group (Flash RefSprite.groupable).
    static bool groupable(Special* ref);
    // Has a member that makes the group a physical body (Flash: _shapesUsed > 0).
    bool hasShapes() const;

    // Special
    cg::Rect refBoundingBox() override;
    void setRotation(float) override {}
    std::vector<std::string> propertyKeysForUI() override;
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;
    cocos2d::ValueMap properties() override;
    void setProperties(const cocos2d::ValueMap& properties) override;
    void updateOverlayWithNode(cocos2d::DrawNode* node) override;

    bool sleeping = false;
    bool foreground = false;
    bool immovable = false;
    bool fixedRotation = false;
    float opacity = 100.0f;
    bool vehicle = false;
    int spaceAction = 0;
    int shiftAction = 0;
    int ctrlAction = 0;
    float acceleration = 1.0f;
    int leaningStrength = 0;
    int characterPose = 0;
    bool lockJoints = false;
    // Vehicle shapes the character can't grab (Flash RefShape.vehicleHandle = false).
    std::set<Special*> nonHandles;

protected:
    GroupRef() = default;
    cocos2d::Vector<Special*> _members;
};
