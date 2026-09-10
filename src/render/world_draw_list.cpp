#include "konbini/render/world_draw_list.h"
#include "konbini/render/grid_ground.h"
#include "konbini/render/animated_store_geometry.h"
#include "konbini/render/phase1_overlay_geometry.h"
#include "konbini/render/campaign_floor_view.h"
#include "konbini/render/skill_pulse_geometry.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

// @implements spec/interface/pictor-rendering.md Offscreen world composition

namespace konbini::render {

namespace {

// 追加側の index を base 分だけずらして 1 本の mesh へ連結する。overlay は
// 1 つの vertex/index buffer へまとめて upload するので、builder ごとに
// draw を分けず geometry を結合する。
void appendMesh(WorldMesh& target, const WorldMesh& source) {
    if (source.vertices.empty()) {
        return;
    }
    if (target.vertices.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max()) -
            source.vertices.size()) {
        throw std::overflow_error("overlay mesh exceeds 32-bit index space");
    }
    const std::uint32_t base =
        static_cast<std::uint32_t>(target.vertices.size());
    target.vertices.insert(
        target.vertices.end(), source.vertices.begin(),
        source.vertices.end());
    target.indices.reserve(target.indices.size() + source.indices.size());
    for (const std::uint32_t index : source.indices) {
        target.indices.push_back(base + index);
    }
}

void validateSpec(const WorldDrawListSpec& spec) {
    if (spec.zocSegmentCount < 3U) {
        throw std::invalid_argument(
            "world draw list requires at least three ZOC segments");
    }
}

}  // namespace

WorldDrawListSpec defaultWorldDrawListSpec() noexcept {
    return {};
}

// @implements spec/interface/pictor-rendering.md Offscreen world composition
WorldDrawList buildWorldDrawList(
    const sim::RenderSnapshot& snapshot,
    const std::optional<sim::FacilityId> selectedFacility,
    const WorldDrawListSpec& spec,
    std::span<const StoreConstructionVisual> construction) {
    validateSpec(spec);
    if (selectedFacility.has_value() && !selectedFacility->isValid()) {
        throw std::invalid_argument(
            "world draw list received an invalid selected facility");
    }

    const std::span<const sim::RenderFacility> facilities =
        snapshot.facilities();
    WorldDrawList drawList;
    drawList.baseFacilities.reserve(facilities.size());
    const sim::RenderFacility* selected = nullptr;
    for (const sim::RenderFacility& facility : facilities) {
        if (!facility.id.isValid() || !facility.figmentumKey.isValid()) {
            throw std::invalid_argument(
                "world draw list received an invalid render facility");
        }
        if (selectedFacility.has_value() &&
            facility.id == *selectedFacility) {
            if (selected != nullptr) {
                throw std::invalid_argument(
                    "render snapshot has duplicate facility IDs");
            }
            selected = &facility;
        }

        if (spec.gridTown || facility.state == sim::FacilityState::Replaced ||
            (spec.stackView && spec.viewedFloor >= 16) ||
            (snapshot.hud().competitive && facility.state != sim::FacilityState::Intact)) {
            // Storefronts own replaced footprints; campaign lots and floor bands
            // retain their existing visibility rules.
            continue;
        }
        WorldFacilityDraw draw{
            .figmentumKey = facility.figmentumKey,
            .facilityId = facility.id,
            .tint = facilityColor(facility.isBuildable, facility.state),
        };
        if (snapshot.hud().campaign.enabled && snapshot.hud().campaign.visibleDimension != 0) {
            for (const auto& dimension : snapshot.hud().campaign.dimensions) {
                if (dimension.id != snapshot.hud().campaign.visibleDimension) continue;
                // Seed-derived lighting changes the world while retaining chain colors.
                const auto accent = static_cast<unsigned>(dimension.seed % 3);
                for (unsigned channel = 0; channel < 3; ++channel)
                    draw.tint[channel] *= channel == accent ? 1.0F : 0.65F;
            }
        }
        if (draw.tint[3] < 1.0F) {
            drawList.overlayFacilities.push_back(draw);
        } else {
            drawList.baseFacilities.push_back(draw);
        }
    }
    if (selectedFacility.has_value() && selected == nullptr) {
        throw std::invalid_argument(
            "selected facility is not present in the render snapshot");
    }

    if (spec.gridTown) appendMesh(drawList.overlayMesh, buildGridGround(facilities));
    const auto bandStores=spec.stackView ? storesInFloorBand(snapshot,spec.viewedFloor) : std::vector<sim::RenderStore>{};
    const std::span<const sim::RenderStore> visibleStores=spec.stackView ? std::span<const sim::RenderStore>(bandStores) : snapshot.stores();
    appendMesh(
        drawList.overlayMesh,
        buildZocOverlayGeometry(
            visibleStores, spec.zocSegmentCount, spec.zocGroundYMeters));
    drawList.storeMesh = buildAnimatedStoreGeometry(visibleStores, spec.storeMarker, construction);
    appendMesh(drawList.overlayMesh, buildSkillPulseGeometry(snapshot, visibleStores));
    if (snapshot.hud().competitive) {
        appendMesh(drawList.overlayMesh, buildPhase1OverlayGeometry(snapshot));
    }
    if (selected != nullptr) {
        appendMesh(
            drawList.overlayMesh,
            buildSelectionOverlayGeometry(spec.stackView ? facilityOnFloor(*selected,spec.viewedFloor,
                snapshot.hud().campaign.floorHeightMeters) : *selected, spec.selection));
    }
    return drawList;
}

}  // namespace konbini::render
