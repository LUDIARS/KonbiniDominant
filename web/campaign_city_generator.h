#pragma once
#include "konbini/city/i_city_generator.h"
#include <stdexcept>
namespace konbini::web {
// SimulationHost builds campaign grids directly. Legacy Figmentum content is
// deliberately rejected instead of being replaced by a different city.
class CampaignCityGenerator final : public city::ICityGenerator {
public:
    city::CityManifest planFirstPlayableCity(
            sim::GenerationalIdPool<sim::FacilityId>&) const override {
        throw std::runtime_error("Web edition requires campaign content");
    }
    std::shared_ptr<const city::FacilityGeometry> buildFacility(
            const city::ManifestFacility&) const override {
        throw std::runtime_error("Web edition does not load legacy facility meshes");
    }
    city::GeneratedCity generateFirstPlayableCity(
            sim::GenerationalIdPool<sim::FacilityId>&) const override {
        throw std::runtime_error("Web edition requires campaign content");
    }
};
}
