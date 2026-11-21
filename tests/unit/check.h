#pragma once
// A tiny test framework (no dependencies): TEST(name) { CHECK(cond); CHECK_EQ(a, b); }
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace check {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& cases() { static std::vector<Case> c; return c; }
inline int& failures() { static int f = 0; return f; }
struct Reg { Reg(const char* n, std::function<void()> f) { cases().push_back({n, f}); } };
}  // namespace check

#define TEST(name) \
    static void test_##name(); \
    static check::Reg reg_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond) \
    do { if (!(cond)) { std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++check::failures(); } } while (0)

#define CHECK_EQ(a, b) \
    do { const auto va = (a); const auto vb = (b); if (!(va == vb)) { \
        std::printf("    FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); ++check::failures(); } } while (0)

#define CHECK_CONTAINS(hay, needle) \
    do { const std::string h_ = (hay); if (h_.find(needle) == std::string::npos) { \
        std::printf("    FAIL %s:%d: \"%s\" does not contain \"%s\"\n", __FILE__, __LINE__, h_.c_str(), needle); ++check::failures(); } } while (0)
