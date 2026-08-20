#include "konbini/adapters/pictor/world_overlay_buffers.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

// @implements spec/interface/pictor-rendering.md Required bridge

namespace konbini::adapters::pictor {

namespace {

// 空フレーム (店舗も選択も無い tick) でも buffer を 0 byte にはできないので、
// 最小容量を確保しておく。以後は必要量の 2 倍で伸ばし、毎フレーム再確保に
// ならないようにする。
constexpr VkDeviceSize kMinimumVertexBytes = 64U * 1024U;
constexpr VkDeviceSize kMinimumIndexBytes = 32U * 1024U;
constexpr std::uint32_t kMaximumFlightCount = 4;

[[nodiscard]] VkDeviceSize grownCapacity(
    const VkDeviceSize required, const VkDeviceSize minimum) {
    if (required > std::numeric_limits<VkDeviceSize>::max() / 2U) {
        throw std::length_error(
            "world overlay geometry is too large to allocate");
    }
    VkDeviceSize capacity = std::max(required, minimum);
    if (required > capacity / 2U) {
        capacity = required * 2U;
    }
    return capacity;
}

}  // namespace

WorldOverlayBuffers::~WorldOverlayBuffers() {
    shutdown();
}

void WorldOverlayBuffers::initialize(
    const VkPhysicalDevice physicalDevice, const VkDevice device,
    const std::uint32_t flightCount) {
    if (isInitialized()) {
        throw std::logic_error("world overlay buffers are already initialized");
    }
    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        throw std::invalid_argument(
            "world overlay buffers require a Vulkan device");
    }
    // Pictor の attachment registry と同じ上限に合わせる。scene target と
    // flight 数が食い違う構成を許すと、descriptor と buffer の対応が崩れる。
    if (flightCount == 0 || flightCount > kMaximumFlightCount) {
        throw std::invalid_argument(
            "world overlay flight count must be within Pictor limits");
    }

    physicalDevice_ = physicalDevice;
    device_ = device;
    try {
        for (std::uint32_t flight = 0; flight < flightCount; ++flight) {
            auto buffer = std::make_unique<WorldGeometryBuffer>();
            buffer->initialize(
                physicalDevice_, device_, kMinimumVertexBytes,
                kMinimumIndexBytes);
            perFlight_.push_back(std::move(buffer));
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

void WorldOverlayBuffers::shutdown() noexcept {
    while (!perFlight_.empty()) {
        perFlight_.back()->shutdown();
        perFlight_.pop_back();
    }
    physicalDevice_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}

bool WorldOverlayBuffers::isInitialized() const noexcept {
    return device_ != VK_NULL_HANDLE && !perFlight_.empty();
}

std::uint32_t WorldOverlayBuffers::flightCount() const noexcept {
    return static_cast<std::uint32_t>(perFlight_.size());
}

// @implements spec/interface/pictor-rendering.md Required bridge
const WorldGeometryBuffer& WorldOverlayBuffers::upload(
    const std::uint32_t flightIndex, const render::WorldMesh& mesh) {
    if (!isInitialized()) {
        throw std::logic_error(
            "world overlay upload requires initialization");
    }
    if (flightIndex >= perFlight_.size()) {
        throw std::out_of_range(
            "world overlay flight index is out of range");
    }

    WorldGeometryBuffer& buffer = *perFlight_[flightIndex];
    const VkDeviceSize vertexBytes =
        WorldGeometryBuffer::requiredVertexBytes(mesh);
    const VkDeviceSize indexBytes =
        WorldGeometryBuffer::requiredIndexBytes(mesh);
    if (vertexBytes > buffer.vertexCapacityBytes() ||
        indexBytes > buffer.indexCapacityBytes()) {
        // 触るのは記録中の flight の buffer だけなので、この場での破棄と
        // 再確保が他 flight の in-flight 参照を壊すことはない。ただし旧 buffer
        // を先に捨てると allocation / upload 失敗時に所有状態も失うので、
        // replacement を完成させてから入れ替える。
        auto replacement = std::make_unique<WorldGeometryBuffer>();
        replacement->initialize(
            physicalDevice_, device_,
            grownCapacity(vertexBytes, kMinimumVertexBytes),
            grownCapacity(indexBytes, kMinimumIndexBytes));
        replacement->upload(mesh);
        perFlight_[flightIndex] = std::move(replacement);
        return *perFlight_[flightIndex];
    }
    buffer.upload(mesh);
    return buffer;
}

}  // namespace konbini::adapters::pictor
