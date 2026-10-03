#pragma once
// PC verification mode (not part of the original game): play a level through the original
// Gameplay scene in replay mode with scripted control bytes, at exactly 1/60 s per frame, then
// write the Box2D world in the oracle's JSON schema (compare with tools/re/worlddiff.py against
// `tools/re/oracle.py play`, which runs the ORIGINAL game the same way in an emulator).

#include <string>

namespace openwheels {
namespace pc {

// script: "frame:hexstate,frame:hexstate,..." (control byte changes). dumpAt: "a,b,c" extra
// frames to dump as <outPath minus .json>_f<N>.json (same naming as oracle.py play --dump-at).
// Returns a process exit code.
int runWorldDump(const std::string& outPath, const std::string& levelPath, int frames,
                 const std::string& script, const std::string& dumpAt = std::string());

}  // namespace pc
}  // namespace openwheels
