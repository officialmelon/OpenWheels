#pragma once
// PC layer (not part of the original game): on an unhandled exception, write a symbolized stack
// trace to openwheels_crash.txt (and a minidump, openwheels_crash.dmp) next to the exe.

namespace openwheels {
namespace pc {

void installCrashHandler();

}  // namespace pc
}  // namespace openwheels
