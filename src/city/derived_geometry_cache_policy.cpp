#include "konbini/city/derived_geometry_cache_policy.h"

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::city {

// @implements spec/interface/mobile-platform.md Assets and generated geometry
bool DerivedGeometryCachePolicy::shouldEvict(
    const GeometryOrigin origin, const bool heldOutsideCache,
    const GeometryEvictionScope scope) const noexcept {
    // Packaged geometry is the fallback a cache miss relies on; dropping it
    // would leave nothing to draw until a background job finishes.
    if (origin != GeometryOrigin::Regenerated) {
        return false;
    }
    switch (scope) {
        case GeometryEvictionScope::None:
            return false;
        case GeometryEvictionScope::Unused:
            return !heldOutsideCache;
        case GeometryEvictionScope::AllRegenerable:
            return true;
    }
    return false;
}

}  // namespace konbini::city
