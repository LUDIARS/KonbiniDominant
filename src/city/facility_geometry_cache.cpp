#include "konbini/city/facility_geometry_cache.h"

#include <stdexcept>
#include <utility>

// @implements spec/interface/figmentum-city-generation.md Geometry generation

namespace konbini::city {

// @implements spec/interface/figmentum-city-generation.md Geometry generation
std::shared_ptr<const FacilityGeometry> FacilityGeometryCache::find(
    const FacilityGeometryCacheKey& key) const {
    std::scoped_lock lock(mutex_);
    const auto found = entries_.find(key);
    return found == entries_.end() ? nullptr : found->second;
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
std::shared_ptr<const FacilityGeometry> FacilityGeometryCache::insert(
    std::shared_ptr<const FacilityGeometry> geometry) {
    if (geometry == nullptr ||
        geometry->cacheKey.schemaVersion !=
            kFacilityGeometryCacheSchemaVersion ||
        geometry->cacheKey.generatorRevision.empty() ||
        geometry->cacheKey.recipeHash == 0 ||
        geometry->cacheKey.polygonizeResolution < 1 ||
        geometry->cacheKey.vertexFormatVersion !=
            kFacilityVertexFormatVersion) {
        throw std::invalid_argument("invalid facility geometry cache entry");
    }
    // key は move の前に取り出す。`emplace(geometry->cacheKey,
    // std::move(geometry))` は同一 object を読みつつ move する形になり、
    // 正しさが pair の member 初期化順という暗黙の前提に依存してしまう。
    const FacilityGeometryCacheKey key = geometry->cacheKey;
    std::scoped_lock lock(mutex_);
    // An equal cache key must map to one shared geometry, so an existing entry
    // wins and the caller gets the canonical instance back.
    const auto entry = entries_.emplace(key, std::move(geometry)).first;
    return entry->second;
}

// @implements spec/interface/figmentum-city-generation.md Geometry generation
std::size_t FacilityGeometryCache::size() const {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

}  // namespace konbini::city
