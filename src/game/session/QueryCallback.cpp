#include "QueryCallback.h"

// In the binary the class is emitted in HomingMine's translation unit (inline class): implicit
// destructors @005c4578 (D2) and @005c5578 (D0) free _fixtures. ReportFixture is the class's
// only out-of-line function here, so the vtable and destructors are emitted with it.

// @005c55b8
bool QueryCallback::ReportFixture(b2Fixture* fixture)
{
    _fixtures.push_back(fixture);
    return true;
}
