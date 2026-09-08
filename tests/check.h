#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

// Test binaries in this repository do not link a test framework: the game
// libraries are the only dependency the build has, and a framework would add
// a fetch step to every configure. This header is the one shared piece so the
// individual test files stay a list of assertions about one module.
// @implements spec/test/verification-strategy.md 原則

namespace konbini::test {

inline int g_failureCount = 0;

inline void reportCheck(const bool passed, const char* const expression,
                        const char* const file, const int line) {
    if (passed) {
        return;
    }
    ++g_failureCount;
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
}

// Every builder under validation reports rejection by throwing, so a boolean
// assertion alone cannot express the contract. The exception type is part of
// the assertion: a builder that throws the wrong type has still lost the
// distinction between "caller passed nonsense" and "the module is broken".
template <typename Exception, typename Callable>
[[nodiscard]] bool throwsException(Callable&& callable) {
    try {
        static_cast<void>(std::forward<Callable>(callable)());
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

template <typename Callable>
[[nodiscard]] bool doesNotThrow(Callable&& callable) {
    try {
        static_cast<void>(std::forward<Callable>(callable)());
    } catch (...) {
        return false;
    }
    return true;
}

// Most expectations here are exact because the test mirrors the production
// expression. This is only for the few places where a value travels through
// float storage and back.
[[nodiscard]] inline bool approxEqual(const double left, const double right,
                                      const double tolerance) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] inline int summarize(const char* const name) {
    if (g_failureCount != 0) {
        std::fprintf(stderr, "%s: %d check(s) failed\n", name, g_failureCount);
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "%s: all checks passed\n", name);
    return EXIT_SUCCESS;
}

}  // namespace konbini::test

#define CHECK(expression) \
    ::konbini::test::reportCheck((expression), #expression, __FILE__, __LINE__)

#define CHECK_THROWS(exception_type, expression)               \
    ::konbini::test::reportCheck(                              \
        ::konbini::test::throwsException<exception_type>(      \
            [&] { expression; }),                              \
        "throws " #exception_type ": " #expression, __FILE__, __LINE__)

#define CHECK_NO_THROW(expression)                             \
    ::konbini::test::reportCheck(                              \
        ::konbini::test::doesNotThrow([&] { expression; }),     \
        "does not throw: " #expression, __FILE__, __LINE__)
