// bionic rand()/srand() == NetBSD random()/srandom() with the default TYPE_3 state
// (degree 31, separation 3). See BionicRand.h.
#if !defined(__ANDROID__)
#include <cstdint>

namespace {

const int kDeg = 31;
const int kSep = 3;

uint32_t g_state[kDeg];
int g_front = kSep;  // fptr index
int g_rear = 0;      // rptr index
bool g_seeded = false;

int32_t goodRand(int32_t x) {
    // Park-Miller minimal standard, Schrage's method, as in NetBSD random.c
    const int32_t hi = x / 127773;
    const int32_t lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x <= 0) x += 0x7fffffff;
    return x;
}

int32_t next() {
    g_state[g_front] += g_state[g_rear];
    const int32_t result = (int32_t)((g_state[g_front] >> 1) & 0x7fffffff);
    if (++g_front >= kDeg) {
        g_front = 0;
        ++g_rear;
    } else if (++g_rear >= kDeg) {
        g_rear = 0;
    }
    return result;
}

void seed(uint32_t x) {
    g_state[0] = x;
    for (int i = 1; i < kDeg; ++i) g_state[i] = (uint32_t)goodRand((int32_t)g_state[i - 1]);
    g_front = kSep;
    g_rear = 0;
    for (int i = 0; i < 10 * kDeg; ++i) next();
    g_seeded = true;
}

}  // namespace

extern "C" void ow_bionic_srand(unsigned int s) { seed(s); }

extern "C" int ow_bionic_rand(void) {
    if (!g_seeded) seed(1);  // libc's built-in table is the srandom(1) state
    return next();
}
#endif
