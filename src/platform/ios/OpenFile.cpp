// iOS "Open in OpenWheels" (OpenWheels port): hands a received .happywheels / level file to the
// level store, like --open on PC.

#include <string>

#include "LevelSession.h"

void openwheelsOpenFile(const std::string& path)
{
    LevelSession::getInstance()->openHappyWheelsFile(path);
}
