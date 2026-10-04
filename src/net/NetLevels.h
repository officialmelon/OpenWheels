#pragma once
// NET (PC addition): start-up / shutdown hooks of the LAN level transfer for AppDelegate
// (src/net/LevelTransfer.h). Builds without the editor (OW_WITH_EDITOR=OFF) link stubs instead.

namespace net {

// Opens the TCP listener and starts announcing this game on the LAN. Cocos thread.
void startLevelSharing();
// Stops announcing and joins the network thread (at exit).
void stopLevelSharing();

}  // namespace net
