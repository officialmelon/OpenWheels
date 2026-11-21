#ifndef __ANDROID__
#include "platform/common/FilePicker.h"

namespace openwheels {
namespace filepicker {

bool available() { return false; }

void pickLevelFiles(const std::string&, std::function<void(int, int)> done)
{
    if (done) done(-1, 0);
}

}  // namespace filepicker
}  // namespace openwheels
#endif
