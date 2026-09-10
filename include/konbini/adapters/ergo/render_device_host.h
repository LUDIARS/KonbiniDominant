#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "konbini/render/viewport_extent.h"

struct GLFWwindow;

namespace pictor {
class ISurfaceProvider;
class VulkanContext;
}  // namespace pictor

namespace ergo::render {
struct RenderContext;
}

// @implements spec/interface/ergo-runtime.md Render host
// @implements spec/plan/tasks/first-playable.md Build and process boundary

namespace konbini::adapters::ergo {

struct RenderDeviceConfig {
    std::uint32_t windowWidth = 1280;
    std::uint32_t windowHeight = 720;
    std::string windowTitle = "Konbini Dominant";
    // scene target は flight ごとに color / depth を持ち、Pictor の
    // AttachmentRegistry 上限に合わせて 1..4 を要求する。
    std::uint32_t framesInFlight = 2;
    bool validation = false;
    // build が生成した SPIR-V の置き場。上方探索 fallback は使わない。
    std::string shaderDirectory;
    std::string assetRoot;
};

// window / Vulkan device / Ergo `RenderContext` の所有者。
//
// 破棄順は Ergo layer 群 -> scene target -> この host。host 自身の破棄も
// VulkanContext -> surface provider の順を守る。
class RenderDeviceHost {
public:
    RenderDeviceHost();
    ~RenderDeviceHost();

    RenderDeviceHost(const RenderDeviceHost&) = delete;
    RenderDeviceHost& operator=(const RenderDeviceHost&) = delete;

    // 失敗時は確保済み resource を逆順で解放してから例外を投げる。
    void initialize(const RenderDeviceConfig& config);
    // Mobile host owns the native surface, which must outlive this device.
    void initialize(const RenderDeviceConfig& config,::pictor::ISurfaceProvider& surface);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

    [[nodiscard]] ::pictor::VulkanContext& vulkan();
    [[nodiscard]] ::pictor::ISurfaceProvider& surface();
    [[nodiscard]] ::ergo::render::RenderContext& context();
    [[nodiscard]] GLFWwindow* window() const;

    void pollEvents();
    // Minimized frames have no GPU work. Block briefly for window events so
    // the app does not spin a CPU core while waiting for restore/close.
    void waitEvents(double timeoutSeconds);
    [[nodiscard]] bool shouldClose() const;

    // swapchain の実 extent。minimize 中は 0 を含みうる。
    [[nodiscard]] render::ViewportExtent swapchainExtent() const;

    // window の framebuffer size。swapchain 再生成の要否判定に使う。
    [[nodiscard]] render::ViewportExtent framebufferExtent() const;

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::adapters::ergo
