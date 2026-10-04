#pragma once
// NET (PC addition): the "Send to Nearby" and "Receive Levels" panels (net::ui::Modal, styled
// like the online level browser and the game's HWWindow alerts).
//
// Send to Nearby (opened from the user-level screen, the editor menu and the online browser):
//   title "Send to Nearby" and the level's name; the players LanDiscovery finds as big rows (a
//   phone / PC drawing, the player name in Clarendon, the device name small and muted; the
//   selected row in the main menu's blue button art); while the list is empty a spinner and
//   "Looking for players on your Wi-Fi..."; "You appear as [name]" as a small editable line;
//   one big blue Send button (disabled until a player is picked; "Cancel" while sending); the
//   progress / result inline above it; "Not listed? Use a code" expands a code field.
// Receive Levels: the ready state, the editable name, this device's receive code in a big card,
//   its address, and how sending works.

#include "net/LevelTransfer.h"

namespace net {

void showSendToNearby(const LevelPackage& level);
void showReceiveLevels();

}  // namespace net
