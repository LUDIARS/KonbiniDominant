#include "konbini/sim/canonical_snapshot.h"
#include "campaign_snapshot_fields.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

// @implements spec/plan/tasks/first-playable.md Simulation

namespace konbini::sim {

namespace {

class FieldWriter {
public:
    void u8(const std::uint8_t value) {
        bytes_.push_back(static_cast<std::byte>(value));
    }

    void u32(const std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) {
            u8(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void u64(const std::uint64_t value) {
        for (unsigned shift = 0; shift < 64; shift += 8) {
            u8(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void i64(const std::int64_t value) {
        u64(std::bit_cast<std::uint64_t>(value));
    }

    void f64(const double value) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                "canonical snapshot cannot contain non-finite numbers");
        }
        u64(std::bit_cast<std::uint64_t>(value));
    }

    void entity(const EntityId id) {
        u32(id.index);
        u32(id.generation);
    }

    void text(const std::string_view value) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::overflow_error("canonical string is too large");
        }
        u32(static_cast<std::uint32_t>(value.size()));
        for (const char byte : value) {
            u8(static_cast<std::uint8_t>(byte));
        }
    }

    [[nodiscard]] std::vector<std::byte> finish() && {
        return std::move(bytes_);
    }

private:
    std::vector<std::byte> bytes_;
};

void writePosition(FieldWriter& writer, const Vec3 position) {
    writer.f64(position.x);
    writer.f64(position.y);
    writer.f64(position.z);
}

void writeBounds(FieldWriter& writer, const Bounds3& bounds) {
    writePosition(writer, bounds.min);
    writePosition(writer, bounds.max);
}

std::uint64_t hashBytes(const std::vector<std::byte>& bytes) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint8_t>(value);
        hash *= 1099511628211ull;
    }
    return hash;
}

}  // namespace

