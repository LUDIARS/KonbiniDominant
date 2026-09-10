// @implements spec/feature/store-construction-effects.md Playback lifecycle
#include "konbini/adapters/ergo/construction_presentation.h"
#include "construction_particle_geometry.h"
#include "construction_particle_presets.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>
namespace konbini::adapters::ergo {
namespace {
struct Construction {
    render::StoreConstructionVisual visual;
    ::ergo::particle::ParticleSystem spin, dust;
    bool spinStopped = false, dustEmitted = false;

    explicit Construction(const sim::RenderStore& store) {
        visual.store = store;
        visual.animation = render::sampleStorePlacementAnimation(
            render::defaultStorePlacementAnimationSpec(), {store.positionMeters, 0}, 0);
        spin.set_config(detail::constructionSpinPreset());
        spin.update(0);  // Apply the config before burst; Ergo snapshots on update.
        spin.burst(12);
        spin.update(0);
        dust.set_config(detail::constructionDustPreset());
        dust.update(0);
    }
    void advance(const double dt) {
        visual.elapsedSeconds += dt;
        visual.animation = render::sampleStorePlacementAnimation(
            render::defaultStorePlacementAnimationSpec(),
            {visual.store.positionMeters, 0}, visual.elapsedSeconds);
        if (!spinStopped && visual.animation.stage != render::StorePlacementStage::Spin) {
            auto config = spin.config(); config.emission_rate = 0;
            spin.set_config(config); spinStopped = true;
        }
        spin.update(static_cast<float>(dt));
        if (visual.animation.hasLanded && !dustEmitted) {
            dust.burst(56);
            dust.update(0);
            dustEmitted = true;
        } else {
            dust.update(static_cast<float>(dt));
        }
    }
};
}
struct ConstructionPresentation::Impl {
    std::vector<std::unique_ptr<Construction>> active;
    std::vector<render::StoreConstructionVisual> visuals;
    std::optional<std::uint64_t> lastTick;
    std::uint32_t dimension = 0;
    void refresh() {
        visuals.clear();
        for (const auto& entry : active) visuals.push_back(entry->visual);
    }
};
ConstructionPresentation::ConstructionPresentation() : impl_(std::make_unique<Impl>()) {}
ConstructionPresentation::~ConstructionPresentation() = default;
void ConstructionPresentation::reset() {
    impl_->active.clear(); impl_->visuals.clear(); impl_->lastTick.reset();
}
void ConstructionPresentation::observe(const sim::RenderSnapshot& snapshot) {
    if (snapshot.hud().phase == sim::GamePhase::Result) {
        reset();
        impl_->lastTick = snapshot.completedTicks();
        return;
    }
    const auto dimension = snapshot.hud().campaign.visibleDimension;
    if ((impl_->lastTick && snapshot.completedTicks() < *impl_->lastTick) ||
        dimension != impl_->dimension) reset();
    impl_->dimension = dimension;
    if (impl_->lastTick == snapshot.completedTicks()) return;
    impl_->lastTick = snapshot.completedTicks();
    const auto stores = snapshot.stores();
    std::erase_if(impl_->active, [&](auto& entry) {
        const auto found = std::ranges::find(stores, entry->visual.store.id, &sim::RenderStore::id);
        if (found == stores.end()) return true;  // Destroyed or no longer visible.
        entry->visual.store = *found;
        return false;
    });
    for (const auto& cue : snapshot.placementCues()) {
        if (impl_->active.size() >= 64) break;  // Overflow draws the settled store.
        const auto found = std::ranges::find(stores, cue.storeId, &sim::RenderStore::id);
        if (found == stores.end() || std::ranges::any_of(impl_->active, [&](const auto& item) {
                return item->visual.store.id == cue.storeId;
            })) continue;
        impl_->active.push_back(std::make_unique<Construction>(*found));
    }
    impl_->refresh();
}
void ConstructionPresentation::advance(const double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
        throw std::invalid_argument("construction presentation requires a finite non-negative delta");
    const double dt = std::min(deltaSeconds, 0.1);
    for (auto& entry : impl_->active) entry->advance(dt);
    std::erase_if(impl_->active, [](const auto& entry) { return entry->visual.animation.isComplete; });
    impl_->refresh();
}
std::span<const render::StoreConstructionVisual> ConstructionPresentation::visuals() const {
    return impl_->visuals;
}
render::WorldMesh ConstructionPresentation::particleMesh(
    const render::IsometricCamera& camera, std::span<const sim::RenderStore> visibleStores) const {
    render::WorldMesh mesh;
    for (const auto& entry : impl_->active) {
        if (std::ranges::find(visibleStores, entry->visual.store.id, &sim::RenderStore::id)
                == visibleStores.end()) continue;
        detail::appendConstructionParticles(mesh, entry->spin.instances(), entry->visual, camera, false);
        detail::appendConstructionParticles(mesh, entry->dust.instances(), entry->visual, camera, true);
    }
    return mesh;
}
}
