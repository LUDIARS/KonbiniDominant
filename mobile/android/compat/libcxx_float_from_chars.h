#pragma once

// Android NDK r27 ships libc++ 18, which declares integral `std::from_chars`
// only; floating-point overloads arrived in libc++ 20. The pinned Figmentum
// (garment profile) and Pictor (visus JSON) parse floats with them, so the
// Android package force-includes this header into those dependency targets
// only (mobile/CMakeLists.txt). The upstream sources stay untouched, and the
// shim disappears on its own once the NDK libc++ provides the overloads.
// KonbiniDominant's own code does not rely on it.

#include <charconv>

#if defined(__ANDROID__) && defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 200000

namespace std {

// Same contract as the C++17 overloads: no leading whitespace or '+', no
// "0x" prefix for chars_format::general, `ptr == first` with
// errc::invalid_argument when nothing parses, errc::result_out_of_range when
// the value does not fit (the value is left unchanged in both cases).
from_chars_result from_chars(const char* first, const char* last, float& value,
                             chars_format format = chars_format::general);
from_chars_result from_chars(const char* first, const char* last, double& value,
                             chars_format format = chars_format::general);
from_chars_result from_chars(const char* first, const char* last, long double& value,
                             chars_format format = chars_format::general);

}  // namespace std

#endif
