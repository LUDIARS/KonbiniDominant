#pragma once
#include "konbini/app/frame_input.h"
#include "konbini/app/command_composer.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::app {
class CampaignInputController {
public:
    void updateFloor(const FrameInput& input,const sim::RenderSnapshot& snapshot,std::optional<sim::FacilityId> selected);
    std::vector<sim::PlayerCommand> actions(const FrameInput& input,const sim::RenderSnapshot& snapshot,
        std::optional<sim::FacilityId> selected,std::uint64_t tick,CommandComposer& commands);
    std::uint32_t floor() const noexcept { return floor_; }
    void reset() noexcept { floor_=0; nextBuildTick_=0; visibleDimension_=0; }
private:
    std::uint32_t firstEmpty(const sim::RenderSnapshot& snapshot,sim::FacilityId facility) const;
    std::uint32_t floor_=0, visibleDimension_=0;
    std::uint64_t nextBuildTick_=0;
};
}
