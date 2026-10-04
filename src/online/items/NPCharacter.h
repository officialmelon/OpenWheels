#pragma once
// ONLINE (PC addition): browser special 17, NPCharacter (Flash level/userspecials/NPCharacter +
// editor/specials/NPCharacterRef): a posable ragdoll "non-player character" in one of 16 skins
// (npcsprites/NPCSprite1..16). Interactive NPCs are 11 Box2D bodies with the Flash shapes,
// joint limits, pose motors / locks, injuries (joint breaks, head / chest / pelvis smashes with
// chunks), blood and voices; non-interactive ones are static art (groupable).
//
// XML (NPCharacterRef._attributes): p0 x, p1 y, p2 angle, p3 charIndex 1..16, p4 sleeping,
// p5 reverse, p6 holdPose, p7 interactive, p8 neck, p9 shoulder1, p10 shoulder2, p11 elbow1,
// p12 elbow2, p13 hip1, p14 hip2, p15 knee1, p16 knee2 (degrees), p17 destroyJointsUponDeath.
// Trigger actions: 0 wake from sleep, 1 apply impulse (impulseX, impulseY, spin), 2 hold pose,
// 3 release pose.

#include <string>
#include <vector>

#include "online/items/Grindable.h"

#include "EmitterDelegate.h"
#include "online/items/FlashSpecials.h"

namespace cocos2d {
class Node;
}
class FlowEmitter;
class Sound;

namespace online {

namespace npc {
struct SpriteData;
}

class NPCharacter : public FlashItem, public EmitterDelegate, public Grindable
{
public:
    // Lawnmower Man blade (Flash grindShape / removeBody), see Grindable.h.
    void grindFixture(b2Fixture* fixture) override;
    void grindBody(b2Body* body) override;
    ~NPCharacter() override;
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;

    void actions() override;
    b2Body* getJointBody(b2Vec2 point) override;
    int getFluidType() override;
    int shapeImpale(b2Fixture* fixture, bool impale, b2Vec2 point, float radius) override;
    void explodeShape(b2Fixture* fixture, float force) override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;
    void jointWillBeDestroyed(b2Joint* joint) override;
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;
    std::vector<b2Body*> getBodyList() override;
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float rotation) override;
    void setOpacity(float opacity) override;

    void emitterComplete(Emitter* emitter) override;

    // Flash NPCharacter.centralBody: the heart once the chest burst, else the chest.
    b2Body* centralBody() const { return _heartBody ? _heartBody : _body[kChest]; }

private:
    enum Part {
        kChest, kHead, kPelvis, kUpperArm1, kUpperArm2, kLowerArm1, kLowerArm2,
        kUpperLeg1, kUpperLeg2, kLowerLeg1, kLowerLeg2, kPartCount
    };
    enum JointId {
        kNeck, kWaist, kShoulder1, kShoulder2, kElbow1, kElbow2, kHip1, kHip2, kKnee1, kKnee2, kJointCount
    };
    enum Flow {
        kHeadFlow, kNeckFlow, kStomachFlow, kShoulder1Flow, kShoulder2Flow, kArm1Flow, kArm2Flow,
        kHip1Flow, kHip2Flow, kThigh1Flow, kThigh2Flow, kFlowCount
    };

    void readPose(LevelDataElement* element);
    void createStaticArt(bool inGroup, bool foreground);
    void createBodies();
    void createJoints();
    void createArt();
    void setBreakLimits();

    // art
    cocos2d::Node* makePartNode(Part part, int frame);
    void setPartFrame(Part part, int frame);
    std::string partName(Part part) const;
    cocos2d::Node* chunkNode(const std::string& name, float fallbackRadiusPx);

