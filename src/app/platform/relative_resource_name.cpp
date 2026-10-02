#include "konbini/app/platform/relative_resource_name.h"

#include <algorithm>
#include <stdexcept>
#include <string>

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::app {

// @implements spec/interface/mobile-platform.md Assets and generated geometry
void validateRelativeResourceName(const std::string_view name) {
    const auto reject = [name](const char* const reason) {
        throw std::invalid_argument(
            std::string("invalid resource name '") + std::string(name) +
            "': " + reason);
    };
    if (name.empty()) {
        reject("empty");
    }
    if (name.front() == '/') {
        reject("absolute path");
    }
    if (name.find('\\') != std::string_view::npos) {
        reject("backslash separator");
    }
    if (name.find(':') != std::string_view::npos) {
        reject("drive letter or scheme");
    }
    std::size_t begin = 0;
    while (begin <= name.size()) {
        const std::size_t end = std::min(name.find('/', begin), name.size());
        const std::string_view segment = name.substr(begin, end - begin);
        if (segment.empty()) {
            reject("empty segment");
        }
        if (segment == "." || segment == "..") {
            reject("relative segment");
        }
        begin = end + 1;
    }
}

}  // namespace konbini::app
