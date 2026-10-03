#pragma once

// EmitterNode: a session particle layer (foreground/midground/background) that tracks its
// Emitter children for pausing and particle budgeting. sizeof 0x310 (arm64).

#include <vector>

#include "2d/CCNode.h"

class Emitter;

class EmitterNode : public cocos2d::Node
{
public:
    // Inlined into Session::init (no symbol): new (std::nothrow), init(), autorelease / delete.
    CREATE_FUNC(EmitterNode);

    EmitterNode();
    virtual ~EmitterNode();

    virtual bool init() override;
    void addChild(Emitter* emitter);  // Node::addChild + remember (not an override)
    void addChildEmitter(Emitter* emitter);
    virtual void removeChild(cocos2d::Node* child, bool cleanup = true) override;
    void pauseEmitters();
    void resumeEmitters();
    int getMaxParticles();
    // QOL (PC addition): liquid/realistic blood draws the blood emitters through
    // qol::BloodCompositor; otherwise Node::visit unchanged.
    void visit(cocos2d::Renderer* renderer, const cocos2d::Mat4& parentTransform, uint32_t parentFlags) override;

protected:
    std::vector<Emitter*> _emitters;  // +0x2f8
};
