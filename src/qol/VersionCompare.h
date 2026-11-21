#pragma once
// Engine-free version comparison for the update check (tests/unit covers it).
#include <string>
#include <vector>

namespace qol {

// "0.4.7" / "v0.4.10" / "0.4.7-beta.3": the numeric core as a list of integers.
std::vector<int> versionParts(const std::string& version);
// true when `latest` is a newer release than `current` (missing parts count as 0; unreadable = false).
bool isNewerVersion(const std::string& latest, const std::string& current);

}  // namespace qol
