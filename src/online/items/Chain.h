#pragma once
// ONLINE (PC addition): browser special 30, Chain (Flash userspecials/Chain + editor ChainRef).
// The Android Chain is a stub; this replaces it in converted browser levels (Override).
//
// XML (ChainRef._attributes): p0 x, p1 y, p2 angle, p3 sleeping, p4 interactive,
// p5 linkCount 2..40, p6 linkScale 1..10, p7 linkAngle -10..10.
//
// linkCount alternating link0MC / link1MC boxes (9x15 / 3x15 px times the link scale) along an
// arc, pinned together with revolute joints. Every 10 Flash frames each joint is checked: a
// reaction force above the break limit (scaled with the link mass) or anchors drifting apart
// break it (broken-link art, ChainBreak sound). Non-interactive chains are art only.
// Trigger actions: 0 wake from sleep (first link), 1 apply impulse (every link).

#include <vector>

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

namespace cocos2d {
class Sprite;
}

namespace online {

class Chain : public FlashItem
{
public:
    ~Chain() override;
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void actions() override;
    b2Body* getJointBody(b2Vec2 point) override;
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;
    std::vector<b2Body*> getBodyList() override { return _bodies; }
    void jointWillBeDestroyed(b2Joint* joint) override;

private:
    void breakJoint(size_t index);
    void showBrokenLink(const b2Vec2& worldAnchor, b2Body* body);

    std::vector<b2Body*> _bodies;
    std::vector<b2RevoluteJoint*> _joints;  // nullptr once broken
    std::vector<cocos2d::Node*> _sprites;   // per link (retained)
    std::vector<int> _linkKinds;            // 0 link0MC, 1 link1MC
    std::vector<float> _linkScales;         // sprite scale per link (for the broken flip)
    float _linkMass = 0.0f;                 // Flash overwrites every link's m_mass with link 0's
    float _breakLimit = 100000.0f;
    int _frameCounter = 0;
    misc::FlashClock _clock;
};

}  // namespace online
