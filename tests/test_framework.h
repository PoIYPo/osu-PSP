// Tiny zero-dependency test framework: a test binary that returns non-zero on
// failure is all CI and a local PowerShell one-liner need.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace test {

using TestFunc = void (*)();

struct Case {
    const char* name;
    TestFunc fn;
};

std::vector<Case>& registry();
int& failures();
void fail(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, TestFunc fn) { registry().push_back({name, fn}); }
};

inline std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    return std::string(buf);
}

}  // namespace test

#define TEST_CASE(name)                                              \
    static void name();                                              \
    static ::test::Registrar test_registrar_##name(#name, name);     \
    static void name()

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) ::test::fail(__FILE__, __LINE__, "CHECK failed: " #cond); \
    } while (0)

#define CHECK_MSG(cond, msg)                                                     \
    do {                                                                         \
        if (!(cond)) ::test::fail(__FILE__, __LINE__, std::string("CHECK failed: " #cond " -- ") + (msg)); \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        const auto a_ = (a);                                                  \
        const auto b_ = (b);                                                  \
        if (!(a_ == b_)) {                                                    \
            ::test::fail(__FILE__, __LINE__,                                  \
                         std::string("CHECK_EQ failed: " #a " != " #b));      \
        }                                                                     \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                              \
    do {                                                                                   \
        const double a_ = static_cast<double>(a);                                          \
        const double b_ = static_cast<double>(b);                                          \
        const double e_ = static_cast<double>(eps);                                        \
        if (!(std::fabs(a_ - b_) <= e_)) {                                                 \
            ::test::fail(__FILE__, __LINE__,                                               \
                         std::string("CHECK_NEAR failed: " #a " = ") + ::test::num(a_) +    \
                             ", " #b " = " + ::test::num(b_) + ", eps = " + ::test::num(e_)); \
        }                                                                                  \
    } while (0)

#define CHECK_STR_EQ(a, b)                                                        \
    do {                                                                          \
        const std::string a_ = (a);                                               \
        const std::string b_ = (b);                                               \
        if (a_ != b_) {                                                           \
            ::test::fail(__FILE__, __LINE__,                                      \
                         std::string("CHECK_STR_EQ failed: " #a " = \"") + a_ +   \
                             "\", expected \"" + b_ + "\"");                      \
        }                                                                         \
    } while (0)
