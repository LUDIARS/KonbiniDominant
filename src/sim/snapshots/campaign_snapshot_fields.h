#pragma once
#include "konbini/sim/game_state.h"
#include "konbini/sim/campaign_content.h"
#include "skill_snapshot_fields.h"
namespace konbini::sim {
// Kept separate from the historical v2/v3 wire format.
template<class Writer>
void writeCampaignFields(Writer& out,const GameState& state,const CampaignContent& content) {
    const auto& s=state.campaign; const auto& v=content.vertical;
    const auto& d=content.dimensions; const auto& a=content.aion;
    out.u8(s.enabled?1:0);
    writeSkillFields(out,s.skills,content.skills);
    for(const auto value:{s.elapsedTicks,s.imageReadyTick,s.inversionReadyTick,s.escapeReadyTick,
        s.nextBossSpawnTick,s.nextTimePasteTick}) out.u64(value);
    // visibleDimension is presentation-only navigation. It must not change
    // deterministic replay identity or the authoritative save payload.
    for(const auto value:{s.nextDimension,s.destroyedForeign,s.highestStack,
        s.reachedPhase,s.timePasteEvents,s.maxValueEvents,state.destroyedStores[chainIndex(ChainId::Aion)]}) out.u32(value);
    out.u64(s.dimensions.size());
    for(const auto& dim:s.dimensions) {
        out.u32(dim.id); out.u64(dim.seed); out.u8(static_cast<std::uint8_t>(dim.status));
        out.u8(dim.foreignObjective?1:0); out.u64(dim.energyEndTick); out.u64(dim.maxValueAtTick);
        for(const auto store:dim.maxValueTriangle) out.entity(store.value());
    }
    out.u64(s.history.size());
    for(const auto& frame:s.history) {
        out.u64(frame.tick); out.u64(frame.stores.size());
        for(const auto& store:frame.stores) {
            out.u32(store.dimension); out.u32(store.verticalSlot); out.u64(store.anchor);
        }
    }
    for(const auto value:{v.durationTicks,v.slots,v.costStepPermille,v.faithMax,v.startingFaith,
        v.faithGrowth,v.faithNeighborLimit,v.triangleFaith,v.heightFaithLimit,v.imageCost,v.imageCooldownTicks,v.imageFaith,
        d.foreignCount,d.seedVersion,d.initialRivalStores,d.inversionCost,d.inversionCooldownTicks,d.energyDurationTicks,
        d.energyRevenuePermille,d.energyFaith,d.maxActive,d.escapeCost,d.escapeCooldownTicks,
        a.warningTicks,a.manifestationTicks,a.spawnPeriodTicks,a.maxValueWarningTicks,a.budgetCredits,
        a.buildCostCredits,a.timePeriodTicks,a.historyTicks,a.pasteLotOffset,a.pasteLimit}) out.u32(value);
    out.f64(v.floorHeightMeters); out.f64(a.zocRadiusMeters);
}
}
