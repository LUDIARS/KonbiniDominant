#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "konbini/adapters/pictor/world_geometry_buffer.h"
#include "konbini/render/world_mesh.h"

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

// ZOC / store marker / selection の overlay geometry は毎フレーム作り直す
// ので、flight ごとに 1 本ずつ buffer を持つ。
//
// flight N の buffer を書いてよいのは、`VulkanContext::acquire_next_image()`
// が flight N の fence を待った後だけ。FrameComposer は acquire → record の
// 順に回すので、記録中の `current_frame()` に対応する buffer への書き込みは
// 直前の同 flight の submit が完了している。
class WorldOverlayBuffers {
public:
    WorldOverlayBuffers() = default;
    ~WorldOverlayBuffers();

    WorldOverlayBuffers(const WorldOverlayBuffers&) = delete;
    WorldOverlayBuffers& operator=(const WorldOverlayBuffers&) = delete;

    void initialize(
        VkPhysicalDevice physicalDevice, VkDevice device,
        std::uint32_t flightCount);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] std::uint32_t flightCount() const noexcept;

    // `flightIndex` の buffer へ upload し、その buffer を返す。容量不足なら
    // その flight の buffer だけを作り直す (他 flight の in-flight buffer には
    // 触れない)。返り値は次にこの flight を upload するまで有効。
    [[nodiscard]] const WorldGeometryBuffer& upload(
        std::uint32_t flightIndex, const render::WorldMesh& mesh);

private:
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<WorldGeometryBuffer>> perFlight_;
};

}  // namespace konbini::adapters::pictor
