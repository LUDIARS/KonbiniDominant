#include "libcxx_float_from_chars.h"

#if defined(__ANDROID__) && defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 200000

#include <cerrno>
#include <cstdlib>
#include <string>

namespace {

// Longest prefix of [first, last) that is a decimal floating-point number in
// `format`, or inf / nan. Hexadecimal and leading '+' are not accepted, as in
// std::from_chars. Bionic's strto* is locale-independent ('.' only).
[[nodiscard]] std::size_t acceptedLength(const char* first, const char* last, const std::chars_format format) {
    const char* cursor = first;
    if (cursor != last && *cursor == '-') {
        ++cursor;
    }
    const auto lower = [](const char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); };
    const auto matches = [&](const char* word) {
        const char* probe = cursor;
        for (; *word != '\0'; ++word, ++probe) {
            if (probe == last || lower(*probe) != *word) {
                return false;
            }
        }
        return true;
    };
    if (matches("infinity")) {
        return static_cast<std::size_t>(cursor - first) + 8;
    }
    if (matches("inf") || matches("nan")) {
        return static_cast<std::size_t>(cursor - first) + 3;
    }
    const auto isDigit = [](const char c) { return c >= '0' && c <= '9'; };
    bool digits = false;
    while (cursor != last && isDigit(*cursor)) {
        ++cursor;
        digits = true;
    }
    if (cursor != last && *cursor == '.') {
        ++cursor;
        while (cursor != last && isDigit(*cursor)) {
            ++cursor;
            digits = true;
        }
    }
    if (!digits) {
        return 0;
    }
    const bool allowExponent = (static_cast<int>(format) & static_cast<int>(std::chars_format::scientific)) != 0;
    if (allowExponent && cursor != last && (*cursor == 'e' || *cursor == 'E')) {
        const char* exponent = cursor + 1;
        if (exponent != last && (*exponent == '+' || *exponent == '-')) {
            ++exponent;
        }
        if (exponent != last && isDigit(*exponent)) {
            while (exponent != last && isDigit(*exponent)) {
                ++exponent;
            }
            cursor = exponent;
        }
    }
    return static_cast<std::size_t>(cursor - first);
}

template <typename Value, typename Parse>
std::from_chars_result parseFloating(const char* first, const char* last, Value& value,
                                     const std::chars_format format, Parse parse) {
    const std::size_t length = acceptedLength(first, last, format);
    if (length == 0) {
        return {first, std::errc::invalid_argument};
    }
    const std::string text(first, length);
    char* end = nullptr;
    errno = 0;
    const Value parsed = parse(text.c_str(), &end);
    if (end != text.c_str() + text.size()) {
        return {first, std::errc::invalid_argument};
    }
    if (errno == ERANGE) {
        return {first + length, std::errc::result_out_of_range};
    }
    value = parsed;
    return {first + length, std::errc{}};
}

}  // namespace

namespace std {

from_chars_result from_chars(const char* first, const char* last, float& value, chars_format format) {
    return parseFloating(first, last, value, format, [](const char* s, char** e) { return std::strtof(s, e); });
}

from_chars_result from_chars(const char* first, const char* last, double& value, chars_format format) {
    return parseFloating(first, last, value, format, [](const char* s, char** e) { return std::strtod(s, e); });
}

from_chars_result from_chars(const char* first, const char* last, long double& value, chars_format format) {
    return parseFloating(first, last, value, format, [](const char* s, char** e) { return std::strtold(s, e); });
}

}  // namespace std

#endif
