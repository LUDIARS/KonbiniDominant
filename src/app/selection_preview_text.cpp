// @implements spec/feature/ui-ux.md Smartphone interaction
#include "konbini/app/selection_preview_text.h"
namespace konbini::app {
std::vector<std::string> buildSelectionPreviewLines(const HudTextInput& input) {
    if(!input.selectedFacility) return {};
    if(input.selectedStoreChain)
        return {hudChainLabel(*input.selectedStoreChain)+" FAITH "+std::to_string(input.selectedFaith)};
    if(!input.selectionIsPlacementCandidate) return {"SELECTED - NOT PLACEABLE"};
    std::string line="SELECTED LOT";
    if(input.hud.campaign.enabled && input.hud.campaign.reachedPhase>=2)
        line+=" FLOOR "+std::to_string(input.selectedFloor+1);
    line+=" COST "+std::to_string(input.selectedBuildCostCredits);
    return {line};
}
std::string placementHintLine(const HudTextInput& input) {
    const bool gridImmediate=input.gridPlacement && input.hud.phase==sim::GamePhase::Phase1 && !input.tapSelectsOnly;
    if(gridImmediate) return "TAP EMPTY GRID TO BUILD";
    return input.selectedFacility?"TAP PLACE TO BUILD A STORE":"TAP A LOT TO SELECT";
}
}
