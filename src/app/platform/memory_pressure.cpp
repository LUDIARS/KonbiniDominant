#include "konbini/app/platform/memory_pressure.h"

// @implements spec/interface/mobile-platform.md Lifecycle

namespace konbini::app {

// @implements spec/interface/mobile-platform.md Lifecycle
city::GeometryEvictionScope geometryEvictionScope(
    const MemoryPressureLevel level) noexcept {
    switch (level) {
        case MemoryPressureLevel::Normal:
            return city::GeometryEvictionScope::None;
        case MemoryPressureLevel::Moderate:
            return city::GeometryEvictionScope::Unused;
        case MemoryPressureLevel::Critical:
            return city::GeometryEvictionScope::AllRegenerable;
    }
    return city::GeometryEvictionScope::None;
}

}  // namespace konbini::app
