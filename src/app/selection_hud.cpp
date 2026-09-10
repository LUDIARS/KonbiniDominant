// @implements spec/feature/pointer-controls.md
#include "konbini/app/selection_hud.h"
#include "konbini/app/selection_controller.h"
#include "konbini/sim/vertical_placement.h"
#include "konbini/sim/skill_system.h"
namespace konbini::app {
HudTextInput makeSelectionHud(const sim::RenderSnapshot& snapshot,const sim::FirstPlayableContent& content,
    const std::optional<sim::FacilityId> selected,const std::uint32_t floor) {
    HudTextInput input;
    input.hud=snapshot.hud();input.selectedFacility=selected;input.selectedFloor=floor;
    if(!selected) return input;
    for(const auto& facility:snapshot.facilities()) if(facility.id==*selected) {
        input.selectionIsPlacementCandidate=SelectionController::isPlacementCandidate(facility);break;
    }
    if(snapshot.hud().playerChain) {
        input.selectedBuildCostCredits=content.chain(*snapshot.hud().playerChain).buildCostCredits;
        if(content.campaign) {
            input.selectedBuildCostCredits=sim::verticalBuildCost(input.selectedBuildCostCredits,floor,content.campaign->vertical);
            sim::GameState pricing;pricing.playerChain=snapshot.hud().playerChain;
            pricing.campaign.enabled=true;pricing.campaign.skills=snapshot.hud().campaign.skills;
            input.selectedBuildCostCredits=sim::skillBuildCost(input.selectedBuildCostCredits,pricing,content.campaign->skills,*pricing.playerChain);
        }
    }
    for(const auto& store:snapshot.stores()) if(store.facilityId==*selected && store.verticalSlot==floor) {
        input.selectedStoreChain=store.chain;input.selectedFaith=store.faith;
        input.selectedAntiStore=store.isAntiStore;input.selectedBossForm=store.id.value().index%7;break;
    }
    return input;
}
}
