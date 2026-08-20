#include "konbini/app/simulation_host.h"

#include <stdexcept>
#include <utility>

#include "konbini/city/city_manifest_projection.h"

// @implements spec/plan/tasks/first-playable.md Simulation

namespace konbini::app {

SimulationHost::SimulationHost(
    const std::filesystem::path& contentFile,
    const city::ICityGenerator& cityGenerator)
    : content_(sim::loadFirstPlayableContent(contentFile)),
      city_(cityGenerator.generateFirstPlayableCity(identities_.facilities())) {
    if (city_.manifest.facilities.empty()) {
        throw std::runtime_error("generated city has no facilities");
    }
    simulation_.emplace(
        content_, city::projectFacilityTable(city_.manifest), identities_);
    latestSnapshot_ = snapshot();
}

const sim::FirstPlayableContent& SimulationHost::content() const noexcept {
    return content_;
}

const city::GeneratedCity& SimulationHost::city() const noexcept {
    return city_;
}

void SimulationHost::submit(sim::PlayerCommand command) {
    simulation_->submit(std::move(command));
}

sim::CompletedTick SimulationHost::tick() {
    sim::CompletedTick completed = simulation_->completeNextTick();
    if (completed.render == nullptr) {
        throw std::runtime_error("completed tick published no render snapshot");
    }
    latestSnapshot_ = completed.render;
    return completed;
}

// @implements spec/interface/pictor-rendering.md `RenderSnapshot`
std::shared_ptr<const sim::RenderSnapshot> SimulationHost::snapshot() const {
    if (latestSnapshot_ != nullptr) {
        return latestSnapshot_;
    }
    // 最初の tick より前でも都市は描く。placement cue はまだ無いので空。
    return sim::makeRenderSnapshot(
        simulation_->state(), content_, simulation_->facilities(),
        simulation_->stores(), simulation_->populationCells(),
        simulation_->economy(), {});
}

std::uint64_t SimulationHost::completedTicks() const noexcept {
    return simulation_->state().completedTicks;
}

}  // namespace konbini::app
