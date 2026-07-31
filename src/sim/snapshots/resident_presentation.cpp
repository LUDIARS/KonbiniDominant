#include "konbini/sim/resident_presentation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "konbini/sim/counter_rng.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline
// @implements spec/interface/visia-presentation.md Pictor integration boundary
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`

namespace konbini::sim {

namespace {

[[nodiscard]] std::uint64_t stableEntityValue(
    const PopulationCellId id) noexcept {
    const EntityId value = id.value();
    return (static_cast<std::uint64_t>(value.generation) << 32U) |
           value.index;
}

[[nodiscard]] std::uint64_t checkedAdd(const std::uint64_t left,
                                       const std::uint64_t right,
                                       const char* const message) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error(message);
    }
    return left + right;
}

[[nodiscard]] std::uint64_t walkingTicks(
    const Vec3 home, const Vec3 store, const std::uint32_t ticksPerSecond,
    const double speedMetersPerSecond) {
    const double distance = std::hypot(store.x - home.x, store.z - home.z);
    const double ticks =
        std::ceil((distance / speedMetersPerSecond) * ticksPerSecond);
    constexpr std::uint64_t kMaximumWalkingTicks =
        std::numeric_limits<std::uint64_t>::max() / 4U;
    if (!std::isfinite(ticks) ||
        ticks > static_cast<double>(kMaximumWalkingTicks)) {
        throw std::overflow_error("resident walking duration is not representable");
    }
    return std::max<std::uint64_t>(1, static_cast<std::uint64_t>(ticks));
}

[[nodiscard]] std::uint64_t cycleTick(const std::uint64_t completedTicks,
                                      const std::uint64_t phaseOffset,
                                      const std::uint64_t cycleTicks) noexcept {
    const std::uint64_t remainder = completedTicks % cycleTicks;
    if (phaseOffset == 0) {
        return remainder;
    }
    const std::uint64_t untilWrap = cycleTicks - phaseOffset;
    if (remainder >= untilWrap) {
        return remainder - untilWrap;
    }
    return remainder + phaseOffset;
}

[[nodiscard]] Vec3 interpolateXZ(const Vec3 start, const Vec3 end,
                                 const std::uint64_t elapsedTicks,
                                 const std::uint64_t durationTicks) noexcept {
    const double progress = static_cast<double>(elapsedTicks) /
                            static_cast<double>(durationTicks);
    return {
        .x = start.x + ((end.x - start.x) * progress),
        .y = start.y,
        .z = start.z + ((end.z - start.z) * progress),
    };
}

[[nodiscard]] double facingYaw(const Vec3 start, const Vec3 end) noexcept {
    const double deltaX = end.x - start.x;
    const double deltaZ = end.z - start.z;
    if (deltaX == 0.0 && deltaZ == 0.0) {
        return 0.0;
    }
    return std::atan2(deltaX, deltaZ);
}

[[nodiscard]] std::size_t remarkIndex(
    const std::uint64_t worldSeed, const PopulationCellId cellId,
    const std::uint32_t sampleOrdinal) {
    const std::uint64_t value = counterRandom({
        .worldSeed = worldSeed,
        .stream = RandomStreamId::FirstPlayableResidentRemark,
        .tick = 0,
        .stableId = stableEntityValue(cellId),
        .ordinal = sampleOrdinal,
    });
    return static_cast<std::size_t>(value % kResidentRemarkCount);
}

[[nodiscard]] std::vector<PopulationCellRow> sortedPopulationCells(
    const PopulationCellTable& populationCells) {
    std::vector<PopulationCellRow> cells;
    cells.reserve(populationCells.size());
    for (std::size_t index = 0; index < populationCells.size(); ++index) {
        cells.push_back(populationCells.row(index));
    }
    std::sort(cells.begin(),
              cells.end(),
              [](const PopulationCellRow& left,
                 const PopulationCellRow& right) {
                  return left.id.value() < right.id.value();
              });
    return cells;
}

void validateResidentSourceCell(const PopulationCellRow& cell) {
    if (!cell.id.isValid() || !isFinite(cell.positionMeters)) {
        throw std::logic_error(
            "population table exposed an invalid resident source row");
    }
}

[[nodiscard]] std::optional<StoreRow> resolveResidentTarget(
    const PopulationCellRow& cell, const StoreTable& stores) {
    if (!cell.assignedStore.has_value()) {
        return std::nullopt;
    }

    const std::optional<std::size_t> storeIndex =
        stores.find(*cell.assignedStore);
    if (!storeIndex.has_value()) {
        throw std::logic_error(
            "resident population assignment references a missing store");
    }
    const StoreRow candidate = stores.row(*storeIndex);
    if (!candidate.isActive || candidate.dimension != cell.dimension ||
        candidate.id != *cell.assignedStore ||
        !cell.preferredChain.has_value() ||
        *cell.preferredChain != candidate.chain) {
        throw std::logic_error(
            "resident population assignment is not an active local store");
    }
    return candidate;
}

struct ResidentCycle {
    StoreId targetStore{};
    Vec3 destination{};
    std::uint64_t travelTicks = 0;
    std::uint64_t storeStart = 0;
    std::uint64_t returnStart = 0;
    std::uint64_t cycleTicks = 0;
    double outboundYaw = 0.0;
};

[[nodiscard]] ResidentCycle makeResidentCycle(
    const PopulationCellRow& cell, const StoreRow& target,
    const std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content) {
    const Vec3 destination{
        .x = target.positionMeters.x,
        .y = cell.positionMeters.y,
        .z = target.positionMeters.z,
    };
    const std::uint64_t travelTicks = walkingTicks(
        cell.positionMeters,
        destination,
        ticksPerSecond,
        content.walkingSpeedMetersPerSecond);
    const std::uint64_t storeStart = checkedAdd(
        content.homeDwellTicks,
        travelTicks,
        "resident outbound trip duration overflow");
    const std::uint64_t returnStart = checkedAdd(
        storeStart,
        content.storeDwellTicks,
        "resident store dwell duration overflow");
    return {
        .targetStore = target.id,
        .destination = destination,
        .travelTicks = travelTicks,
        .storeStart = storeStart,
        .returnStart = returnStart,
        .cycleTicks = checkedAdd(
            returnStart,
            travelTicks,
            "resident trip cycle duration overflow"),
        .outboundYaw = facingYaw(cell.positionMeters, destination),
    };
}

[[nodiscard]] ResidentPresentation makeResidentPresentation(
    const PopulationCellRow& cell, const std::uint32_t ordinal,
    const ResidentPresentationContent& content) {
    return {
        .id = {.populationCellId = cell.id, .ordinal = ordinal},
        .phase = ResidentTripPhase::AtHome,
        .positionMeters = cell.positionMeters,
        .bubble = {
            .heightMeters = content.bubbleHeightMeters,
            .maxDistanceMeters = content.bubbleMaxDistanceMeters,
        },
    };
}

void sampleResidentPhase(
    ResidentPresentation& resident, const PopulationCellRow& cell,
    const ResidentCycle& cycle, const std::uint64_t completedTicks,
    const std::uint64_t worldSeed,
    const ResidentPresentationContent& content) {
    resident.targetStore = cycle.targetStore;
    const std::uint64_t phaseOffset = counterRandom({
        .worldSeed = worldSeed,
        .stream = RandomStreamId::FirstPlayableResidentSchedule,
        .tick = 0,
        .stableId = stableEntityValue(cell.id),
        .ordinal = resident.id.ordinal,
    }) % cycle.cycleTicks;
    const std::uint64_t activeCycleTick =
        cycleTick(completedTicks, phaseOffset, cycle.cycleTicks);

    if (activeCycleTick < content.homeDwellTicks) {
        resident.yawRadians = cycle.outboundYaw;
        return;
    }
    if (activeCycleTick < cycle.storeStart) {
        resident.phase = ResidentTripPhase::WalkingToStore;
        resident.positionMeters = interpolateXZ(
            cell.positionMeters,
            cycle.destination,
            activeCycleTick - content.homeDwellTicks,
            cycle.travelTicks);
        resident.yawRadians = cycle.outboundYaw;
        return;
    }
    if (activeCycleTick < cycle.returnStart) {
        resident.phase = ResidentTripPhase::AtStore;
        resident.positionMeters = cycle.destination;
        resident.yawRadians = cycle.outboundYaw;
        const std::uint64_t storeElapsed =
            activeCycleTick - cycle.storeStart;
        if (storeElapsed < content.speechDurationTicks) {
            resident.speech = content.remarks[remarkIndex(
                worldSeed, cell.id, resident.id.ordinal)];
        }
        return;
    }

    resident.phase = ResidentTripPhase::WalkingHome;
    resident.positionMeters = interpolateXZ(
        cycle.destination,
        cell.positionMeters,
        activeCycleTick - cycle.returnStart,
        cycle.travelTicks);
    resident.yawRadians = facingYaw(cycle.destination, cell.positionMeters);
}

void validateResidentProjection(const ResidentPresentation& resident) {
    if (!isFinite(resident.positionMeters) ||
        !std::isfinite(resident.yawRadians)) {
        throw std::logic_error(
            "resident projection produced a non-finite transform");
    }
}

}  // namespace

// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline
// @implements spec/feature/npc-conversations-and-placement-feedback.md Determinism and ownership
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::vector<ResidentPresentation> projectResidentPresentations(
    const std::uint64_t completedTicks, const std::uint64_t worldSeed,
    const std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content,
    const PopulationCellTable& populationCells, const StoreTable& stores) {
    validateResidentPresentationContent(content);
    if (ticksPerSecond == 0) {
        throw std::invalid_argument(
            "resident presentation requires a positive tick rate");
    }
    if (populationCells.size() >
        std::numeric_limits<std::size_t>::max() /
            content.samplesPerPopulationCell) {
        throw std::overflow_error("resident presentation count overflow");
    }

    std::vector<PopulationCellRow> cells =
        sortedPopulationCells(populationCells);

    std::vector<ResidentPresentation> residents;
    residents.reserve(cells.size() * content.samplesPerPopulationCell);
    for (const PopulationCellRow& cell : cells) {
        validateResidentSourceCell(cell);
        const std::optional<StoreRow> target =
            resolveResidentTarget(cell, stores);
        const std::optional<ResidentCycle> cycle = target.has_value()
            ? std::optional<ResidentCycle>(makeResidentCycle(
                  cell, *target, ticksPerSecond, content))
            : std::nullopt;

        for (std::uint32_t ordinal = 0;
             ordinal < content.samplesPerPopulationCell;
             ++ordinal) {
            ResidentPresentation resident =
                makeResidentPresentation(cell, ordinal, content);
            if (cycle.has_value()) {
                sampleResidentPhase(resident,
                                    cell,
                                    *cycle,
                                    completedTicks,
                                    worldSeed,
                                    content);
            }
            validateResidentProjection(resident);
            residents.push_back(std::move(resident));
        }
    }
    return residents;
}

}  // namespace konbini::sim
