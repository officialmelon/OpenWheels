#pragma once
// QOL (PC addition): update check. On start-up the game asks GitHub for the latest OpenWheels
// release (one request to api.github.com, nothing about the player is sent) and, when it is newer
// than the running version, offers to open its download page on the main menu. "Later" remembers
// that version so it is not offered again. Can be turned off in the QoL menu.

#include <string>

namespace qol {

const char* appVersion();
bool updateCheckEnabled();
void setUpdateCheckEnabled(bool on);

// Starts the request (once per launch; no-op when disabled).
void checkForUpdates();
// Shows the "update available" popup if a newer release was found and not dismissed yet. Call when
// the main menu is up; cheap otherwise.
void showPendingUpdatePrompt();

// "0.4.7" / "v0.4.10" / "0.4.7-beta.3" -> true when `latest` is newer than `current` (testable).
bool isNewerVersion(const std::string& latest, const std::string& current);

}  // namespace qol
