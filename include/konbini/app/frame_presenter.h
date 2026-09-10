#pragma once

#include <optional>
#include "konbini/adapters/ergo/construction_presentation.h"

#include "konbini/app/hud_text_model.h"
#include "konbini/app/pointer_controls.h"
#include "konbini/render/prepared_frame.h"
#include "konbini/render/hud_text_geometry.h"
#include "konbini/render/isometric_camera.h"
#include "konbini/render/world_draw_list.h"
#include "konbini/sim/render_snapshot.h"

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::app {

// tick 終端の immutable snapshot だけを読み、world draw list と HUD geometry
// を組んで GPU 非依存の frame として返す。simulation table へは触れない。
class FramePresenter {
public:
    FramePresenter(
        render::WorldDrawListSpec drawListSpec, render::HudTextStyle hudStyle);

    void observe(const sim::RenderSnapshot& snapshot);
    void advance(double deltaSeconds);
    void reset();

    [[nodiscard]] render::PreparedFrame compose(
        const sim::RenderSnapshot& snapshot,
        const render::IsometricCamera& camera,
        std::optional<sim::FacilityId> selectedFacility,
        const HudTextInput& hudInput,
        const PointerControls& controls, std::optional<PointerAction> pressed);

    [[nodiscard]] const render::WorldDrawListSpec& drawListSpec()
        const noexcept;
    [[nodiscard]] const render::HudTextStyle& hudStyle() const noexcept;

private:
    adapters::ergo::ConstructionPresentation construction_;
    render::WorldDrawListSpec drawListSpec_;
    render::HudTextStyle hudStyle_;
};

}  // namespace konbini::app
