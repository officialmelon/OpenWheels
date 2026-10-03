#pragma once

// Pure interface (vptr only). Implemented by CharacterB2D (sub-object at +0x98).
// The implementer's secondary vtable holds a single slot (no virtual destructor here).

class Emitter;

class EmitterDelegate
{
public:
    virtual void emitterComplete(Emitter* emitter) = 0;  // +0x00
};
