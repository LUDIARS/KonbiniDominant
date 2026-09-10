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
#include "konbini/sim/encirclement.h"
#include "konbini/sim/campaign_hud.h"

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
    bool isLotRepresentation = false;
    bool allowsStacking = false;
};

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
struct RenderStore {
    StoreId id{};
    FacilityId facilityId{};
    ChainId chain = ChainId::Losan;
    Vec3 positionMeters{};
    double zocRadiusMeters = 0.0;
    std::uint64_t capturedPopulation = 0;
    std::uint32_t revenuePermille = 1000;
    bool isEncircled = false;
    std::uint32_t captureTicksRemaining = 0, captureDelayTotal = 0;
    std::uint32_t verticalSlot = 0, faith = 0;
    bool isAntiStore = false;
    std::uint32_t dimension = 0;
};

struct RenderPopulationCell {
    Vec3 positionMeters{};
    std::optional<ChainId> owner;
    std::uint32_t population = 0;
};

struct ChainMatchView {
    ChainId chain = ChainId::Losan;
    std::uint32_t stores = 0;
    std::uint64_t customers = 0;
    std::uint32_t triangles = 0;
    std::int64_t buildCost = 0;
};

// @implements spec/feature/ui-ux.md Common HUD
struct HudViewModel {
    std::uint64_t completedTicks = 0;
    std::optional<ChainId> playerChain;
    std::int64_t cashCredits = 0;
    std::uint32_t storeCount = 0;
    std::uint32_t ticksUntilEconomy = 0;
    std::int64_t predictedEconomyIncomeCredits = 0;
    bool competitive = false;
    GamePhase phase = GamePhase::ChainSelect;
    MatchOutcome outcome = MatchOutcome::None;
    MatchEndReason endReason = MatchEndReason::None;
    std::uint64_t phaseTicks = 0;
    std::uint64_t ticksRemaining = 0;
    std::uint32_t ticksPerSecond = 10;
    std::uint32_t dominationPercent = 0;
    std::uint64_t totalPopulation = 0;
    std::uint32_t threatenedPlayerStores = 0;
    std::uint32_t destroyedRivalStores = 0;
    std::uint32_t aiPeriodTicks = 0;
    std::uint32_t captureDelayTicks = 0;
    std::array<ChainMatchView, kFirstPlayableChainCount> chains{};
    CampaignHud campaign;
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
    [[nodiscard]] std::span<const DominantTriangle> triangles() const noexcept;
    [[nodiscard]] std::span<const RenderPopulationCell> populationCells() const noexcept;

private:
    friend std::shared_ptr<const RenderSnapshot> makeRenderSnapshot(
        const GameState&, const FirstPlayableContent&, const FacilityTable&,
        const StoreTable&, const PopulationCellTable&, const ChainEconomyTable&,
        std::span<const RenderStorePlacementCue>, std::span<const DominantTriangle>,
        std::span<const Encirclement>);

    std::uint64_t completedTicks_ = 0;
    std::vector<RenderFacility> facilities_;
    std::vector<RenderStore> stores_;
    std::vector<ResidentPresentation> residents_;
    std::vector<RenderStorePlacementCue> placementCues_;
    HudViewModel hud_;
    std::vector<DominantTriangle> triangles_;
    std::vector<RenderPopulationCell> populationCells_;
};

[[nodiscard]] std::shared_ptr<const RenderSnapshot> makeRenderSnapshot(
    const GameState& state, const FirstPlayableContent& content,
    const FacilityTable& facilities, const StoreTable& stores,
    const PopulationCellTable& populationCells,
    const ChainEconomyTable& economy,
    std::span<const RenderStorePlacementCue> placementCues,
    std::span<const DominantTriangle> triangles = {},
    std::span<const Encirclement> encirclements = {});

}  // namespace konbini::sim
