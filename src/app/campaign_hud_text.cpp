#include "konbini/app/campaign_hud_text.h"
#include "konbini/app/skill_text.h"
#include <array>
namespace konbini::app {
namespace {
std::string seconds(std::uint64_t ticks,std::uint32_t rate) {
    return std::to_string((ticks+rate-1)/rate)+"S";
}
std::string failure(sim::CampaignFailure value) {
    switch(value) {
    case sim::CampaignFailure::ChooseUpgradeFirst:return "CHOOSE AN UPGRADE FIRST";
    case sim::CampaignFailure::InvalidUpgrade:return "CHOOSE AN AVAILABLE UPGRADE";
    case sim::CampaignFailure::None:return "ACTION COMPLETE";
    case sim::CampaignFailure::NotUnlocked:return "ACTION NOT UNLOCKED";
    case sim::CampaignFailure::WrongChain:return "NOT YOUR CHAIN";
    case sim::CampaignFailure::NoTarget:return "SELECT A STORE ON THIS FLOOR";
    case sim::CampaignFailure::Collapsed:return "DIMENSION HAS COLLAPSED";
    case sim::CampaignFailure::NotRival:return "TARGET A FOREIGN RIVAL STORE";
    case sim::CampaignFailure::FaithRequired:return "TARGET NEEDS MAXIMUM FAITH";
    case sim::CampaignFailure::AlreadyInverted:return "STORE ALREADY INVERTED";
    case sim::CampaignFailure::InsufficientCash:return "NOT ENOUGH CASH";
    case sim::CampaignFailure::Cooldown:return "ACTION IS COOLING DOWN";
    case sim::CampaignFailure::DimensionCapacity:return "ACTIVE DIMENSION LIMIT REACHED";
    }
    return "ACTION REJECTED";
}
}
std::vector<std::string> buildCampaignHudLines(const HudTextInput& input) {
    const auto& h=input.hud; const auto& c=h.campaign;
    std::vector<std::string> lines{"KONBINI DOMINANT"};
    if(h.phase==sim::GamePhase::ChainSelect) {
        lines.emplace_back("CHOOSE ONE CHAIN FOR THE ENTIRE RUN");
        for(std::size_t i=0;i<sim::kFirstPlayableChainCount;++i)
            lines.push_back(std::to_string(i+1)+" "+hudChainLabel(h.chains[i].chain)+" COST "+std::to_string(h.chains[i].buildCost));
        lines.emplace_back("1 - CONQUER THE CITY");
        lines.emplace_back("2 - BUILD INTO THE SKY");
        lines.emplace_back("3 - ANNIHILATE TWO FOREIGN WORLDS");
        lines.emplace_back("4 - SURVIVE AION FOR 30 SECONDS");
        lines.emplace_back("START WITH CASH FOR FIVE STORES");
        lines.emplace_back("CLICK A LOT TWICE TO BUILD");
        lines.emplace_back("WASD MOVE / WHEEL ZOOM / P PAUSE");
        return lines;
    }
    if(h.phase==sim::GamePhase::Result) {
        lines.push_back(h.outcome==sim::MatchOutcome::Win ? "VICTORY - AION HAS WITHDRAWN" : "DEFEAT");
        if(h.endReason==sim::MatchEndReason::AllStoresLost) lines.emplace_back("NO STORE REMAINS IN AN ACTIVE WORLD");
        if(h.endReason==sim::MatchEndReason::NoCapital) lines.emplace_back("NO STORES AND NO REBUILDING CAPITAL");
        lines.push_back("REACHED PHASE "+std::to_string(c.reachedPhase));
        lines.push_back("SURVIVED "+seconds(c.elapsedTicks,h.ticksPerSecond));
        lines.push_back("FOREIGN WORLDS DESTROYED "+std::to_string(c.destroyedForeign)+"/2");
        lines.push_back("HIGHEST STACK "+std::to_string(c.highestStack)+"/256");
        lines.push_back("SURVIVING STORES "+std::to_string(h.storeCount));
        lines.push_back("MAXVALUE EVENTS "+std::to_string(c.maxValueEvents));
        lines.push_back("TIME PASTE EVENTS "+std::to_string(c.timePasteEvents));
        if(c.maxValueEvents) lines.emplace_back("MAXVALUE 9223372036854775807");
        lines.emplace_back("R - PLAY AGAIN");
        return lines;
    }
    const char* phase=h.phase==sim::GamePhase::Phase1 ? "PHASE 1 - CITY CONQUEST" :
        h.phase==sim::GamePhase::Phase2 ? "PHASE 2 - VERTICAL INVASION" :
        h.phase==sim::GamePhase::Phase3 ? "PHASE 3 - MULTIVERSE" :
        h.phase==sim::GamePhase::BossWarning ? "PHASE 4 - AION APPROACHING" : "PHASE 4 - SURVIVE AION";
    lines.emplace_back(input.isPaused ? "PAUSED" : phase);
    if(h.phase!=sim::GamePhase::Phase3)
        lines.push_back("TIME "+seconds(h.phase==sim::GamePhase::Phase1?h.ticksRemaining:c.phaseRemaining,h.ticksPerSecond));
    lines.push_back(hudChainLabel(*h.playerChain)+" CASH "+std::to_string(h.cashCredits));
    lines.push_back("LEVEL "+std::to_string(c.skills.level)+" XP "+std::to_string(c.skills.experience)+"/"+std::to_string(c.nextExperience));
    if(c.skills.snacksEndTick>c.elapsedTicks) lines.emplace_back("HOT SNACKS - ATTRACTION BOOST");
    if(c.skills.waveEndTick>c.elapsedTicks) lines.emplace_back("MIND WAVE - CUSTOMERS CAPTURED");
    if(c.skills.coffeePayout && c.skills.coffeeAtTick+20>c.elapsedTicks)
        lines.push_back("COFFEE PAYOUT +"+std::to_string(c.skills.coffeePayout));
    lines.push_back("STORES "+std::to_string(h.storeCount)+" INCOME "+std::to_string(h.predictedEconomyIncomeCredits));
    if(h.phase==sim::GamePhase::Phase1) lines.emplace_back("DOMINATE OR SURVIVE THE TIMER TO ADVANCE");
    if(h.phase==sim::GamePhase::Phase2) lines.push_back("TOWER "+std::to_string(c.highestStack)+"/256 OR SURVIVE TIMER");
    if(c.reachedPhase>=2) {
        lines.push_back("FLOOR "+std::to_string(input.selectedFloor+1)+"/"+std::to_string(c.slots));
        lines.push_back("IMAGE C "+std::to_string(c.imageCost)+" CD "+seconds(c.imageCooldown,h.ticksPerSecond));
    }
    if(input.selectedFacility) {
        lines.push_back("BUILD "+std::to_string(input.selectedBuildCostCredits)+" - CLICK AGAIN");
        if(input.selectedStoreChain) {
            lines.push_back(hudChainLabel(*input.selectedStoreChain)+" FAITH "+std::to_string(input.selectedFaith)+"/"+std::to_string(c.faithMax));
            if(input.selectedAntiStore) lines.emplace_back("ANTI STORE - MATCH ITS ORIGIN ANCHOR");
            if(*input.selectedStoreChain==sim::ChainId::Aion) {
                constexpr std::array<const char*,7> forms{"AION","SHIKAKU ETSU","KASUMI","SHIGENARI","DAIEI","MANY STOP","MAI BISUKETTO"};
                lines.emplace_back(forms[input.selectedBossForm%forms.size()]);
            }
        }
    } else lines.emplace_back("CLICK A LOT TO SELECT");
    if(c.reachedPhase>=3) {
        lines.push_back("FOREIGN WORLDS "+std::to_string(c.destroyedForeign)+"/2 - VIEW D"+std::to_string(c.visibleDimension));
        for(const auto& d:c.dimensions) {
            if(!d.active && d.id!=c.visibleDimension) continue;
            lines.push_back("D"+std::to_string(d.id)+(d.active?" OWN ":" COLLAPSED ")+std::to_string(d.playerStores)+
                " RIVALS "+std::to_string(d.rivalStores)+" AION "+std::to_string(d.bossStores));
            if(d.maxValueRemaining) lines.push_back("WARNING D"+std::to_string(d.id)+" MAXVALUE "+seconds(d.maxValueRemaining,h.ticksPerSecond));
            if(d.energyRemaining) lines.push_back("D"+std::to_string(d.id)+" CONVENIENCE "+seconds(d.energyRemaining,h.ticksPerSecond));
        }
        lines.push_back("INVERT I "+std::to_string(c.inversionCost)+" CD "+seconds(c.inversionCooldown,h.ticksPerSecond));
        if(h.phase==sim::GamePhase::Phase3) lines.emplace_back("INVERT AND MATCH D0 / OR ELIMINATE ALL RIVALS");
    }
    if(c.reachedPhase==4 && c.maxValueEvents) lines.emplace_back("MAXVALUE 9223372036854775807");
    if(c.reachedPhase==4) lines.push_back("ESCAPE X "+std::to_string(c.escapeCost+h.chains[sim::chainIndex(*h.playerChain)].buildCost)+
        " CD "+seconds(c.escapeCooldown,h.ticksPerSecond));
    if(h.threatenedPlayerStores) lines.push_back("ENCIRCLED STORES "+std::to_string(h.threatenedPlayerStores));
    if(input.lastCampaignFailure) lines.push_back(failure(*input.lastCampaignFailure));
    if(input.lastPlacementFailure && *input.lastPlacementFailure!=sim::PlacementFailure::None)
        lines.emplace_back(hudPlacementFailureText(*input.lastPlacementFailure));
    if(input.showControls) {
        for(std::size_t i=0;i<sim::kSkillCount;++i) if(c.skills.ranks[i])
            lines.push_back(skillLabel(static_cast<sim::SkillId>(i))+" LV "+std::to_string(c.skills.ranks[i]));
        if(c.reachedPhase>=2) lines.emplace_back("Q/E FLOOR / F NEXT FREE / G GROUND");
        if(c.reachedPhase>=2) lines.emplace_back("HOLD SPACE BUILD / C IMAGE STRATEGY");
        if(c.reachedPhase>=3) lines.emplace_back("TAB WORLD / I INVERT MAX FAITH RIVAL");
        lines.emplace_back("P PAUSE / F1 HELP / ESC CANCEL");
    }
    return lines;
}
}  // namespace konbini::app
