#pragma once

#include <memory>

// Ergo's Vulkan forward header can include Win32 before Pictor parses
// ObjectFlags::TRANSPARENT and PassType::OPAQUE. Suppress the colliding GDI
// macros at this public boundary, matching Pictor's registry headers.
#ifndef NOGDI
#define NOGDI
#endif
#include "ergo/render/render_layer.h"

namespace konbini::adapters::pictor {
class WorldSceneTargets;
}

namespace konbini::render {

// @implements spec/interface/pictor-rendering.md Offscreen world composition
class WorldCompositeLayer final : public ::ergo::render::IRenderLayer {
public:
    // `targets` and the RenderContext passed to initialize() are borrowed.
    // Shutdown order is composite -> targets -> VulkanContext; violating it
    // is fatal because the layer owns raw VkDevice resources.
    explicit WorldCompositeLayer(
        adapters::pictor::WorldSceneTargets& targets);
    ~WorldCompositeLayer() override;

    WorldCompositeLayer(const WorldCompositeLayer&) = delete;
    WorldCompositeLayer& operator=(const WorldCompositeLayer&) = delete;

    void initialize(::ergo::render::RenderContext& context) override;
    void set_render_pass(VkRenderPass renderPass) override;
    void record(VkCommandBuffer commandBuffer, VkExtent2D extent) override;
    void shutdown() override;

private:
    struct Impl;

    adapters::pictor::WorldSceneTargets* targets_ = nullptr;
    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::render
