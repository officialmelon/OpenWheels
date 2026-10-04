// NET (PC addition): see NetLevels.h.
#include "net/NetLevels.h"

#include "net/LevelTransfer.h"

namespace net {

void startLevelSharing() {
    LevelTransfer::getInstance()->start();
}

void stopLevelSharing() {
    LevelTransfer::getInstance()->shutdown();
}

}  // namespace net
