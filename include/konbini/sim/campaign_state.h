#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "konbini/sim/entity_id.h"
#include "konbini/sim/skill_state.h"
namespace konbini::sim {
enum class DimensionStatus : std::uint8_t { Active, Destroyed };
struct DimensionRecord {
    std::uint32_t id=0;
    std::uint64_t seed=0;
    DimensionStatus status=DimensionStatus::Active;
    bool foreignObjective=false;
    std::uint64_t energyEndTick=0, maxValueAtTick=0;
    std::array<StoreId,3> maxValueTriangle{};
};
struct AionHistoryStore {
    std::uint32_t dimension=0, verticalSlot=0;
    std::uint64_t anchor=0;
};
struct AionHistoryFrame {
    std::uint64_t tick=0;
    std::vector<AionHistoryStore> stores;
};
struct CampaignState {
    bool enabled=false;
    SkillState skills;
    std::uint64_t elapsedTicks=0, imageReadyTick=0, inversionReadyTick=0, escapeReadyTick=0;
    std::uint32_t visibleDimension=0, nextDimension=1, destroyedForeign=0, highestStack=0;
    std::uint32_t reachedPhase=1, timePasteEvents=0, maxValueEvents=0;
    std::uint64_t nextBossSpawnTick=0, nextTimePasteTick=0;
    std::vector<DimensionRecord> dimensions;
    std::vector<AionHistoryFrame> history;
};
inline bool dimensionActive(const CampaignState& state,const std::uint32_t id) noexcept {
    if(!state.enabled) return id==0;
    for(const auto& d:state.dimensions) if(d.id==id) return d.status==DimensionStatus::Active;
    return false;
}
}
