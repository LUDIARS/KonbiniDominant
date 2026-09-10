#pragma once
#include "konbini/sim/campaign_world.h"
namespace konbini::sim {
std::uint32_t createCampaignDimension(CampaignWorld world,bool foreignObjective);
void seedForeignOpponents(CampaignWorld world,std::uint32_t dimension,ChainId chain);
void destroyCampaignStore(CampaignWorld world,StoreId store);
void collapseCampaignDimension(CampaignWorld world,std::uint32_t dimension,bool annihilation);
void resolveForeignConquest(CampaignWorld world);
void resolveAnnihilations(CampaignWorld world);
std::optional<FacilityId> anchorFacility(const FacilityTable& facilities,std::uint32_t dimension,std::uint64_t key);
}
