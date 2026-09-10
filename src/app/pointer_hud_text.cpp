// @implements spec/feature/pointer-controls.md
#include "konbini/app/pointer_hud_text.h"
#include "konbini/app/skill_text.h"
namespace konbini::app {
std::vector<std::string> buildPointerHudLines(const HudTextInput& input) {
    const auto& h=input.hud;const auto& c=h.campaign;
    if(h.phase==sim::GamePhase::ChainSelect || h.phase==sim::GamePhase::Result || c.skills.pending) return {};
    std::vector<std::string> lines{"KONBINI DOMINANT"};
    const auto phase=h.phase==sim::GamePhase::Phase1?"PHASE 1 - CITY":
        h.phase==sim::GamePhase::Phase2?"PHASE 2 - TOWER":
        h.phase==sim::GamePhase::Phase3?"PHASE 3 - WORLDS":
        h.phase==sim::GamePhase::BossWarning?"WARNING - AION APPROACHES":"PHASE 4 - SURVIVE AION";
    lines.emplace_back(input.isPaused?"PAUSED":phase);
    lines.push_back(hudChainLabel(*h.playerChain)+" CASH "+std::to_string(h.cashCredits)+" STORES "+std::to_string(h.storeCount));
    const auto ticks=h.phase==sim::GamePhase::Phase1?h.ticksRemaining:c.phaseRemaining;
    if(h.phase!=sim::GamePhase::Phase3) lines.push_back("TIME "+std::to_string((ticks+h.ticksPerSecond-1)/h.ticksPerSecond)+"S");
    if(c.enabled) {
        lines.push_back("LEVEL "+std::to_string(c.skills.level)+" XP "+std::to_string(c.skills.experience)+"/"+std::to_string(c.nextExperience));
        if(c.reachedPhase>=2) lines.push_back("FLOOR "+std::to_string(input.selectedFloor+1)+" / "+std::to_string(c.slots));
        if(c.reachedPhase>=3) lines.push_back("WORLD D"+std::to_string(c.visibleDimension)+" CONQUERED "+std::to_string(c.destroyedForeign)+"/2");
        for(const auto& d:c.dimensions) if(d.maxValueRemaining)
            lines.push_back("D"+std::to_string(d.id)+" MAXVALUE IN "+std::to_string((d.maxValueRemaining+h.ticksPerSecond-1)/h.ticksPerSecond)+"S");
        if(c.reachedPhase>=3) for(const auto& d:c.dimensions)
            if(d.id==c.visibleDimension && !d.active) lines.emplace_back("WORLD COLLAPSED - ACTIONS TO LEAVE");
        if(c.skills.waveEndTick>c.elapsedTicks) lines.emplace_back("MIND WAVE ACTIVE");
        if(c.skills.snacksEndTick>c.elapsedTicks) lines.emplace_back("HOT SNACKS ACTIVE");
        if(c.skills.coffeePayout && c.skills.coffeeAtTick+20>c.elapsedTicks)
            lines.push_back("COFFEE +"+std::to_string(c.skills.coffeePayout));
    }
    lines.push_back("INCOME "+std::to_string(h.predictedEconomyIncomeCredits));
    if(c.reachedPhase>=3) for(const auto& d:c.dimensions) if(d.id==c.visibleDimension)
        lines.push_back("OWN "+std::to_string(d.playerStores)+" RIVALS "+std::to_string(d.rivalStores)+" AION "+std::to_string(d.bossStores));
    if(input.selectedAntiStore) lines.emplace_back("ANTI STORE - MATCH D0 AT THIS FLOOR");
    if(input.selectedStoreChain) lines.push_back(hudChainLabel(*input.selectedStoreChain)+" FAITH "+std::to_string(input.selectedFaith));
    else if(input.gridPlacement && h.phase==sim::GamePhase::Phase1) lines.emplace_back("TAP EMPTY GRID TO BUILD");
    else lines.emplace_back(input.selectedFacility?"TAP BUILD TO PLACE A STORE":"TAP A LOT TO SELECT");
    if(h.threatenedPlayerStores) lines.push_back("ENCIRCLED "+std::to_string(h.threatenedPlayerStores));
    if(input.lastPlacementFailure && *input.lastPlacementFailure!=sim::PlacementFailure::None)
        lines.emplace_back(hudPlacementFailureText(*input.lastPlacementFailure));
    if(input.lastCampaignFailure && *input.lastCampaignFailure!=sim::CampaignFailure::None)
        lines.emplace_back("ACTION UNAVAILABLE - CHECK CASH AND TARGET");
    if(input.droppedTicks) lines.push_back("DROPPED TICKS "+std::to_string(input.droppedTicks));
    if(input.showControls) {
        lines.emplace_back("DRAG CITY TO MOVE / PINCH TO ZOOM");
        lines.emplace_back(input.gridPlacement && h.phase==sim::GamePhase::Phase1
            ? "TAP EMPTY GRID TO BUILD / ADJACENT STORES CONNECT"
            : "TAP A LOT THEN BUILD / HOLD BUILD TO STACK");
        lines.emplace_back("FLOORS - CHANGE LEVEL / NEXT FREE / GROUND");
        lines.emplace_back("ACTIONS - IMAGE / WORLD / INVERT / ESCAPE");
        if(c.reachedPhase==1) lines.emplace_back("DOMINATE OR SURVIVE TO ADVANCE");
        if(c.reachedPhase==2) lines.emplace_back("256 FLOORS OR SURVIVE TO ADVANCE");
        if(c.reachedPhase==3) lines.emplace_back("INVERT AND MATCH D0 / OR ELIMINATE RIVALS");
        for(std::size_t i=0;i<sim::kSkillCount;++i) if(c.skills.ranks[i])
            lines.push_back(skillLabel(static_cast<sim::SkillId>(i))+" LV "+std::to_string(c.skills.ranks[i]));
    }
    return lines;
}
}
