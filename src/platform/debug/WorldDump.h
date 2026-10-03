#pragma once
// Debug/verification only: serialises a b2World in the exact JSON schema produced by
// tools/re/oracle.py (which runs the ORIGINAL game in an arm64 emulator), so the two can be
// compared with tools/re/worlddiff.py. Not part of the original game.

#include <string>

class b2World;

namespace openwheels {
namespace debug {

// Writes bodies (world list order), fixtures (body list order) and joints (world list order).
// `frames` is recorded for bookkeeping. Returns false if the file could not be written.
bool dumpWorldJson(b2World* world, const std::string& path, int frames);

}  // namespace debug
}  // namespace openwheels