    // injuries (Flash names)
    void handleContactResults();
    void checkJoints();
    void checkRevJoint(JointId id, float limit);
    void breakJoint(JointId id);
    void setDead();
    void destroyUserJoints();
    void cancelPose();
    void lockPose();
    void releasePose();
    std::vector<bool> awakeStates() const;
    void restoreAwake(const std::vector<bool>& awake);
    void headSmash();
    void chestSmash();
    void pelvisSmash();
    void torsoBreak(bool blood = true, bool sound = true);
    void neckBreak(bool blood = true, bool sound = true);
    void shoulderBreak(int k, bool blood = true);
    void elbowBreak(int k);
    void hipBreak(int k, bool blood = true);
    void kneeBreak(int k);
    void checkVocals();
    void addVocals(const char* name, int priority);
    void randomVocals(b2Fixture* fixture);

    // helpers
    void scanUserJoints();
    b2Body* createChunk(const std::string& art, float radiusM, b2Vec2 position, float angle,
                        b2Body* source, cocos2d::Node* nearNode, float fallbackRadiusPx);
    void destroyPartBody(Part part);
    void destroyJoint(JointId id);
    void destroyJointExternal(b2Joint* joint);
    void setAbsLimits(JointId id, float lowerDeg, float upperDeg);
    void setFilter(b2Body* body, const b2Filter& filter);
    void startFlow(Flow flow, float minSpeed, float maxSpeed, int count, b2Body* body, b2Vec2 local,
                   float flashAngle);
    void stopFlow(Flow flow);
    void burst(b2Body* body, b2Vec2 local, int count);
    void playSound(const std::string& name, b2Body* body);
    bool isLimb(b2Fixture* fixture, Part part) const;

    const npc::SpriteData* _data = nullptr;
    int _charIndex = 1;
    bool _interactive = true;
    bool _reversed = false;
    bool _sleeping = false;
    bool _holdPose = false;
    bool _destroyJointsUponDeath = false;
    float _x = 0, _y = 0, _angle = 0;   // Flash px / degrees
    int _pose[9] = {0};                 // neck, shoulder1/2, elbow1/2, hip1/2, knee1/2
    std::string _tag;                   // voice prefix
    bool _showGore = true;

    b2Body* _body[kPartCount] = {nullptr};
    b2Fixture* _fixture[kPartCount] = {nullptr};
    b2RevoluteJoint* _joint[kJointCount] = {nullptr};
    bool _broken[kJointCount] = {false};
    bool _ground[kPartCount] = {false};  // Flash shape userData GRIND_STATE
    b2Body* _heartBody = nullptr;
    b2Vec2 _elbow1LocalAnchorB;         // Flash elbowBreak2 bleeds at elbow 1's anchor (sic)
    b2Filter _defaultFilter, _zeroFilter, _lowerBodyFilter;

    float _headSmashLimit = 0, _chestSmashLimit = 0, _pelvisSmashLimit = 0;
    float _jointLimit[kJointCount] = {0};
    float _headChunkRadius = 0, _chestChunkRadius = 0, _pelvisChunkRadius = 0;  // Flash px

    bool _dead = false;
    bool _userJointsScanned = false;
    std::vector<b2Joint*> _userJoints;

    FlowEmitter* _flow[kFlowCount] = {nullptr};
    b2Body* _flowBody[kFlowCount] = {nullptr};
    Sound* _voiceSound = nullptr;
    int _voicePriority = -1;
    std::string _voiceArray[6];
    int _voiceTop = -1;

    // art: one node per part (positioned by LevelB2D::paint through the body's user data)
    cocos2d::Node* _layer = nullptr;
    cocos2d::Node* _partNode[kPartCount] = {nullptr};
    int _partFrame[kPartCount] = {0};
    int _partZ[kPartCount] = {0};
    std::vector<cocos2d::Node*> _extraNodes;  // chunks
    cocos2d::Node* _staticRoot = nullptr;     // non-interactive art
    bool _inGroup = false;
};

// Trigger::checkAdd2 hook ("triggered by any character"): Flash also accepts NPCs, by the
// centralBody of the NPCharacter in the fixture's user data.
bool isNPCharacterCentralBody(LevelItem* item, b2Body* body);

}  // namespace online
