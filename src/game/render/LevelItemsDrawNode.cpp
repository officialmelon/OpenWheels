#include "LevelItemsDrawNode.h"

#include "Box2D/Box2D.h"
#include "HarpoonGun.h"
#include "LevelItemsDrawNodeWreckingBallDelegate.h"
#include "Ligament.h"
#include "SpinalCord.h"
#include "WreckingBall.h"

USING_NS_CC;

// @005de8c4
LevelItemsDrawNode* LevelItemsDrawNode::create(float ptmRatio)
{
    // Plain (throwing) new + value-initialisation (memset 0x500), DrawNode(2.0) via the default
    // line width; init(float) inlined.
    LevelItemsDrawNode* node = new LevelItemsDrawNode();
    if (node->init(ptmRatio))
    {
        node->autorelease();
        return node;
    }
    delete node;
    return nullptr;
}

// @005de9c4
bool LevelItemsDrawNode::init(float ptmRatio)
{
    bool ok = DrawNode::init();
    if (ok)
    {
        _ptmRatio = ptmRatio;
        _thickness = ptmRatio * 0.02f;
        setLineWidth(_thickness);
    }
    return ok;
}

// @005dea1c
void LevelItemsDrawNode::update()
{
    clear();

    // Ligaments and spinal cords: one red segment per distance joint, anchor to anchor.
    for (size_t i = 0; i < _ligaments.size(); i++)
    {
        std::vector<b2DistanceJoint*> joints = _ligaments[i]->getJoints();
        for (unsigned int j = 0; j < joints.size(); j++)
        {
            b2DistanceJoint* joint = joints[j];
            const b2Vec2 anchorA = joint->GetBodyA()->GetWorldPoint(joint->GetLocalAnchorA());
            const b2Vec2 anchorB = joint->GetBodyB()->GetWorldPoint(joint->GetLocalAnchorB());
            drawSegment(Vec2(anchorA.x * _ptmRatio, anchorA.y * _ptmRatio),
                        Vec2(anchorB.x * _ptmRatio, anchorB.y * _ptmRatio), _thickness, _redColor);
        }
    }
    for (size_t i = 0; i < _spinalCords.size(); i++)
    {
        std::vector<b2DistanceJoint*> joints = _spinalCords[i]->getJoints();
        for (unsigned int j = 0; j < joints.size(); j++)
        {
            b2DistanceJoint* joint = joints[j];
            const b2Vec2 anchorA = joint->GetBodyA()->GetWorldPoint(joint->GetLocalAnchorA());
            const b2Vec2 anchorB = joint->GetBodyB()->GetWorldPoint(joint->GetLocalAnchorB());
            drawSegment(Vec2(anchorA.x * _ptmRatio, anchorA.y * _ptmRatio),
                        Vec2(anchorB.x * _ptmRatio, anchorB.y * _ptmRatio), _thickness, _redColor);
        }
    }

    // Harpoon ropes: black segments through the A anchors of consecutive joints, then from the
    // last joint's A anchor to its B anchor. Each anchor is fetched twice (x from one virtual
    // call, y from another), and the first element is read even if the vector is empty (sic).
    for (size_t i = 0; i < _harpoons.size(); i++)
    {
        std::vector<b2DistanceJoint*> joints = _harpoons[i]->getJoints();
        b2DistanceJoint* joint = joints[0];
        for (unsigned int j = 1; j < joints.size(); j++)
        {
            b2DistanceJoint* previous = joint;
            joint = joints[j];
            drawSegment(Vec2(previous->GetAnchorA().x * _ptmRatio, previous->GetAnchorA().y * _ptmRatio),
                        Vec2(joint->GetAnchorA().x * _ptmRatio, joint->GetAnchorA().y * _ptmRatio),
                        _thickness, _blackColor);
        }
        drawSegment(Vec2(joint->GetAnchorA().x * _ptmRatio, joint->GetAnchorA().y * _ptmRatio),
                    Vec2(joint->GetAnchorB().x * _ptmRatio, joint->GetAnchorB().y * _ptmRatio),
                    _thickness, _blackColor);
    }

    // Wrecking-ball chains (points already in pixels).
    for (size_t i = 0; i < _wreckingBalls.size(); i++)
    {
        WreckingBall* wreckingBall = _wreckingBalls[i];
        drawSegment(wreckingBall->pointA(), wreckingBall->pointB(), _thickness, _blackColor);
    }
    for (size_t i = 0; i < _wreckingBallDelegates.size(); i++)
    {
        LevelItemsDrawNodeWreckingBallDelegate* wreckingBallDelegate = _wreckingBallDelegates[i];
        drawSegment(wreckingBallDelegate->getPointA(), wreckingBallDelegate->getPointB(), _thickness,
                    _blackColor);
    }
}

// @005def60
void LevelItemsDrawNode::addLigament(Ligament* ligament)
{
    _ligaments.push_back(ligament);
}

// @005df0dc
void LevelItemsDrawNode::addSpinalCord(SpinalCord* spinalCord)
{
    _spinalCords.push_back(spinalCord);
}

// @005df258
void LevelItemsDrawNode::addWreckingBall(WreckingBall* wreckingBall)
{
    _wreckingBalls.push_back(wreckingBall);
}

// @005df3d4
void LevelItemsDrawNode::addWreckingBallDelegate(LevelItemsDrawNodeWreckingBallDelegate* wreckingBallDelegate)
{
    _wreckingBallDelegates.push_back(wreckingBallDelegate);
}

// @005df550
void LevelItemsDrawNode::addHarpoonGun(HarpoonGun* harpoonGun)
{
    _harpoons.push_back(harpoonGun);
}

// @005df6cc
void LevelItemsDrawNode::removeAll()
{
    // Spinal cords and wrecking-ball delegates are NOT cleared (as in the original).
    _ligaments.clear();
    _harpoons.clear();
    _wreckingBalls.clear();
}

// @005df6e8
LevelItemsDrawNode::~LevelItemsDrawNode()
{
}
