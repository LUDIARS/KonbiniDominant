#pragma once
#include "konbini/render/facility_picker.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::render {
sim::RenderFacility facilityOnFloor(const sim::RenderFacility& facility,std::uint32_t floor,double floorHeight);
std::optional<FacilityPick> pickCampaignFloor(const WorldRay& ray,const sim::RenderSnapshot& snapshot,std::uint32_t floor);
std::vector<sim::RenderStore> storesInFloorBand(const sim::RenderSnapshot& snapshot,std::uint32_t floor);
}
