#pragma once

#include <optional>

#include "konbini/app/hud_text_model.h"
#include "konbini/render/hud_overlay_layer.h"
#include "konbini/render/hud_text_geometry.h"
#include "konbini/render/isometric_camera.h"
#include "konbini/render/world_draw_list.h"
#include "konbini/render/world_render_layer.h"
#include "konbini/sim/render_snapshot.h"

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::app {

// tick 終端の immutable snapshot だけを読み、world draw list と HUD geometry
// を組んで layer へ publish する。simulation table へは触れない。
class FramePresenter {
public:
    FramePresenter(
        render::WorldDrawListSpec drawListSpec, render::HudTextStyle hudStyle);

    void present(
        render::WorldRenderLayer& worldLayer,
        render::HudOverlayLayer& hudLayer,
        const sim::RenderSnapshot& snapshot,
        const render::IsometricCamera& camera,
        std::optional<sim::FacilityId> selectedFacility,
        const HudTextInput& hudInput);

    [[nodiscard]] const render::WorldDrawListSpec& drawListSpec()
        const noexcept;
    [[nodiscard]] const render::HudTextStyle& hudStyle() const noexcept;

private:
    render::WorldDrawListSpec drawListSpec_;
    render::HudTextStyle hudStyle_;
};

}  // namespace konbini::app
