#include "konbini/app/frame_presenter.h"

#include <string>
#include <utility>
#include <vector>

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`

namespace konbini::app {

FramePresenter::FramePresenter(
    render::WorldDrawListSpec drawListSpec, render::HudTextStyle hudStyle)
    : drawListSpec_(drawListSpec), hudStyle_(hudStyle) {}

// @implements spec/feature/ui-ux.md Common HUD
void FramePresenter::present(
    render::WorldRenderLayer& worldLayer, render::HudOverlayLayer& hudLayer,
    const sim::RenderSnapshot& snapshot,
    const render::IsometricCamera& camera,
    const std::optional<sim::FacilityId> selectedFacility,
    const HudTextInput& hudInput) {
    // draw list は snapshot の順序をそのまま使うので、同じ snapshot からは
    // 常に同じ geometry になる。
    worldLayer.publishFrame(
        camera,
        render::buildWorldDrawList(snapshot, selectedFacility, drawListSpec_));

    const std::vector<std::string> lines = buildHudLines(hudInput);
    hudLayer.publishFrame(
        render::buildHudTextMesh(lines, hudStyle_, camera.extent),
        camera.extent);
}

const render::WorldDrawListSpec& FramePresenter::drawListSpec()
    const noexcept {
    return drawListSpec_;
}

const render::HudTextStyle& FramePresenter::hudStyle() const noexcept {
    return hudStyle_;
}

}  // namespace konbini::app
