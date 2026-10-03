#pragma once

// QueryCallback: b2QueryCallback collecting every reported fixture. Its code lives in
// HomingMine's translation unit in the binary (inline class), sizeof 0x20 (arm64).

#include <vector>

#include "Box2D/Box2D.h"

class QueryCallback : public b2QueryCallback
{
public:
    // destructor implicit (emitted with the class in HomingMine's TU)
    virtual bool ReportFixture(b2Fixture* fixture) override;  // push_back, return true

    std::vector<b2Fixture*> _fixtures;  // +0x08
};
