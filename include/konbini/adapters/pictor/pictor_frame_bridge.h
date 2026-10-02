#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

#include "konbini/adapters/pictor/pictor_scene_sync.h"
#include "konbini/render/world_draw_list.h"

#ifndef NOGDI
#define NOGDI
#endif
#include <vulkan/vulkan.h>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

class GpuAssetStore;

// Measured per-frame values (no estimates).
struct PictorFrameStats {
    // Simulation tick of the last consumed snapshot. Several render frames
    // can draw the same tick; the two counters advance independently.
    std::uint64_t snapshotTick = 0;
    // Render frames prepared since initialization.
    std::uint64_t frameSerial = 0;
    std::uint32_t objects = 0;
    std::uint32_t visibleObjects = 0;
    std::uint32_t batches = 0;
    std::uint32_t drawCalls = 0;
    std::uint32_t instances = 0;
    std::uint64_t triangles = 0;
};

// Game-side half of Pictor frame production
// (pictor-rendering.md#PictorFrameBridge).
//
// Owns Pictor's `SceneRegistry` / `CullingSystem` / `BatchBuilder` /
// `CompiledBatchRecorder` and drives them from the immutable snapshot:
//
//   consume()      snapshot draw list → FacilityId ↔ ObjectId diff, camera
//   prepareFrame() frustum cull → batch → instance data upload (flight N)
//   recordOpaque() / recordTranslucent()
//                  bind instance set + camera, hand batches to the recorder
//
// Frame acquire / submit / present, the attachment / render pass /
// framebuffer registry and resize stay with `WorldFrameGraph` /
// `WorldSceneTargets` on Ergo's `FrameComposer` (ergo-runtime.md#Render
// host); the bridge records inside the world pass that graph begins.
//
// Two lifetimes:
// - `initialize()` / `shutdown()` hold the Pictor scene. It survives
//   swapchain rebuilds, so a resize does not re-register the city.
// - `attachDevice()` / `setRenderPass()` / `detachDevice()` hold the
//   per-composer GPU objects (pipelines, instance buffers) and follow the
//   world layer's initialize / set_render_pass / shutdown.
//
// `assets` is borrowed and must outlive the bridge; the shutdown order is
// world layer (detach) → bridge → asset store → scene targets.
class PictorFrameBridge {
public:
    PictorFrameBridge();
    ~PictorFrameBridge();

    PictorFrameBridge(const PictorFrameBridge&) = delete;
    PictorFrameBridge& operator=(const PictorFrameBridge&) = delete;

    void initialize(GpuAssetStore& assets, std::uint32_t flightCount);
    void shutdown() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] std::uint32_t flightCount() const noexcept;

    void attachDevice(VkPhysicalDevice physicalDevice, VkDevice device,
                      const std::filesystem::path& shaderDirectory);
    // The caller guarantees the previous pipelines are no longer in flight.
    void setRenderPass(VkRenderPass renderPass);
    // The caller idles the device first.
    void detachDevice() noexcept;
    [[nodiscard]] bool isAttached() const noexcept;
    [[nodiscard]] VkRenderPass renderPass() const noexcept;

    // Applies the snapshot's facility draws and the camera used for culling
    // and recording. Throws before touching the scene when the draw list
    // references a mesh the asset store does not hold.
    SceneSyncReport consume(
        const render::WorldDrawList& drawList,
        const std::array<float, 16>& viewProjection);

    // Culls, batches and uploads the instance data for `flightIndex`. Must
    // run once per recorded frame, after the flight's fence was waited.
    void prepareFrame(std::uint32_t flightIndex);

    // Record into the world pass. Opaque runs before the depth-writing store
    // mesh, translucent after it (pictor-rendering.md#World pass recording).
    // A batch the GPU source cannot resolve is `std::runtime_error`.
    void recordOpaque(VkCommandBuffer commandBuffer, VkExtent2D extent);
    void recordTranslucent(VkCommandBuffer commandBuffer, VkExtent2D extent);

    [[nodiscard]] const PictorFrameStats& stats() const noexcept;
    [[nodiscard]] std::size_t objectCount() const noexcept;
    [[nodiscard]] std::optional<::pictor::ObjectId> objectFor(
        sim::FacilityId facility) const noexcept;

private:
    struct Impl;

    void recordPass(VkCommandBuffer commandBuffer, VkExtent2D extent,
                    bool translucent);

    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::adapters::pictor
