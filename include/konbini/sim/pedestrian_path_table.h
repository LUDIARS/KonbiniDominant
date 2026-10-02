#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "konbini/sim/figmentum_facility_key.h"
#include "konbini/sim/math_types.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking
// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract

namespace konbini::sim {

// Figmentum が採番した pedestrian node / edge の stable key。KD は bit layout を
// 解釈せず、昇順 = canonical 順という性質だけを使う。
using PedestrianNodeKey = std::uint64_t;
using PedestrianEdgeKey = std::uint64_t;

struct PedestrianNodeRow {
    PedestrianNodeKey key = 0;
    Vec3 positionMeters{};
};

// 無向 edge。`from < to` は generator の canonical 形で、向きの意味は持たない。
struct PedestrianEdgeRow {
    PedestrianEdgeKey key = 0;
    PedestrianNodeKey from = 0;
    PedestrianNodeKey to = 0;
    double lengthMeters = 0.0;
};

// facility の歩行者用入口と、その入口から最寄りの node。dimension 複製された
// facility も同じ Figmentum key を持つので、key で引けば全 dimension に効く。
struct PedestrianEntranceRow {
    FigmentumFacilityKey facility{};
    Vec3 positionMeters{};
    PedestrianNodeKey node = 0;
};

// city 側の manifest から一方向に投影される read-only な歩行者 graph。
// gameplay の正本ではなく、resident presentation の経路選択だけが読む。
// node / edge / entrance は key 昇順の flat array で持ち、adjacency は
// CSR (offset + edge index 列、edge key 昇順) にして pointer chase を避ける。
// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking
class PedestrianPathTable {
public:
    // 入力は key 昇順・重複無し・finite・正の edge 長・既知 node 参照であること。
    // 破れていれば std::invalid_argument。黙って並べ替えたり捨てたりしない。
    [[nodiscard]] static PedestrianPathTable build(
        std::span<const PedestrianNodeRow> nodes,
        std::span<const PedestrianEdgeRow> edges,
        std::span<const PedestrianEntranceRow> entrances);

    [[nodiscard]] std::size_t nodeCount() const noexcept;
    [[nodiscard]] std::size_t edgeCount() const noexcept;
    [[nodiscard]] std::size_t entranceCount() const noexcept;

    [[nodiscard]] PedestrianNodeKey nodeKey(std::size_t nodeIndex) const;
    [[nodiscard]] Vec3 nodePosition(std::size_t nodeIndex) const;
    [[nodiscard]] std::optional<std::size_t> findNode(
        PedestrianNodeKey key) const noexcept;

    [[nodiscard]] PedestrianEdgeKey edgeKey(std::size_t edgeIndex) const;
    [[nodiscard]] std::size_t edgeFromNode(std::size_t edgeIndex) const;
    [[nodiscard]] std::size_t edgeToNode(std::size_t edgeIndex) const;
    // Route cost in whole millimetres. Integer costs make equal-length routes
    // compare exactly, so the stable edge-key tie-break is not decided by
    // floating-point summation order.
    [[nodiscard]] std::uint64_t edgeCostMillimeters(std::size_t edgeIndex) const;

    // node に接する edge index。edge key 昇順。
    [[nodiscard]] std::span<const std::uint32_t> adjacentEdges(
        std::size_t nodeIndex) const;

    [[nodiscard]] std::optional<PedestrianEntranceRow> findEntrance(
        FigmentumFacilityKey facility) const noexcept;

private:
    std::vector<PedestrianNodeKey> nodeKeys_;
    std::vector<Vec3> nodePositions_;
    std::vector<PedestrianEdgeKey> edgeKeys_;
    std::vector<std::uint32_t> edgeFrom_;
    std::vector<std::uint32_t> edgeTo_;
    std::vector<std::uint64_t> edgeCostsMillimeters_;
    std::vector<std::uint32_t> adjacencyOffsets_;
    std::vector<std::uint32_t> adjacencyEdges_;
    std::vector<FigmentumFacilityKey> entranceFacilities_;
    std::vector<Vec3> entrancePositions_;
    std::vector<std::uint32_t> entranceNodes_;
};

}  // namespace konbini::sim
