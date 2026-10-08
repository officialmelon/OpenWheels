#include "platform/unix/CrashHandler.h"

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cstring>

namespace openwheels {
namespace pc {
namespace {

// Filled before any crash: the handler must not allocate.
char g_reportPath[4096] = {0};

void writeText(int fd, const char* text) {
    const ssize_t ignored = write(fd, text, strlen(text));
    (void)ignored;
}

void onFatalSignal(int sig) {
    const int fd = open(g_reportPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        writeText(fd, "OpenWheels crashed: ");
        writeText(fd, strsignal(sig));
        writeText(fd, "\n\nStack:\n");
        void* frames[128];
        const int n = backtrace(frames, 128);
        backtrace_symbols_fd(frames, n, fd);
        close(fd);
    }
    // Default action (core dump / termination) with the original signal.
    signal(sig, SIG_DFL);
    raise(sig);
}

}  // namespace

void installCrashHandler(const std::string& directory) {
    const std::string path = directory + "openwheels_crash.txt";
    std::strncpy(g_reportPath, path.c_str(), sizeof(g_reportPath) - 1);
    // Warm up backtrace() (it may load libgcc lazily, which is not async-signal-safe).
    void* warm[1];
    backtrace(warm, 1);
    for (int sig : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS}) signal(sig, onFatalSignal);
}

}  // namespace pc
}  // namespace openwheels
