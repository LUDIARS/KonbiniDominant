#include "konbini/adapters/ergo/tracked_render_layer.h"

#include <stdexcept>

// @implements spec/interface/ergo-runtime.md Render host

namespace konbini::adapters::ergo {

TrackedRenderLayer::TrackedRenderLayer(
    ::ergo::render::IRenderLayer& inner, LayerInitializationScope& scope)
    : inner_(&inner), scope_(&scope) {}

void TrackedRenderLayer::initialize(::ergo::render::RenderContext& context) {
    if (inner_ == nullptr || scope_ == nullptr) {
        throw std::logic_error("tracked render layer has no target");
    }
    // 例外はそのまま composer 経由で host へ伝播させ、既に初期化済みの
    // layer は scope が逆順で解放する。
    inner_->initialize(context);
    scope_->onInitialized(*inner_);
}

void TrackedRenderLayer::set_render_pass(const VkRenderPass renderPass) {
    inner_->set_render_pass(renderPass);
}

void TrackedRenderLayer::on_first_frame(
    ::ergo::render::RenderContext& context) {
    inner_->on_first_frame(context);
}

void TrackedRenderLayer::update(const ::ergo::render::FrameContext& frame) {
    inner_->update(frame);
}

void TrackedRenderLayer::record(
    const VkCommandBuffer commandBuffer, const VkExtent2D extent) {
    inner_->record(commandBuffer, extent);
}

void TrackedRenderLayer::shutdown() {
    if (inner_ == nullptr) {
        return;
    }
    if (scope_ != nullptr) {
        scope_->onShutdown(*inner_);
    }
    inner_->shutdown();
}

::ergo::render::IRenderLayer& TrackedRenderLayer::inner() const noexcept {
    return *inner_;
}

}  // namespace konbini::adapters::ergo
