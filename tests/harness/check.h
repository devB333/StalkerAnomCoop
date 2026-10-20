// Minimal test harness for project tests: registration, CHECK/REQUIRE,
// recorded metrics and a JSON report. No third-party framework to pin.
//
//   TEST("group/name") { CHECK(x == 1); REQUIRE(ok); metric("rtt_p95_ms", v); }
//
// CHECK records a failure and continues; REQUIRE records it and ends the test.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace coop::test
{
struct Abort
{
};

struct Registrar
{
    Registrar(std::string name, std::function<void()> fn);
};

void fail(const char* file, int line, const std::string& what);
void metric(const std::string& name, double value);
void note(const std::string& text);
bool failed_now(); // the current test has failed so far

std::string fmt(const char* format, ...);
} // namespace coop::test

#define COOP_CAT2(a, b) a##b
#define COOP_CAT(a, b) COOP_CAT2(a, b)
#define TEST(name)                                                                   \
    static void COOP_CAT(test_fn_, __LINE__)();                                      \
    static ::coop::test::Registrar COOP_CAT(test_reg_, __LINE__)(name, &COOP_CAT(test_fn_, __LINE__)); \
    static void COOP_CAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                     \
    do                                                                  \
    {                                                                   \
        if (!(cond))                                                    \
            ::coop::test::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define CHECK_MSG(cond, ...)                                                                  \
    do                                                                                        \
    {                                                                                         \
        if (!(cond))                                                                          \
            ::coop::test::fail(__FILE__, __LINE__, "CHECK(" #cond "): " + ::coop::test::fmt(__VA_ARGS__)); \
    } while (0)

#define REQUIRE(cond)                                                     \
    do                                                                    \
    {                                                                     \
        if (!(cond))                                                      \
        {                                                                 \
            ::coop::test::fail(__FILE__, __LINE__, "REQUIRE(" #cond ")"); \
            throw ::coop::test::Abort{};                                  \
        }                                                                 \
    } while (0)

#define REQUIRE_MSG(cond, ...)                                                                  \
    do                                                                                          \
    {                                                                                           \
        if (!(cond))                                                                            \
        {                                                                                       \
            ::coop::test::fail(__FILE__, __LINE__, "REQUIRE(" #cond "): " + ::coop::test::fmt(__VA_ARGS__)); \
            throw ::coop::test::Abort{};                                                        \
        }                                                                                       \
    } while (0)
