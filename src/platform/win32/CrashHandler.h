#pragma once
// PC layer (not part of the original game): on an unhandled exception, write a symbolized stack
// trace to openwheels_crash.txt (and a minidump, openwheels_crash.dmp) next to the exe.

namespace openwheels {
namespace pc {

void installCrashHandler();

// Hang watchdog: call installHangWatchdog() on the main thread, then heartbeat() once per frame.
// If no heartbeat arrives for 10 s, the main thread's stack is written to openwheels_hang.txt
// (the game keeps running).
void installHangWatchdog();
void heartbeat();

}  // namespace pc
}  // namespace openwheels
