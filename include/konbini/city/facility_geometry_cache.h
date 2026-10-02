#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>

#include "konbini/city/derived_geometry_cache_policy.h"
#include "konbini/city/facility_geometry.h"

// @implements spec/interface/figmentum-city-generation.md Geometry generation

namespace konbini::city {

// geometry の生成は tick 外の worker から呼ばれうるので、内部を mutex で
// 守る。cache 自体は immutable な geometry を共有するだけで、simulation
// state は持たない。
// @implements spec/interface/figmentum-city-generation.md Geometry generation
class FacilityGeometryCache {
public:
    [[nodiscard]] std::shared_ptr<const FacilityGeometry> find(
        const FacilityGeometryCacheKey& key) const;
    // An existing entry for the same key wins, including its origin.
    [[nodiscard]] std::shared_ptr<const FacilityGeometry> insert(
        std::shared_ptr<const FacilityGeometry> geometry,
        GeometryOrigin origin = GeometryOrigin::Regenerated);
    // Memory-pressure response. Returns how many entries were dropped; a
    // dropped key regenerates from its recipe on the next miss.
    // @implements spec/interface/mobile-platform.md Assets and generated geometry
    std::size_t evict(
        const DerivedGeometryCachePolicy& policy,
        GeometryEvictionScope scope);
    [[nodiscard]] std::size_t size() const;

private:
    struct Entry {
        std::shared_ptr<const FacilityGeometry> geometry;
        GeometryOrigin origin = GeometryOrigin::Regenerated;
    };

    mutable std::mutex mutex_;
    std::map<FacilityGeometryCacheKey, Entry> entries_;
};

}  // namespace konbini::city
