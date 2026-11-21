#include <cstdio>
#include <cstring>

#include "check.h"

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    int ran = 0, bad = 0;
    for (const check::Case& c : check::cases()) {
        if (only && !std::strstr(c.name, only)) continue;
        const int before = check::failures();
        c.fn();
        ++ran;
        if (check::failures() != before) {
            ++bad;
            std::printf("[FAIL] %s\n", c.name);
        } else {
            std::printf("[ ok ] %s\n", c.name);
        }
    }
    std::printf("\n%d tests, %d failed\n", ran, bad);
    return bad ? 1 : 0;
}
