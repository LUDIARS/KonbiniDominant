#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "konbini/sim/figmentum_facility_key.h"
#include "konbini/sim/math_types.h"
#include "konbini/sim/pedestrian_path_table.h"

// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract

namespace konbini::city {

// 受け入れる Figmentum pedestrian network の version。未対応 version は
// 読み替えずに reject する (figmentum-city-generation.md#Pedestrian path contract)。
inline constexpr std::uint32_t kSupportedPedestrianNetworkSchemaVersion = 1;
inline constexpr std::uint32_t kSupportedPedestrianNetworkRecipeVersion = 1;

// Figmentum `PedestrianSide` の値をそのまま写す。値は canonical bytes に入る。
enum class PedestrianEntranceSide : std::uint8_t {
    South = 0,
    East = 1,
    North = 2,
    West = 3,
};

struct PedestrianPathNode {
    sim::PedestrianNodeKey key = 0;
    sim::Vec3 positionMeters{};
};

struct PedestrianPathEdge {
    sim::PedestrianEdgeKey key = 0;
    sim::PedestrianNodeKey fromNode = 0;
    sim::PedestrianNodeKey toNode = 0;
    double lengthMeters = 0.0;
};

struct PedestrianPathEntrance {
    sim::FigmentumFacilityKey facility{};
    PedestrianEntranceSide side = PedestrianEntranceSide::South;
    sim::Vec3 positionMeters{};
    sim::PedestrianNodeKey node = 0;
};

// Figmentum が所有する歩行者 semantic path を、manifest 上に game-owned な
// 値として投影したもの。KD はこれを編集せず、別の道路 graph も正本にしない。
// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
struct PedestrianPathContract {
    std::uint32_t schemaVersion = 0;
    std::uint32_t recipeVersion = 0;
    std::vector<PedestrianPathNode> nodes;          // key 昇順
    std::vector<PedestrianPathEdge> edges;          // key 昇順
    std::vector<PedestrianPathEntrance> entrances;  // facility key 昇順
};

// 構造契約を検査する: version、key 昇順と重複、edge endpoint / entrance node の
// 存在、finite 座標、正の edge 長、全 facility に entrance が 1 つずつあること。
// 連結性は検査しない (到達不能は経路選択が明示状態で返す)。
// 違反は std::invalid_argument。
// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
void validatePedestrianPathContract(
    const PedestrianPathContract& contract,
    std::span<const sim::FigmentumFacilityKey> facilityKeys);

// sim が読む flat table へ変換する。contract は検証済みであること。
// @implements spec/interface/figmentum-city-generation.md Pedestrian path contract
[[nodiscard]] sim::PedestrianPathTable toPedestrianPathTable(
    const PedestrianPathContract& contract);

}  // namespace konbini::city
