#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

#include "konbini/adapters/pictor/world_geometry_buffer.h"
#include "konbini/render/world_mesh.h"
#include "konbini/sim/figmentum_facility_key.h"

// @implements spec/interface/pictor-rendering.md Object lifecycle

namespace konbini::adapters::pictor {

// Figmentum の stable key から GPU 常駐 geometry を引く cache。
//
// key は geometry の同一性だけを表し、facility state (Intact / Replaced /
// Destroyed) や chain 色は含まない。色は draw ごとの tint で与えるので、
// state が変わっても upload をやり直す必要はない。
//
// 借用した `VkDevice` はこの cache より長生きしなければならない。破棄は
// 確保と逆順 (後から insert した geometry から) に行う。
class WorldGeometryCache {
public:
    WorldGeometryCache() = default;
    ~WorldGeometryCache();

    WorldGeometryCache(const WorldGeometryCache&) = delete;
    WorldGeometryCache& operator=(const WorldGeometryCache&) = delete;

    void initialize(VkPhysicalDevice physicalDevice, VkDevice device);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

    // 同じ key の二重登録は上書きせず `std::logic_error`。上書きすると
    // 描画中の buffer を差し替えうるうえ、key 衝突を検出できなくなる。
    void insert(
        sim::FigmentumFacilityKey key, const render::WorldMesh& mesh);

    [[nodiscard]] bool contains(
        sim::FigmentumFacilityKey key) const noexcept;

    // 未登録 key は `std::out_of_range`。geometry を持たない facility を
    // 無言で描画から落とすと、生成漏れと「そこに何も無い」が区別できない。
    [[nodiscard]] const WorldGeometryBuffer& find(
        sim::FigmentumFacilityKey key) const;

    [[nodiscard]] std::size_t size() const noexcept;

private:
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    // insert 順を保つ実体列。逆順破棄はこの列を後ろから回す。
    std::vector<std::unique_ptr<WorldGeometryBuffer>> buffers_;
    std::map<sim::FigmentumFacilityKey, std::size_t> indexByKey_;
};

}  // namespace konbini::adapters::pictor
