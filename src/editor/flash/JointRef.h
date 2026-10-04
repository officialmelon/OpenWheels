#pragma once
// EDITOR (browser features, PC addition): a browser joint (Flash editor PinJoint / PrisJoint,
// XML <joints><j t="0|1" ...>). A point that pins the two topmost joinable items under it
// (Flash RefJoint.identifyBodies); with one item, that item is pinned to the world.
//   pin:   l limit, ua/la upper/lower angle, m motor, tq torque, sp speed
//   slide: a axis angle, l limit, ul/ll upper/lower limit (px), m motor, fo force, sp speed
//   both:  c collide connected, v vehicle controlled (when a body is a vehicle)
// The bodies are re-identified whenever the joint is placed or dragged.

#include "Special.h"

class JointRef : public Special
{
public:
    static const int kPinLevelItemID = 7001;
    static const int kSlideLevelItemID = 7002;
    static JointRef* create(bool prismatic);
    bool initWithType(bool prismatic);

    bool prismatic() const { return _prismatic; }
    Special* body1() const { return _body1.get(); }
    Special* body2() const { return _body2.get(); }
    void setBodies(Special* body1, Special* body2);
    // Flash identifyBodies: the two topmost joinable units under the joint (registers undo).
    void identifyBodies(bool registerUndo);
    bool vehicleAttached() const;

    // Special
    void didMove() override { identifyBodies(true); }
    void onEnter() override;
    std::vector<std::string> propertyKeysForUI() override;
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;
    cocos2d::ValueMap properties() override;
    void setProperties(const cocos2d::ValueMap& properties) override;
    void updateOverlayWithNode(cocos2d::DrawNode* node) override;
    void setRotation(float) override {}

    bool limit = false;
    bool motor = false;
    bool collideSelf = false;
    bool vehicleControlled = true;
    float upper = 90.0f;     // pin: degrees 0..180;  slide: px 0..3000
    float lower = -90.0f;
    float speed = 3.0f;
    float torque = 50.0f;    // pin torque / slide force
    float axisAngle = 0.0f;

protected:
    JointRef() = default;
    bool _prismatic = false;
    cocos2d::RefPtr<Special> _body1;
    cocos2d::RefPtr<Special> _body2;
    bool _identifyOnEnter = true;
};
