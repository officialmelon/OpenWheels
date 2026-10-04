#pragma once
// EDITOR (browser features, PC addition): entry points into the level editor from outside it
// (command line, online level browser). tools/parity/EditorlessStubs.cpp stubs them for builds
// without src/editor/.

#include <string>

namespace flashed {

// Replaces the running scene with the editor on `xml` (browser or iOS level XML); saving makes a
// new user level named `name`.
void openLevelInEditor(const std::string& xml, const std::string& name);

}  // namespace flashed
