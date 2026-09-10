#include "konbini/sim/campaign_command_system.h"
#include "konbini/sim/vertical_placement.h"
#include "konbini/sim/skill_system.h"
#include "konbini/sim/population_change_system.h"
#include <algorithm>
#include <stdexcept>
namespace konbini::sim {
CampaignFailure applyCampaignCommand(CampaignWorld w,const CampaignCommand& command) {
    if(!w.content.campaign || !isPlayingPhase(w.state.phase)) return CampaignFailure::NotUnlocked;
    if(!w.state.playerChain || command.chain!=*w.state.playerChain ||
       command.order.sourcePriority!=CommandSourcePriority::Player) return CampaignFailure::WrongChain;
    auto& state=w.state.campaign; const auto& rules=*w.content.campaign;
    if(state.skills.pending && command.action!=CampaignAction::ChooseSkill)
        return CampaignFailure::ChooseUpgradeFirst;
    switch(command.action) {
    case CampaignAction::ChooseSkill:
        return chooseSkill(w,command.dimension) ? CampaignFailure::None : CampaignFailure::InvalidUpgrade;
    case CampaignAction::ImageStrategy:
        if(!verticalUnlocked(w.state)) return CampaignFailure::NotUnlocked;
        if(state.elapsedTicks<state.imageReadyTick) return CampaignFailure::Cooldown;
        if(!w.economy.spend(command.chain,rules.vertical.imageCost)) return CampaignFailure::InsufficientCash;
        for(std::size_t i=0;i<w.stores.size();++i) {
            const auto store=w.stores.row(i);
            if(store.isActive && store.chain==command.chain)
                w.stores.setFaith(i,std::min(rules.vertical.faithMax,store.faith+rules.vertical.imageFaith));
        }
        state.imageReadyTick=state.elapsedTicks+rules.vertical.imageCooldownTicks;
        return CampaignFailure::None;
    case CampaignAction::ViewDimension:
        if(state.reachedPhase<3) return CampaignFailure::NotUnlocked;
        if(!dimensionActive(state,command.dimension)) return CampaignFailure::Collapsed;
        state.visibleDimension=command.dimension; return CampaignFailure::None;
    case CampaignAction::InvertStore: {
        if(state.reachedPhase<3) return CampaignFailure::NotUnlocked;
        const auto index=w.stores.findAt(command.facilityId,command.verticalSlot);
        if(!index) return CampaignFailure::NoTarget;
        const auto store=w.stores.row(*index);
        if(!dimensionActive(state,store.dimension)) return CampaignFailure::Collapsed;
        if(store.dimension==0 || store.chain==command.chain || store.chain==ChainId::Aion) return CampaignFailure::NotRival;
        if(store.isAntiStore) return CampaignFailure::AlreadyInverted;
        if(store.faith!=rules.vertical.faithMax) return CampaignFailure::FaithRequired;
        if(state.elapsedTicks<state.inversionReadyTick) return CampaignFailure::Cooldown;
        if(!w.economy.spend(command.chain,rules.dimensions.inversionCost)) return CampaignFailure::InsufficientCash;
        w.stores.markAntiStore(*index);
        state.inversionReadyTick=state.elapsedTicks+rules.dimensions.inversionCooldownTicks;
        return CampaignFailure::None;
    }
    case CampaignAction::Escape: {
        if(w.state.phase!=GamePhase::Boss && w.state.phase!=GamePhase::BossWarning) return CampaignFailure::NotUnlocked;
        if(state.elapsedTicks<state.escapeReadyTick) return CampaignFailure::Cooldown;
        const auto active=std::ranges::count_if(state.dimensions,[](const auto& d){return d.status==DimensionStatus::Active;});
        if(active>=rules.dimensions.maxActive) return CampaignFailure::DimensionCapacity;
        const std::int64_t cost=static_cast<std::int64_t>(rules.dimensions.escapeCost)+skillBuildCost(w.economy.rules(command.chain).buildCostCredits,
            w.state,rules.skills,command.chain);
        if(!w.economy.canAfford(command.chain,cost)) return CampaignFailure::InsufficientCash;
        std::optional<std::uint64_t> anchor;
        const auto selected=w.facilities.find(command.facilityId);
        if(selected && w.facilities.row(*selected).isBuildable) anchor=w.facilities.row(*selected).figmentumKey.value();
        if(!anchor) for(std::size_t i=0;i<w.facilities.size();++i) {
            const auto f=w.facilities.row(i);
            if(f.dimension==0 && f.isBuildable) { anchor=f.figmentumKey.value(); break; }
        }
        if(!anchor) return CampaignFailure::NoTarget;
        const auto dimension=createCampaignDimension(w,false);
        const auto target=anchorFacility(w.facilities,dimension,*anchor);
        if(!target) throw std::logic_error("new dimension lacks its origin anchor");
        const auto result=commitPlacementAtomically({command.order,command.chain,*target,0},
            w.state,w.facilities,w.stores,w.economy,w.identities.stores());
        if(result.failure!=PlacementFailure::None || !w.economy.spend(command.chain,rules.dimensions.escapeCost))
            throw std::logic_error("validated escape transaction failed");
        applyPopulationLoss(w.population,*target,w.content.phase1->destructionPopulationLossPercent);
        state.visibleDimension=dimension;
        state.escapeReadyTick=state.elapsedTicks+rules.dimensions.escapeCooldownTicks;
        return CampaignFailure::None;
    }
    }
    return CampaignFailure::NotUnlocked;
}
}