// tick 境界の state を byte 列へ確定させる唯一の入口。table の dense 順は
// 挿入履歴に依存するので、書き出す前に stable な ID 順へ並べ替える。
// @implements spec/plan/tasks/first-playable.md Simulation
// @implements spec/design.md 5. Tick と決定性
CanonicalSnapshot makeCanonicalSnapshot(
    const GameState& state, const FirstPlayableContent& content,
    const FacilityTable& facilities, const StoreTable& stores,
    const PopulationCellTable& populationCells,
    const ChainEconomyTable& economy, const std::span<const Encirclement> encirclements) {
    FieldWriter writer;
    writer.text("KonbiniDominantCanonicalSnapshot");
    const std::uint32_t schemaVersion = content.campaign
        ? kCanonicalSnapshotSchemaVersion
        : content.phase1 ? kPhase1CanonicalSnapshotSchemaVersion
                         : kLegacyCanonicalSnapshotSchemaVersion;
    writer.u32(schemaVersion);
    writer.u32(content.schemaVersion);
    writer.u32(content.contentVersion);
    writer.u64(state.completedTicks);
    writer.u8(static_cast<std::uint8_t>(state.phase));
    writer.u8(state.playerChain.has_value() ? 1U : 0U);
    writer.u8(state.playerChain.has_value()
                  ? static_cast<std::uint8_t>(*state.playerChain)
                  : 0U);
    writer.u64(state.worldSeed);
    if (content.phase1) {
        writer.u8(state.competitive ? 1 : 0);
        writer.u64(state.phaseTicks);
        writer.u8(static_cast<std::uint8_t>(state.outcome));
        writer.u8(static_cast<std::uint8_t>(state.endReason));
        for (std::size_t i = 0; i < kFirstPlayableChainCount; ++i) {
            writer.u8(state.hasOpened[i] ? 1 : 0);
            writer.u64(state.nextAiTick[i]);
            writer.u32(state.destroyedStores[i]);
        }
        // Tunable gameplay values participate in the hash, so using a different
        // balance file cannot masquerade as the same deterministic match.
        const auto& rules = *content.phase1;
        writer.u32(rules.durationTicks);
        writer.u32(rules.captureDelayTicks);
        writer.u32(rules.dominationPercent);
        writer.u32(rules.aiPeriodTicks);
        writer.u32(rules.aiOpeningPeriodTicks);
        writer.u32(rules.aiRampTicks);
        writer.u32(rules.aiTriangleScore);
        writer.u32(rules.aiEncirclementScore);
        writer.u32(rules.aiExposurePenalty);
        writer.f64(rules.triangleMaxEdgeMeters);
        writer.f64(rules.triangleMinAreaSquareMeters);
        writer.u32(rules.triangleInfluence);
        writer.u32(rules.triangleRevenuePermille);
        writer.u32(rules.destructionPopulationLossPercent);
        writer.u32(rules.populationRecoveryPerPeriod);
        writer.u32(content.simulation.ticksPerSecond);
        writer.u32(content.simulation.economyPeriodTicks);
        writer.u32(content.population.basePopulation);
        writer.u32(content.population.randomPopulationCount);
        writer.u32(content.startingStoreEquivalent);
        for (const auto& chain : content.chains) {
            writer.i64(chain.buildCostCredits);
            writer.f64(chain.zocRadiusMeters);
            writer.i64(chain.revenueMilliCreditsPerPerson);
        }
    }

    if (content.campaign) { writeCampaignFields(writer, state, *content.campaign); }
    std::vector<FacilityRow> facilityRows;
    facilityRows.reserve(facilities.size());
    for (std::size_t index = 0; index < facilities.size(); ++index) {
        facilityRows.push_back(facilities.row(index));
    }
    std::sort(facilityRows.begin(),
              facilityRows.end(),
              [](const FacilityRow& left, const FacilityRow& right) {
                  if (left.figmentumKey != right.figmentumKey) {
                      return right.figmentumKey > left.figmentumKey;
                  }
                  return right.id.value() > left.id.value();
              });
    writer.u64(facilityRows.size());
    for (const FacilityRow& row : facilityRows) {
        writer.entity(row.id.value());
        writer.u64(row.figmentumKey.value());
        writer.u32(row.dimension);
        writePosition(writer, row.positionMeters);
        writeBounds(writer, row.boundsMeters);
        writer.u8(row.isBuildable ? 1U : 0U);
        writer.u8(static_cast<std::uint8_t>(row.state));
    }

    std::vector<StoreRow> storeRows;
    storeRows.reserve(stores.size());
    for (std::size_t index = 0; index < stores.size(); ++index) {
        storeRows.push_back(stores.row(index));
    }
    std::sort(storeRows.begin(),
              storeRows.end(),
              [](const StoreRow& left, const StoreRow& right) {
                  return right.id.value() > left.id.value();
              });
    writer.u64(storeRows.size());
    for (const StoreRow& row : storeRows) {
        writer.entity(row.id.value());
        writer.entity(row.facilityId.value());
        writer.u8(static_cast<std::uint8_t>(row.chain));
        writer.u32(row.dimension);
        writePosition(writer, row.positionMeters);
        writer.f64(row.zocRadiusMeters);
        writer.u64(row.capturedPopulation);
        writer.u8(row.isActive ? 1U : 0U);
        if (content.phase1) { writer.u32(row.revenuePermille); }
        if (content.campaign) { writer.u32(row.verticalSlot); writer.u32(row.faith); writer.u8(row.isAntiStore ? 1 : 0); }
    }

    std::vector<PopulationCellRow> populationRows;
    populationRows.reserve(populationCells.size());
    for (std::size_t index = 0; index < populationCells.size(); ++index) {
        populationRows.push_back(populationCells.row(index));
    }
    std::sort(populationRows.begin(),
              populationRows.end(),
              [](const PopulationCellRow& left,
                 const PopulationCellRow& right) {
                  return right.id.value() > left.id.value();
              });
    writer.u64(populationRows.size());
    for (const PopulationCellRow& row : populationRows) {
        writer.entity(row.id.value());
        writer.entity(row.facilityId.value());
        writer.u32(row.dimension);
        writePosition(writer, row.positionMeters);
        writer.u32(row.population);
        if (content.phase1) { writer.u32(row.capacity); }
        writer.u8(row.assignedStore.has_value() ? 1U : 0U);
        if (row.assignedStore.has_value()) {
            if (!row.preferredChain.has_value()) {
                throw std::invalid_argument(
                    "assigned population cell is missing its preferred chain");
            }
            writer.entity(row.assignedStore->value());
            writer.u8(static_cast<std::uint8_t>(*row.preferredChain));
        }
    }

    const auto chainCount=content.campaign ? kSimulationChainCount : kFirstPlayableChainCount;
    writer.u32(static_cast<std::uint32_t>(chainCount));
    for (std::size_t index = 0; index < chainCount; ++index) {
        const ChainId chain = static_cast<ChainId>(index);
        const ChainEconomyRow row = economy.row(chain);
        writer.u8(static_cast<std::uint8_t>(chain));
        writer.i64(row.cashCredits);
        writer.u32(row.storeCount);
        writer.u64(row.customerShare);
        writer.i64(row.incomeThisTick);
        writer.i64(row.expenseThisTick);
        writer.u8(row.isActive ? 1U : 0U);
    }

    if (content.phase1) {
        std::vector<Encirclement> sorted(encirclements.begin(), encirclements.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.target.value() < b.target.value();
        });
        writer.u64(sorted.size());
        for (const auto& threat : sorted) {
            writer.entity(threat.target.value());
            writer.u8(static_cast<std::uint8_t>(threat.attacker));
            for (const auto id : threat.triangleStores) { writer.entity(id.value()); }
            writer.u32(threat.elapsedTicks);
        }
    }
    std::vector<std::byte> bytes = std::move(writer).finish();
    // Hash first so the payload can be moved out instead of copied: a snapshot
    // is produced on every tick and its size grows with the world.
    const std::uint64_t hash = hashBytes(bytes);
    return {
        .schemaVersion = schemaVersion,
        .bytes = std::move(bytes),
        .hash = hash,
    };
}

}  // namespace konbini::sim
