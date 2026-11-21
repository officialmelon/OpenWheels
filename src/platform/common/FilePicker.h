#pragma once
// EDITOR: "import levels" on phones. The importer reads the app's own import folder, which a
// phone's owner cannot reach, so on Android the system file picker copies the chosen .xml / .txt
// files into it (src/platform/android/AndroidFilePicker.cpp + AppActivity.pickLevelFiles).
// Elsewhere there is no picker: desktops read the import folder and the Downloads folder, iOS
// the app's folder in the Files app (file sharing).

#include <functional>
#include <string>

namespace openwheels {
namespace filepicker {

bool available();
// Opens the picker; `done(copied, skipped)` runs later on the GL thread: copied = files copied
// into destDir, skipped = chosen files that were not .xml / .txt or could not be read,
// copied == -1 when the picker could not be opened. A cancelled picker reports (0, 0).
void pickLevelFiles(const std::string& destDir, std::function<void(int copied, int skipped)> done);

}  // namespace filepicker
}  // namespace openwheels
