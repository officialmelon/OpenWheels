#pragma once
// UI text lookup by the original's localization keys (e.g. "EDITOR INTRO MESSAGE", "EDITORBTN 3").
// The table is read at runtime from the player's own copy of the game (the iOS bundle's
// Localizable.strings, a binary plist), so no game text lives in this repository. Additional
// languages can be dropped in later as <lang>.lproj/Localizable.strings next to it.

#include <string>

namespace Localization {

// Text for `key`; falls back to the key itself when the table or entry is missing
// (same behaviour as NSLocalizedString).
const std::string& get(const std::string& key);

// Number of loaded entries (0 if no Localizable.strings was found).
size_t size();

}  // namespace Localization
