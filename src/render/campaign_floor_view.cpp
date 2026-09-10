#include "konbini/render/campaign_floor_view.h"
#include <stdexcept>
namespace konbini::render {
sim::RenderFacility facilityOnFloor(const sim::RenderFacility& facility,const std::uint32_t floor,const double floorHeight) {
    auto shown=facility;
    if(floor==0) return shown;
    shown.positionMeters.y+=floor*floorHeight;
    shown.isLotRepresentation=true;
    // The pick volume is the current floor's lot footprint, not its buried ground building.
    shown.state=sim::FacilityState::Replaced;
    return shown;
}
std::optional<FacilityPick> pickCampaignFloor(const WorldRay& ray,const sim::RenderSnapshot& snapshot,const std::uint32_t floor) {
    std::vector<sim::RenderFacility> facilities;
    for(const auto& facility:snapshot.facilities())
        facilities.push_back(facilityOnFloor(facility,floor,snapshot.hud().campaign.floorHeightMeters));
    return pickFacility(ray,facilities);
}
std::vector<sim::RenderStore> storesInFloorBand(const sim::RenderSnapshot& snapshot,const std::uint32_t floor) {
    if(floor>=256) throw std::invalid_argument("invalid floor view");
    const std::uint32_t first=(floor/16)*16;
    std::vector<sim::RenderStore> stores;
    for(const auto& store:snapshot.stores())
        if(store.verticalSlot>=first && store.verticalSlot<first+16) stores.push_back(store);
    return stores;
}
}  // namespace konbini::render
