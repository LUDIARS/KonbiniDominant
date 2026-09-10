#include "konbini/app/frame_presenter.h"
#include "konbini/render/aion_warning_geometry.h"
#include "konbini/render/campaign_floor_view.h"

#include <string>
#include <algorithm>
#include <utility>
#include <vector>

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`

namespace konbini::app {

FramePresenter::FramePresenter(
    render::WorldDrawListSpec drawListSpec, render::HudTextStyle hudStyle)
    : drawListSpec_(drawListSpec), hudStyle_(hudStyle) {}

void FramePresenter::observe(const sim::RenderSnapshot& snapshot) { construction_.observe(snapshot); }
void FramePresenter::advance(double dt) { construction_.advance(dt); }
void FramePresenter::reset() { construction_.reset(); }

// @implements spec/feature/ui-ux.md Common HUD
render::PreparedFrame FramePresenter::compose(
    const sim::RenderSnapshot& snapshot,
    const render::IsometricCamera& camera,
    const std::optional<sim::FacilityId> selectedFacility,
    const HudTextInput& hudInput,
    const PointerControls& controls, const std::optional<PointerAction> pressed) {
    // draw list は snapshot の順序をそのまま使うので、同じ snapshot からは
    // 常に同じ geometry になる。
    auto drawSpec = drawListSpec_;
    drawSpec.stackView = snapshot.hud().campaign.enabled &&
        snapshot.hud().campaign.reachedPhase >= 2;
    drawSpec.viewedFloor = hudInput.selectedFloor;
    auto drawList = render::buildWorldDrawList(snapshot, selectedFacility, drawSpec, construction_.visuals());
    const auto band = drawSpec.stackView ? render::storesInFloorBand(snapshot, drawSpec.viewedFloor)
                                        : std::vector<sim::RenderStore>{};
    const auto visible = drawSpec.stackView ? std::span<const sim::RenderStore>(band) : snapshot.stores();
    const auto particles = construction_.particleMesh(camera, visible);
    const auto particleBase = static_cast<std::uint32_t>(drawList.overlayMesh.vertices.size());
    drawList.overlayMesh.vertices.insert(drawList.overlayMesh.vertices.end(),
                                         particles.vertices.begin(), particles.vertices.end());
    for (const auto index : particles.indices) drawList.overlayMesh.indices.push_back(particleBase + index);


    auto mesh=render::buildHudTextMesh(controls.statusLines,controls.statusStyle,camera.extent);
    const auto warning = controls.modal ? render::WorldMesh{} : render::buildAionWarningGeometry(snapshot.hud(), camera.extent);
    const auto offset = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(), warning.vertices.begin(), warning.vertices.end());
    for (const auto index : warning.indices) mesh.indices.push_back(offset + index);
    const auto buttons=buildPointerControlMesh(controls,camera.extent,pressed);
    const auto buttonOffset=static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(),buttons.vertices.begin(),buttons.vertices.end());
    for(const auto index:buttons.indices) mesh.indices.push_back(buttonOffset+index);
    return {camera, std::move(drawList), std::move(mesh)};
}

const render::WorldDrawListSpec& FramePresenter::drawListSpec()
    const noexcept {
    return drawListSpec_;
}

const render::HudTextStyle& FramePresenter::hudStyle() const noexcept {
    return hudStyle_;
}

}  // namespace konbini::app
