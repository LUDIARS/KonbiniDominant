#pragma once

#include <cstdint>
#include <optional>

#include "konbini/sim/chain_economy_table.h"
#include "konbini/sim/facility_table.h"
#include "konbini/sim/game_state.h"
#include "konbini/sim/player_command.h"
#include "konbini/sim/store_table.h"

// @implements spec/feature/phase-1-dominant-triangle.md 店舗配置
// @implements spec/feature/economy-and-population.md 建設費

namespace konbini::sim {

enum class PlacementFailure : std::uint8_t {
    None = 0,
    WrongPhase,
    NoPlayerChain,
    InvalidChain,
    WrongChain,
    UnsupportedVerticalSlot,
    FacilityNotFound,
    FacilityProtected,
    FacilityUnavailable,
    FacilityOccupied,
    InsufficientCash,
    MissingSupport,
    DimensionCollapsed,
};

struct PlacementDecision {
    PlacementFailure failure = PlacementFailure::None;
    std::int64_t buildCostCredits = 0;
    double zocRadiusMeters = 0.0;
};

struct PlacementResult {
    PlaceStoreCommand command{};
    PlacementFailure failure = PlacementFailure::None;
    std::optional<StoreId> placedStore;
    bool replacedIntactFacility = false;
};

[[nodiscard]] PlacementDecision validatePlacement(
    const PlaceStoreCommand& command, const GameState& state,
    const FacilityTable& facilities, const StoreTable& stores,
    const ChainEconomyTable& economy);

[[nodiscard]] PlacementResult commitPlacementAtomically(
    const PlaceStoreCommand& command, const GameState& state,
    FacilityTable& facilities, StoreTable& stores,
    ChainEconomyTable& economy, GenerationalIdPool<StoreId>& storeIds);

}  // namespace konbini::sim
