#pragma once
// Location of the player's own iOS app bundle (happywheels.app), which carries content the
// Android build lacks: the level editor's art atlases and its Localizable.strings. Set by the
// platform entry point (--ios-app or auto-detected); empty if not available.

#include <string>

namespace openwheels {

void setIOSBundlePath(const std::string& dirWithSlash);
const std::string& iosBundlePath();          // "" when no iOS bundle was found
inline bool hasIOSBundle() { return !iosBundlePath().empty(); }

}  // namespace openwheels
