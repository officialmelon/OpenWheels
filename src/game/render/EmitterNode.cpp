#include "EmitterNode.h"

#include "qol/BloodCompositor.h"  // QOL (PC addition)
#include "qol/QoL.h"

#include <algorithm>

#include "Emitter.h"

USING_NS_CC;

// @00578b08
EmitterNode::EmitterNode()
{
    cocos2d::log("EmitterNode: constructor");
}

// @00578b7c
EmitterNode::~EmitterNode()
{
    cocos2d::log("EmitterNode: destructor");
}

// @00578bf0
bool EmitterNode::init()
{
    // Node::init is not called.
    cocos2d::log("EmitterNode: init");
    return true;
}

// @00578c10
void EmitterNode::addChild(Emitter* emitter)
{
    Node::addChild(emitter);
    _emitters.push_back(emitter);
}

// @00578d90
void EmitterNode::addChildEmitter(Emitter* emitter)
{
    // Identical to addChild(Emitter*).
    Node::addChild(emitter);
    _emitters.push_back(emitter);
}

// @00578f10
void EmitterNode::removeChild(Node* child, bool cleanup)
{
    auto it = std::find(_emitters.begin(), _emitters.end(), child);
    if (it != _emitters.end())
    {
        _emitters.erase(it);
    }
    Node::removeChild(child, cleanup);
}

// @00578f90
void EmitterNode::pauseEmitters()
{
    for (size_t i = 0; i < _emitters.size(); ++i)
    {
        _emitters[i]->setGameplayPause(true);
    }
}

// @00578fe4
void EmitterNode::resumeEmitters()
{
    for (size_t i = 0; i < _emitters.size(); ++i)
    {
        _emitters[i]->setGameplayPause(false);
    }
}

// @00579038
int EmitterNode::getMaxParticles()
{
    int total = 0;
    for (size_t i = 0; i < _emitters.size(); ++i)
    {
        total += _emitters[i]->getMaxParticles();
    }
    return total;
}

// QOL (PC addition): not in the original (EmitterNode uses Node::visit there).
void EmitterNode::visit(Renderer* renderer, const Mat4& parentTransform, uint32_t parentFlags)
{
    if (!qol::BloodCompositor::active() || !isVisible())
    {
        Node::visit(renderer, parentTransform, parentFlags);
        return;
    }
    std::vector<Node*> blood;
    for (Node* child : getChildren())
    {
        if (child->isVisible() && qol::isBlood(child))
        {
            blood.push_back(child);
            child->setVisible(false);
        }
    }
    Node::visit(renderer, parentTransform, parentFlags);
    for (Node* child : blood)
    {
        child->setVisible(true);
    }
    if (!blood.empty())
    {
        qol::BloodCompositor::draw(this, blood, renderer, parentTransform * getNodeToParentTransform(), parentFlags);
    }
}
