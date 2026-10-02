#pragma once

#include <cstdint>

// @implements spec/interface/mobile-platform.md Assets and generated geometry

namespace konbini::city {

// Where a cached facility geometry came from. Packaged low-LOD geometry ships
// with the read-only assets and cannot be rebuilt on the device, so only
// `Regenerated` (polygonized from the Figmentum recipe) is derived data.
enum class GeometryOrigin : std::uint8_t {
    PackagedLowLod,
    Regenerated,
};

// How much derived geometry a memory-pressure response may drop. The owner
// picks the scope; the policy decides entry by entry.
enum class GeometryEvictionScope : std::uint8_t {
    None,
    // Regenerated geometry nobody outside the cache currently holds.
    Unused,
    // Every regenerated entry. Holders keep their shared instance alive.
    AllRegenerable,
};

// Decides which cache entries are evictable. It owns no entries and never
// touches simulation state, so a wrong scope can only cost a regeneration,
// never the authoritative match.
// @implements spec/interface/mobile-platform.md Assets and generated geometry
class DerivedGeometryCachePolicy {
public:
    [[nodiscard]] bool shouldEvict(
        GeometryOrigin origin, bool heldOutsideCache,
        GeometryEvictionScope scope) const noexcept;
};

}  // namespace konbini::city
