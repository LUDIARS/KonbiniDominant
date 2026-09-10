#include "konbini/app/executable_directory.h"
#include <cstdint>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <vector>
#endif
namespace konbini::app {
// Resolve the running executable, never the caller's working directory. The
// packaged application therefore has the same asset contract on another PC.
std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(256);
    while (buffer.size() <= 32768) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) { throw std::runtime_error("cannot resolve executable path"); }
        if (length < buffer.size()) {
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
    throw std::runtime_error("executable path exceeds Windows path limit");
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        throw std::runtime_error("cannot resolve executable path");
    }
    return std::filesystem::canonical(buffer.data()).parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}
}  // namespace konbini::app
