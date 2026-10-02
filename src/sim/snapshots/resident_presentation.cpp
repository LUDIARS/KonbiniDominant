#include "konbini/sim/resident_presentation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "konbini/sim/counter_rng.h"
#include "konbini/sim/pedestrian_route.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline
// @implements spec/interface/visia-presentation.md Pictor integration boundary
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking

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
    const double distance, const std::uint32_t ticksPerSecond,
    const double speedMetersPerSecond) {
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

// The trip as a planar polyline from the home point to the store point.
// `cumulativeMeters[i]` is the walked distance at `waypoints[i]`.
struct ResidentCycle {
    StoreId targetStore{};
    ResidentRouteState route = ResidentRouteState::DirectLine;
    std::vector<Vec3> waypoints;
    std::vector<double> cumulativeMeters;
    std::uint64_t travelTicks = 0;
    std::uint64_t storeStart = 0;
    std::uint64_t returnStart = 0;
    std::uint64_t cycleTicks = 0;
};

struct TripSample {
    Vec3 position{};
    double yawRadians = 0.0;
};

[[nodiscard]] double lengthMeters(const ResidentCycle& cycle) noexcept {
    return cycle.cumulativeMeters.back();
}

// Facing along the first segment with extent; a zero-length trip faces +Z.
[[nodiscard]] double outboundYaw(const ResidentCycle& cycle) noexcept {
    for (std::size_t index = 1; index < cycle.waypoints.size(); ++index) {
        if (cycle.cumulativeMeters[index] > cycle.cumulativeMeters[index - 1]) {
            return facingYaw(cycle.waypoints[index - 1], cycle.waypoints[index]);
        }
    }
    return 0.0;
}

// Facing on arrival at the store, i.e. along the last segment with extent.
[[nodiscard]] double arrivalYaw(const ResidentCycle& cycle) noexcept {
    for (std::size_t index = cycle.waypoints.size(); index > 1; --index) {
        if (cycle.cumulativeMeters[index - 1] >
            cycle.cumulativeMeters[index - 2]) {
            return facingYaw(cycle.waypoints[index - 2],
                             cycle.waypoints[index - 1]);
        }
    }
    return 0.0;
}

// Position and facing after walking `elapsedTicks` of `durationTicks`, from
// the home end or (when `returning`) from the store end. The resident faces
// along the segment it is on, in its walking direction.
[[nodiscard]] TripSample sampleTrip(const ResidentCycle& cycle,
                                    const std::uint64_t elapsedTicks,
                                    const std::uint64_t durationTicks,
                                    const bool returning) noexcept {
    const std::size_t segments = cycle.waypoints.size() - 1;
    if (segments == 0) {
        return {.position = cycle.waypoints.front(), .yawRadians = 0.0};
    }
    const double progress = static_cast<double>(elapsedTicks) /
                            static_cast<double>(durationTicks);
    if (segments == 1) {
        // One straight segment keeps the exact BASE-NPC-PATH-01 expression.
        const Vec3 start = returning ? cycle.waypoints[1] : cycle.waypoints[0];
        const Vec3 end = returning ? cycle.waypoints[0] : cycle.waypoints[1];
        return {
            .position = {
                .x = start.x + ((end.x - start.x) * progress),
                .y = start.y,
                .z = start.z + ((end.z - start.z) * progress),
            },
            .yawRadians = facingYaw(start, end),
        };
    }

    const double walked =
        (returning ? 1.0 - progress : progress) * lengthMeters(cycle);
    std::size_t segment = 0;
    while (segment + 1 < segments &&
           cycle.cumulativeMeters[segment + 1] <= walked) {
        ++segment;
    }
    const Vec3 start = cycle.waypoints[segment];
    const Vec3 end = cycle.waypoints[segment + 1];
    const double segmentLength =
        cycle.cumulativeMeters[segment + 1] - cycle.cumulativeMeters[segment];
    const double fraction = segmentLength > 0.0
        ? std::clamp((walked - cycle.cumulativeMeters[segment]) / segmentLength,
                     0.0,
                     1.0)
        : 0.0;
    return {
        .position = {
            .x = start.x + ((end.x - start.x) * fraction),
            .y = start.y,
            .z = start.z + ((end.z - start.z) * fraction),
        },
        .yawRadians = returning ? facingYaw(end, start) : facingYaw(start, end),
    };
}

[[nodiscard]] ResidentCycle makeResidentCycle(
    const ResidentRouteState route, std::vector<Vec3> waypoints,
    const StoreId targetStore, const std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content) {
    ResidentCycle cycle{
        .targetStore = targetStore,
        .route = route,
        .waypoints = std::move(waypoints),
    };
    cycle.cumulativeMeters.reserve(cycle.waypoints.size());
    cycle.cumulativeMeters.push_back(0.0);
    for (std::size_t index = 1; index < cycle.waypoints.size(); ++index) {
        cycle.cumulativeMeters.push_back(
            cycle.cumulativeMeters.back() +
            std::hypot(cycle.waypoints[index].x - cycle.waypoints[index - 1].x,
                       cycle.waypoints[index].z - cycle.waypoints[index - 1].z));
    }
    cycle.travelTicks = walkingTicks(lengthMeters(cycle),
                                     ticksPerSecond,
                                     content.walkingSpeedMetersPerSecond);
    cycle.storeStart = checkedAdd(content.homeDwellTicks,
                                  cycle.travelTicks,
                                  "resident outbound trip duration overflow");
    cycle.returnStart = checkedAdd(cycle.storeStart,
                                   content.storeDwellTicks,
                                   "resident store dwell duration overflow");
    cycle.cycleTicks = checkedAdd(cycle.returnStart,
                                  cycle.travelTicks,
                                  "resident trip cycle duration overflow");
    return cycle;
}

// BASE-NPC-PATH-01: the straight XZ line used when the city has no
// pedestrian network (grid town).
[[nodiscard]] ResidentCycle makeDirectLineCycle(
    const PopulationCellRow& cell, const StoreRow& target,
    const std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content) {
    const Vec3 destination{
        .x = target.positionMeters.x,
        .y = cell.positionMeters.y,
        .z = target.positionMeters.z,
    };
    std::vector<Vec3> waypoints{cell.positionMeters};
    if (destination.x != cell.positionMeters.x ||
        destination.z != cell.positionMeters.z) {
        waypoints.push_back(destination);
    }
    return makeResidentCycle(ResidentRouteState::DirectLine,
                             std::move(waypoints),
                             target.id,
                             ticksPerSecond,
                             content);
}

[[nodiscard]] FigmentumFacilityKey facilityKey(const FacilityTable& facilities,
                                               const FacilityId id) {
    const std::optional<std::size_t> index = facilities.find(id);
    if (!index.has_value()) {
        throw std::logic_error(
            "resident trip references a facility missing from the facility table");
    }
    return facilities.row(*index).figmentumKey;
}

using RouteCache = std::map<std::pair<FigmentumFacilityKey, FigmentumFacilityKey>,
                            PedestrianRoute>;

// Trip along the pedestrian network, or the explicit reason it cannot be
// walked. Routes are memoised per projection because cells share stores.
[[nodiscard]] std::variant<ResidentCycle, ResidentRouteState> makePathCycle(
    const PopulationCellRow& cell, const StoreRow& target,
    const ResidentPathContext& paths, RouteCache& routes,
    const std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content) {
    const FigmentumFacilityKey home =
        facilityKey(paths.facilities, cell.facilityId);
    const FigmentumFacilityKey store =
        facilityKey(paths.facilities, target.facilityId);
    auto cached = routes.find({home, store});
    if (cached == routes.end()) {
        cached = routes
                     .emplace(std::pair{home, store},
                              selectPedestrianRoute(paths.paths, home, store))
                     .first;
    }
    const PedestrianRoute& route = cached->second;
    switch (route.status) {
        case PedestrianRouteStatus::Routed:
            break;
        case PedestrianRouteStatus::MissingEntrance:
            return ResidentRouteState::MissingEntrance;
        case PedestrianRouteStatus::Unreachable:
            return ResidentRouteState::Unreachable;
    }
    // Residents walk on the population cell's ground plane, as the
    // straight-line baseline does.
    std::vector<Vec3> waypoints;
    waypoints.reserve(route.waypointsMeters.size());
    for (const Vec3 point : route.waypointsMeters) {
        waypoints.push_back(
            {.x = point.x, .y = cell.positionMeters.y, .z = point.z});
    }
    return makeResidentCycle(ResidentRouteState::PedestrianPath,
                             std::move(waypoints),
                             target.id,
                             ticksPerSecond,
                             content);
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
    resident.route = cycle.route;
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
        resident.positionMeters = cycle.waypoints.front();
        resident.yawRadians = outboundYaw(cycle);
        return;
    }
    if (activeCycleTick < cycle.storeStart) {
        resident.phase = ResidentTripPhase::WalkingToStore;
        const TripSample sample = sampleTrip(cycle,
                                             activeCycleTick - content.homeDwellTicks,
                                             cycle.travelTicks,
                                             false);
        resident.positionMeters = sample.position;
        resident.yawRadians = sample.yawRadians;
        return;
    }
    if (activeCycleTick < cycle.returnStart) {
        resident.phase = ResidentTripPhase::AtStore;
        resident.positionMeters = cycle.waypoints.back();
        resident.yawRadians = arrivalYaw(cycle);
        const std::uint64_t storeElapsed =
            activeCycleTick - cycle.storeStart;
        if (storeElapsed < content.speechDurationTicks) {
            resident.speech = content.remarks[remarkIndex(
                worldSeed, cell.id, resident.id.ordinal)];
        }
        return;
    }

    resident.phase = ResidentTripPhase::WalkingHome;
    const TripSample sample = sampleTrip(
        cycle, activeCycleTick - cycle.returnStart, cycle.travelTicks, true);
    resident.positionMeters = sample.position;
    resident.yawRadians = sample.yawRadians;
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
// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking
// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::vector<ResidentPresentation> projectResidentPresentations(
    const std::uint64_t completedTicks, const std::uint64_t worldSeed,
    const std::uint32_t ticksPerSecond,
    const ResidentPresentationContent& content,
    const PopulationCellTable& populationCells, const StoreTable& stores,
    const ResidentPathContext* const paths) {
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
    RouteCache routes;

    std::vector<ResidentPresentation> residents;
    residents.reserve(cells.size() * content.samplesPerPopulationCell);
    for (const PopulationCellRow& cell : cells) {
        validateResidentSourceCell(cell);
        const std::optional<StoreRow> target =
            resolveResidentTarget(cell, stores);
        std::optional<ResidentCycle> cycle;
        ResidentRouteState blockedRoute = ResidentRouteState::NoTrip;
        if (target.has_value() && paths == nullptr) {
            cycle = makeDirectLineCycle(cell, *target, ticksPerSecond, content);
        } else if (target.has_value()) {
            auto routed = makePathCycle(
                cell, *target, *paths, routes, ticksPerSecond, content);
            if (auto* const value = std::get_if<ResidentCycle>(&routed)) {
                cycle = std::move(*value);
            } else {
                blockedRoute = std::get<ResidentRouteState>(routed);
            }
        }

        for (std::uint32_t ordinal = 0;
             ordinal < content.samplesPerPopulationCell;
             ++ordinal) {
            ResidentPresentation resident =
                makeResidentPresentation(cell, ordinal, content);
            if (blockedRoute != ResidentRouteState::NoTrip) {
                // Explicit "cannot walk there": stay home, no speech, and
                // never substitute a straight line.
                resident.targetStore = target->id;
                resident.route = blockedRoute;
            } else if (cycle.has_value()) {
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
