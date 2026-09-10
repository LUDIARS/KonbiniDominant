// @implements spec/feature/grid-town-and-vector-ui.md Square stores
#include "konbini/render/grid_store_connections.h"
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>

namespace konbini::render {
std::vector<GridStoreLinks> connectGridStores(
        std::span<const sim::RenderStore> stores, const double cellMeters) {
    if (!std::isfinite(cellMeters) || cellMeters <= 0)
        throw std::invalid_argument("grid connections require a positive cell size");
    using Key = std::tuple<std::uint32_t,std::uint32_t,sim::ChainId,long long,long long>;
    std::map<Key,std::size_t> locations;
    std::vector<GridStoreLinks> links(stores.size());
    const auto key = [cellMeters](const sim::RenderStore& store, int dx, int dz) {
        const auto x = static_cast<long long>(std::floor(store.positionMeters.x/cellMeters));
        const auto z = static_cast<long long>(std::floor(store.positionMeters.z/cellMeters));
        return Key{store.dimension,store.verticalSlot,store.chain,x+dx,z+dz};
    };
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const auto& store = stores[index];
        if (!store.id.isValid() || !store.facilityId.isValid() || !sim::isFinite(store.positionMeters) ||
            !sim::isSimulationChainId(store.chain) ||
            std::abs(store.positionMeters.x/cellMeters) > 1000000 ||
            std::abs(store.positionMeters.z/cellMeters) > 1000000)
            throw std::invalid_argument("invalid grid store");
        if (store.isAntiStore || store.chain == sim::ChainId::Aion) continue;
        if (!locations.emplace(key(store,0,0),index).second)
            throw std::invalid_argument("duplicate grid store footprint");
    }
    constexpr std::array<std::array<int,2>,4> offsets{{{-1,0},{1,0},{0,-1},{0,1}}};
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const auto& store = stores[index];
        if (store.isAntiStore || store.chain == sim::ChainId::Aion) continue;
        for (std::size_t side = 0; side < offsets.size(); ++side) {
            const auto found = locations.find(key(store,offsets[side][0],offsets[side][1]));
            if (found != locations.end()) links[index][side] = found->second;
        }
    }
    return links;
}
}
