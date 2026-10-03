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

protected:
    std::vector<Emitter*> _emitters;  // +0x2f8
};
