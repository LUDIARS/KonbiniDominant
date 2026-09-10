#include "konbini/sim/dimension_system.h"
#include "konbini/sim/counter_rng.h"
#include "konbini/sim/population_change_system.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace konbini::sim {
std::optional<FacilityId> anchorFacility(const FacilityTable& facilities,const std::uint32_t dimension,const std::uint64_t key) {
    for(std::size_t i=0;i<facilities.size();++i) {
        const auto f=facilities.row(i);
        if(f.dimension==dimension && f.figmentumKey.value()==key) return f.id;
    }
    return {};
}
// @implements spec/feature/full-campaign-baseline.md Multiverse
std::uint32_t createCampaignDimension(CampaignWorld w,const bool foreignObjective) {
    auto& campaign=w.state.campaign;
    const auto& rules=*w.content.campaign;
    const auto active=std::ranges::count_if(campaign.dimensions,[](const auto& d){return d.status==DimensionStatus::Active;});
    if(active>=rules.dimensions.maxActive || campaign.nextDimension==std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("active dimension capacity exhausted");
    const auto ordinal=campaign.nextDimension++;
    const auto seed=splitMix64(w.state.worldSeed ^ 0x44494D454E53494Full ^
        (static_cast<std::uint64_t>(rules.dimensions.seedVersion)<<32) ^ ordinal);
    campaign.dimensions.push_back({ordinal,seed,DimensionStatus::Active,foreignObjective});
    const auto originalSize=w.facilities.size();
    for(std::size_t i=0;i<originalSize;++i) {
        auto facility=w.facilities.row(i);
        if(facility.dimension!=0) continue;
        facility.id=w.identities.facilities().acquire();
        facility.dimension=ordinal; facility.state=FacilityState::Intact;
        w.facilities.append(facility);
        if(!facility.isBuildable) continue;
        const auto population=w.content.population.basePopulation+
            static_cast<std::uint32_t>(counterRandom({seed,RandomStreamId::FirstPlayablePopulation,
                0,facility.figmentumKey.value(),ordinal})%w.content.population.randomPopulationCount);
        w.population.append({.id=w.identities.populationCells().acquire(),.facilityId=facility.id,
            .dimension=ordinal,.positionMeters=facility.positionMeters,.population=population});
    }
    return ordinal;
}
// @implements spec/feature/full-campaign-baseline.md Multiverse
void seedForeignOpponents(CampaignWorld w,const std::uint32_t dimension,const ChainId chain) {
    const auto count=w.content.campaign->dimensions.initialRivalStores;
    const auto seed=w.state.campaign.dimensions.back().seed;
    std::vector<FacilityRow> candidates;
    for(std::size_t i=0;i<w.facilities.size();++i) {
        auto row=w.facilities.row(i);
        if(row.dimension==dimension && row.isBuildable) candidates.push_back(row);
    }
    std::sort(candidates.begin(),candidates.end(),[&](const auto& a,const auto& b) {
        const auto ak=splitMix64(seed^a.figmentumKey.value()),bk=splitMix64(seed^b.figmentumKey.value());
        return ak!=bk ? ak<bk : a.id<b.id;
    });
    // Each new foreign garrison receives its own explicit starting capital.
    const auto base=w.economy.rules(chain).buildCostCredits;
    if(base>std::numeric_limits<std::int64_t>::max()/count)
        throw std::overflow_error("foreign starting capital overflow");
    w.economy.creditRevenue(chain,base*count);
    std::uint32_t placed=0;
    for(const auto& row:candidates) {
        if(placed==count) break;
        const PlaceStoreCommand command{{w.state.completedTicks,CommandSourcePriority::Ai,placed},chain,row.id,0};
        const auto result=commitPlacementAtomically(command,w.state,w.facilities,w.stores,w.economy,w.identities.stores());
        if(result.failure==PlacementFailure::None) {
            ++placed;
            applyPopulationLoss(w.population,row.id,w.content.phase1->destructionPopulationLossPercent);
        }
    }
    if(placed!=count)
        throw std::logic_error("foreign dimension lacks enough valid garrison lots");
}
void destroyCampaignStore(CampaignWorld w,const StoreId id) {
    const auto index=w.stores.find(id);
    if(!index || !w.stores.row(*index).isActive) return;
    const auto store=w.stores.row(*index);
    (void)w.stores.deactivate(id); w.economy.removeStore(store.chain);
    (void)w.facilities.setState(store.facilityId,w.stores.hasActiveStoreAt(store.facilityId)
        ? FacilityState::Replaced : FacilityState::Destroyed);
    if(store.verticalSlot==0) applyPopulationLoss(w.population,store.facilityId,
        w.content.phase1->destructionPopulationLossPercent);
}
void collapseCampaignDimension(CampaignWorld w,const std::uint32_t dimension,const bool annihilation) {
    auto& dimensions=w.state.campaign.dimensions;
    const auto found=std::ranges::find_if(dimensions,[&](const auto& d){return d.id==dimension;});
    if(found==dimensions.end() || found->status!=DimensionStatus::Active) return;
    found->status=DimensionStatus::Destroyed;
    found->maxValueAtTick=0; found->maxValueTriangle={}; found->energyEndTick=0;
    if(annihilation && found->foreignObjective) ++w.state.campaign.destroyedForeign;
    for(std::size_t i=0;i<w.stores.size();++i) {
        const auto store=w.stores.row(i);
        if(store.dimension==dimension && store.isActive) destroyCampaignStore(w,store.id);
    }
    for(std::size_t i=0;i<w.facilities.size();++i) {
        const auto facility=w.facilities.row(i);
        if(facility.dimension==dimension) (void)w.facilities.setState(facility.id,FacilityState::Destroyed);
    }
    for(std::size_t i=0;i<w.population.size();++i) {
        if(w.population.row(i).dimension==dimension) {
            w.population.assign(i,{},{}); w.population.setPopulation(i,0);
        }
    }
}
void resolveAnnihilations(CampaignWorld w) {
    if(!w.state.playerChain) return;
    std::vector<std::pair<StoreId,StoreId>> pairs;
    for(std::size_t i=0;i<w.stores.size();++i) {
        const auto anti=w.stores.row(i);
        if(!anti.isActive || !anti.isAntiStore || anti.dimension==0 ||
           !dimensionActive(w.state.campaign,anti.dimension)) continue;
        const auto facility=w.facilities.row(*w.facilities.find(anti.facilityId));
        const auto origin=anchorFacility(w.facilities,0,facility.figmentumKey.value());
        if(!origin) continue;
        const auto own=w.stores.findAt(*origin,anti.verticalSlot);
        if(own && w.stores.row(*own).chain==*w.state.playerChain) pairs.emplace_back(anti.id,w.stores.row(*own).id);
    }
    for(const auto& [antiId,ownId]:pairs) {
        const auto anti=w.stores.row(*w.stores.find(antiId));
        const auto own=w.stores.row(*w.stores.find(ownId));
        if(!anti.isActive || !own.isActive) continue;
        destroyCampaignStore(w,antiId); destroyCampaignStore(w,ownId);
        collapseCampaignDimension(w,anti.dimension,true);
        for(auto& dimension:w.state.campaign.dimensions) {
            if(dimension.id==0 && dimension.status==DimensionStatus::Active)
                dimension.energyEndTick=w.state.campaign.elapsedTicks+w.content.campaign->dimensions.energyDurationTicks;
        }
    }
}
}
