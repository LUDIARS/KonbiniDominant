# Script-mode test: KonbiniDominant builds against standard libraries without
# floating-point std::from_chars (Apple libc++, Android NDK libc++ 18) without
# patching its dependencies. The pinned Pictor / Figmentum parse floats on
# their own, so no target may be force-included with a charconv shim again,
# and KD's own floating-point parse stays behind the feature-test macro.
# @implements spec/setup/macos-development.md Dependency revisions
# @implements spec/setup/mobile-development.md NDK libc++ compatibility

set(root "${CMAKE_CURRENT_LIST_DIR}/../..")

file(READ "${root}/mobile/CMakeLists.txt" mobile_build)
if(mobile_build MATCHES "-include[ \t]")
  message(SEND_ERROR "FAIL: mobile/CMakeLists.txt force-includes a header into a target")
endif()
if(mobile_build MATCHES "compat/")
  message(SEND_ERROR "FAIL: mobile/CMakeLists.txt compiles an Android libc++ compat shim")
endif()

# json_document.cpp is the only KD source that parses a floating-point value.
# Its std::from_chars call must sit inside the __cpp_lib_to_chars branch.
file(READ "${root}/src/sim/content/json_document.cpp" json_source)
string(FIND "${json_source}" "#if defined(__cpp_lib_to_chars)" guard_begin)
string(FIND "${json_source}" "std::from_chars(" call)
string(FIND "${json_source}" "#else" guard_else)
if(guard_begin EQUAL -1 OR call EQUAL -1 OR guard_else EQUAL -1
    OR NOT call GREATER guard_begin OR NOT guard_else GREATER call)
  message(SEND_ERROR
    "FAIL: json_document.cpp floating-point std::from_chars is not guarded by __cpp_lib_to_chars")
endif()
