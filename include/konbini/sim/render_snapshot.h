#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/facility_table.h"
#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/game_state.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/resident_presentation.h"
#include "konbini/sim/render_store_placement_cue.h"
#include "konbini/sim/store_table.h"

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
// @implements spec/interface/pictor-rendering.md Ownership

namespace konbini::sim {

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
struct RenderFacility {
    FacilityId id{};
    FigmentumFacilityKey figmentumKey{};
    Vec3 positionMeters{};
    Bounds3 boundsMeters{};
    FacilityState state = FacilityState::Intact;
    bool isBuildable = false;
};

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
struct RenderStore {
    StoreId id{};
    FacilityId facilityId{};
    ChainId chain = ChainId::Losan;
    Vec3 positionMeters{};
    double zocRadiusMeters = 0.0;
    std::uint64_t capturedPopulation = 0;
};

// @implements spec/feature/ui-ux.md Common HUD
struct HudViewModel {
    std::uint64_t completedTicks = 0;
    std::optional<ChainId> playerChain;
    std::int64_t cashCredits = 0;
    std::uint32_t storeCount = 0;
    std::uint32_t ticksUntilEconomy = 0;
    std::int64_t predictedEconomyIncomeCredits = 0;
};

// 生成後は不変。simulation 側の table を参照で抱えず値で保持するので、
// render thread が保持し続けても tick 側の書き換えと競合しない。
// @implements spec/interface/pictor-rendering.md Ownership
class RenderSnapshot {
public:
    [[nodiscard]] std::uint64_t completedTicks() const noexcept;
    [[nodiscard]] std::span<const RenderFacility> facilities() const noexcept;
    [[nodiscard]] std::span<const RenderStore> stores() const noexcept;
    [[nodiscard]] std::span<const ResidentPresentation> residents() const noexcept;
    [[nodiscard]] std::span<const RenderStorePlacementCue>
    placementCues() const noexcept;
    [[nodiscard]] const HudViewModel& hud() const noexcept;

private:
    friend std::shared_ptr<const RenderSnapshot> makeRenderSnapshot(
        const GameState&, const FirstPlayableContent&, const FacilityTable&,
        const StoreTable&, const PopulationCellTable&, const ChainEconomyTable&,
        std::span<const RenderStorePlacementCue>);

    std::uint64_t completedTicks_ = 0;
    std::vector<RenderFacility> facilities_;
    std::vector<RenderStore> stores_;
    std::vector<ResidentPresentation> residents_;
    std::vector<RenderStorePlacementCue> placementCues_;
    HudViewModel hud_;
};

[[nodiscard]] std::shared_ptr<const RenderSnapshot> makeRenderSnapshot(
    const GameState& state, const FirstPlayableContent& content,
    const FacilityTable& facilities, const StoreTable& stores,
    const PopulationCellTable& populationCells,
    const ChainEconomyTable& economy,
    std::span<const RenderStorePlacementCue> placementCues);

}  // namespace konbini::sim
