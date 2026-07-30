#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>

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
    [[nodiscard]] std::shared_ptr<const FacilityGeometry> insert(
        std::shared_ptr<const FacilityGeometry> geometry);
    [[nodiscard]] std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::map<FacilityGeometryCacheKey,
             std::shared_ptr<const FacilityGeometry>>
        entries_;
};

}  // namespace konbini::city
