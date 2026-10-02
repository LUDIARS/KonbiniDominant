#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#ifndef NOGDI
#define NOGDI
#endif
#include "pictor/pipeline/compiled_batch_recorder.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

class GpuAssetStore;

// pipeline for one (shader key, pass type) pair.
struct BatchPipelineBinding {
    std::uint64_t shaderKey = 0;
    ::pictor::PassType passType = ::pictor::PassType::OPAQUE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

enum class BatchResolveFailureReason : std::uint8_t {
    UnknownMesh,
    MissingPipeline,
};

struct BatchResolveFailure {
    ::pictor::MeshHandle mesh = ::pictor::INVALID_MESH;
    std::uint64_t shaderKey = 0;
    ::pictor::PassType passType = ::pictor::PassType::OPAQUE;
    BatchResolveFailureReason reason = BatchResolveFailureReason::UnknownMesh;
};

[[nodiscard]] std::string describeBatchResolveFailure(
    const BatchResolveFailure& failure);

// `IBatchGpuSource` for KonbiniDominant (pictor-rendering.md#KonbiniBatchGpuSource).
//
// Resolves a `RenderBatch` to host-owned GPU handles:
// - `MeshHandle` → vertex / index buffer through `GpuAssetStore`
// - shader key + pass type → `VkPipeline` from the bound table
//
// Pictor's recorder treats `false` as "skip and count", which would silently
// drop geometry. Every `false` is therefore also recorded as a
// `BatchResolveFailure`; the frame bridge turns any recorded failure into an
// explicit frame error instead of drawing a placeholder.
//
// `assets` is borrowed and must outlive this source. Pipelines are borrowed
// from their owner and must be rebound after that owner recreates them.
class KonbiniBatchGpuSource final : public ::pictor::IBatchGpuSource {
public:
    explicit KonbiniBatchGpuSource(const GpuAssetStore& assets);

    // Replaces the pipeline table. Null pipelines and duplicate
    // (shader key, pass type) pairs are `std::invalid_argument`.
    void bindPipelines(std::span<const BatchPipelineBinding> bindings);
    void clearPipelines() noexcept;
    [[nodiscard]] std::size_t pipelineCount() const noexcept;

    bool resolve(const ::pictor::RenderBatch& batch, std::uint64_t shaderKey,
                 ::pictor::PassType passType,
                 ::pictor::BatchGpuResources& out) override;

    [[nodiscard]] std::span<const BatchResolveFailure> failures() const noexcept;
    void clearFailures() noexcept;

private:
    [[nodiscard]] VkPipeline findPipeline(
        std::uint64_t shaderKey, ::pictor::PassType passType) const noexcept;

    const GpuAssetStore* assets_ = nullptr;
    std::vector<BatchPipelineBinding> pipelines_;
    std::vector<BatchResolveFailure> failures_;
};

}  // namespace konbini::adapters::pictor
