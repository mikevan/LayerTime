// Dependency-free test harness. No framework, no IDE, no platform.
// Builds with: g++ -std=c++17 -Wall -Wextra -o t <test>.cpp <sources>.cpp
#pragma once
#include <cstdio>
#include <cstring>
#include <cmath>

namespace check {
inline int g_checks = 0;
inline int g_failed = 0;
inline const char *g_case = "";
inline int g_caseFailed = 0;
// When set (from argv[1]) only this case runs. Per-case isolation is what
// makes density measurable: each run produces its own coverage profile.
inline const char *g_only = nullptr;

inline void beginCase(const char *name) { g_case = name; g_caseFailed = 0; }
inline void endCase() {
    printf("%s  %s\n", g_caseFailed ? "FAIL" : "pass", g_case);
}
inline void fail(const char *file, int line, const char *what) {
    ++g_failed; ++g_caseFailed;
    printf("      %s:%d  %s\n", file, line, what);
}
}

#define CASE(fn) do { \
    if (!check::g_only || std::strcmp(check::g_only, #fn) == 0) { \
        check::beginCase(#fn); fn(); check::endCase(); } } while (0)

// Put at the top of main(). Enables `./tests <case_name>` to run one case.
#define CHECK_MAIN(argc, argv) do { \
    if ((argc) > 1) check::g_only = (argv)[1]; } while (0)

#define CHECK_TRUE(c) do { ++check::g_checks; if (!(c)) \
    check::fail(__FILE__, __LINE__, "expected true: " #c); } while (0)

#define CHECK_FALSE(c) do { ++check::g_checks; if ((c)) \
    check::fail(__FILE__, __LINE__, "expected false: " #c); } while (0)

#define CHECK_INT(expected, actual) do { ++check::g_checks; \
    const long _e = (long)(expected), _a = (long)(actual); \
    if (_e != _a) { char b[256]; snprintf(b, sizeof(b), \
        "%s: expected %ld, got %ld", #actual, _e, _a); \
        check::fail(__FILE__, __LINE__, b); } } while (0)

#define CHECK_CHAR(expected, actual) do { ++check::g_checks; \
    const char _e = (char)(expected), _a = (char)(actual); \
    if (_e != _a) { char b[256]; snprintf(b, sizeof(b), \
        "%s: expected '%c', got '%c'", #actual, _e, _a); \
        check::fail(__FILE__, __LINE__, b); } } while (0)

#define CHECK_NEAR(expected, actual, tol) do { ++check::g_checks; \
    const double _e = (double)(expected), _a = (double)(actual), _t = (double)(tol); \
    if (!(std::fabs(_e - _a) <= _t)) { char b[256]; snprintf(b, sizeof(b), \
        "%s: expected %.6f +/- %.6f, got %.6f (off by %.6f)", \
        #actual, _e, _t, _a, std::fabs(_e - _a)); \
        check::fail(__FILE__, __LINE__, b); } } while (0)

#define CHECK_STR(expected, actual) do { ++check::g_checks; \
    if (std::strcmp((expected), (actual)) != 0) { char b[512]; \
        snprintf(b, sizeof(b), "expected \"%s\", got \"%s\"", (expected), (actual)); \
        check::fail(__FILE__, __LINE__, b); } } while (0)

#define CHECK_SUMMARY() do { \
    printf("\n%d checks, %d failed\n", check::g_checks, check::g_failed); \
    return check::g_failed == 0 ? 0 : 1; } while (0)
