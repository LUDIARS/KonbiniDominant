#include "konbini/sim/render_snapshot.h"

#include <limits>
#include <stdexcept>
#include <utility>

#include "konbini/sim/revenue_math.h"

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::sim {

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::uint64_t RenderSnapshot::completedTicks() const noexcept {
    return completedTicks_;
}

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::span<const RenderFacility> RenderSnapshot::facilities() const noexcept {
    return facilities_;
}

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::span<const RenderStore> RenderSnapshot::stores() const noexcept {
    return stores_;
}

// @implements spec/feature/ui-ux.md Common HUD
const HudViewModel& RenderSnapshot::hud() const noexcept {
    return hud_;
}

// tick 終端で publish する immutable snapshot を組み立てる。render thread は
// これを読むだけなので、simulation table への参照は一切残さず値で写し取る。
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::shared_ptr<const RenderSnapshot> makeRenderSnapshot(
    const GameState& state, const FirstPlayableContent& content,
    const FacilityTable& facilities, const StoreTable& stores,
    const ChainEconomyTable& economy) {
    if (content.simulation.economyPeriodTicks == 0) {
        throw std::invalid_argument("economy period must be positive");
    }
    auto snapshot = std::make_shared<RenderSnapshot>();
    snapshot->completedTicks_ = state.completedTicks;
    snapshot->facilities_.reserve(facilities.size());
    for (std::size_t index = 0; index < facilities.size(); ++index) {
        const FacilityRow row = facilities.row(index);
        snapshot->facilities_.push_back({
            .id = row.id,
            .figmentumKey = row.figmentumKey,
            .positionMeters = row.positionMeters,
            .boundsMeters = row.boundsMeters,
            .state = row.state,
            .isBuildable = row.isBuildable,
        });
    }
    snapshot->stores_.reserve(stores.size());
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const StoreRow row = stores.row(index);
        if (!row.isActive) {
            continue;
        }
        snapshot->stores_.push_back({
            .id = row.id,
            .facilityId = row.facilityId,
            .chain = row.chain,
            .positionMeters = row.positionMeters,
            .zocRadiusMeters = row.zocRadiusMeters,
            .capturedPopulation = row.capturedPopulation,
        });
    }
    snapshot->hud_.completedTicks = state.completedTicks;
    snapshot->hud_.playerChain = state.playerChain;
    const std::uint32_t remainder = static_cast<std::uint32_t>(
        state.completedTicks % content.simulation.economyPeriodTicks);
    snapshot->hud_.ticksUntilEconomy =
        remainder == 0 ? content.simulation.economyPeriodTicks
                       : content.simulation.economyPeriodTicks - remainder;
    if (state.playerChain.has_value()) {
        const ChainEconomyRow row = economy.row(*state.playerChain);
        snapshot->hud_.cashCredits = row.cashCredits;
        snapshot->hud_.storeCount = row.storeCount;
        // 予測値は economy が実際に適用する rule から引く。`content` は同一
        // instance とは限らず、別 content から引くと HUD の予測と tick 決算が
        // 無言で食い違う。
        const ChainContent& rules = economy.rules(*state.playerChain);
        for (const RenderStore& store : snapshot->stores_) {
            if (store.chain != *state.playerChain) {
                continue;
            }
            const std::int64_t revenue = calculateRevenueCredits(
                store.capturedPopulation,
                rules.revenueMilliCreditsPerPerson);
            if (snapshot->hud_.predictedEconomyIncomeCredits >
                std::numeric_limits<std::int64_t>::max() - revenue) {
                throw std::overflow_error(
                    "predicted player economy income overflow");
            }
            snapshot->hud_.predictedEconomyIncomeCredits += revenue;
        }
    }
    return std::shared_ptr<const RenderSnapshot>(std::move(snapshot));
}

}  // namespace konbini::sim
