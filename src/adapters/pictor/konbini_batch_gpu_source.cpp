#include "konbini/adapters/pictor/konbini_batch_gpu_source.h"

#include <stdexcept>
#include <utility>

#include "konbini/adapters/pictor/gpu_asset_store.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

std::string describeBatchResolveFailure(const BatchResolveFailure& failure) {
    const char* const reason =
        failure.reason == BatchResolveFailureReason::UnknownMesh
            ? "mesh handle is not resident in the GPU asset store"
            : "no pipeline is bound for the shader key and pass";
    return std::string("Pictor batch resolve failed: ") + reason +
           " (mesh " + std::to_string(failure.mesh) + ", shader key " +
           std::to_string(failure.shaderKey) + ", pass " +
           std::to_string(static_cast<unsigned>(failure.passType)) + ")";
}

KonbiniBatchGpuSource::KonbiniBatchGpuSource(const GpuAssetStore& assets)
    : assets_(&assets) {}

void KonbiniBatchGpuSource::bindPipelines(
    const std::span<const BatchPipelineBinding> bindings) {
    std::vector<BatchPipelineBinding> table;
    table.reserve(bindings.size());
    for (const BatchPipelineBinding& binding : bindings) {
        if (binding.pipeline == VK_NULL_HANDLE) {
            throw std::invalid_argument(
                "batch GPU source rejects a null pipeline");
        }
        for (const BatchPipelineBinding& existing : table) {
            if (existing.shaderKey == binding.shaderKey &&
                existing.passType == binding.passType) {
                throw std::invalid_argument(
                    "batch GPU source received a duplicate pipeline binding");
            }
        }
        table.push_back(binding);
    }
    pipelines_ = std::move(table);
}

void KonbiniBatchGpuSource::clearPipelines() noexcept {
    pipelines_.clear();
}

std::size_t KonbiniBatchGpuSource::pipelineCount() const noexcept {
    return pipelines_.size();
}

VkPipeline KonbiniBatchGpuSource::findPipeline(
    const std::uint64_t shaderKey,
    const ::pictor::PassType passType) const noexcept {
    for (const BatchPipelineBinding& binding : pipelines_) {
        if (binding.shaderKey == shaderKey && binding.passType == passType) {
            return binding.pipeline;
        }
    }
    return VK_NULL_HANDLE;
}

// @implements spec/interface/pictor-rendering.md Failure
bool KonbiniBatchGpuSource::resolve(
    const ::pictor::RenderBatch& batch, const std::uint64_t shaderKey,
    const ::pictor::PassType passType, ::pictor::BatchGpuResources& out) {
    out = ::pictor::BatchGpuResources{};

    const VkPipeline pipeline = findPipeline(shaderKey, passType);
    if (pipeline == VK_NULL_HANDLE) {
        failures_.push_back({
            .mesh = batch.mesh,
            .shaderKey = shaderKey,
            .passType = passType,
            .reason = BatchResolveFailureReason::MissingPipeline,
        });
        return false;
    }
    const GpuMeshAsset* const mesh = assets_->resolve(batch.mesh);
    if (mesh == nullptr) {
        failures_.push_back({
            .mesh = batch.mesh,
            .shaderKey = shaderKey,
            .passType = passType,
            .reason = BatchResolveFailureReason::UnknownMesh,
        });
        return false;
    }

    out.pipeline = pipeline;
    out.vertex_buffer = mesh->buffers.vertexBuffer;
    out.index_buffer = mesh->buffers.indexBuffer;
    out.index_type = VK_INDEX_TYPE_UINT32;
    out.index_count = mesh->buffers.indexCount;
    return true;
}

std::span<const BatchResolveFailure> KonbiniBatchGpuSource::failures()
    const noexcept {
    return failures_;
}

void KonbiniBatchGpuSource::clearFailures() noexcept {
    failures_.clear();
}

}  // namespace konbini::adapters::pictor
