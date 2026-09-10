#include "konbini/app/playtest_pilot.h"
#include <algorithm>
#include <array>
namespace konbini::app {
std::uint64_t PlaytestPilot::Context::anchor(sim::FacilityId id) const {
    for(const auto& f:snapshot.facilities()) if(f.id==id) return f.figmentumKey.value();
    return 0;
}
bool PlaytestPilot::Context::occupied(sim::FacilityId id,std::uint32_t floor) const {
    return std::ranges::any_of(snapshot.stores(),[&](const auto& s){return s.facilityId==id && s.verticalSlot==floor;});
}
std::optional<sim::PlayerCommand> PlaytestPilot::next(const sim::RenderSnapshot& snapshot,CommandComposer& commands) {
    const auto& hud=snapshot.hud();activeNode_="idle";
    if(hud.phase!=sim::GamePhase::ChainSelect && (!sim::isPlayingPhase(hud.phase) || !hud.playerChain)) return {};
    const Context context{snapshot,commands};
    if(hud.playerChain && !hud.campaign.skills.pending && hud.campaign.visibleDimension==0) {
        originStores_.clear();
        for(const auto& store:snapshot.stores()) if(store.chain==*hud.playerChain)
            originStores_.emplace(context.anchor(store.facilityId),store.verticalSlot);
    }
    struct Child {std::string_view name;Decision (PlaytestPilot::*tick)(const Context&);};
    constexpr std::array children{
        Child{"select_chain",&PlaytestPilot::selectChain},Child{"choose_skill",&PlaytestPilot::chooseSkill},
        Child{"escape_boss",&PlaytestPilot::escapeBoss},Child{"conquer_foreign",&PlaytestPilot::conquerForeign},
        Child{"expand_stores",&PlaytestPilot::expandStores}};
    auto result=bt::selector(children,[&](const Child& child) {
        auto decision=(this->*child.tick)(context);
        if(decision.status!=bt::Status::Failure) activeNode_=child.name;
        return decision;
    });
    return std::move(result.command);
}
}
