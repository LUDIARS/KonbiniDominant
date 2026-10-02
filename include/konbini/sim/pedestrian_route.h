#pragma once

#include <cstdint>
#include <vector>

#include "konbini/sim/figmentum_facility_key.h"
#include "konbini/sim/math_types.h"
#include "konbini/sim/pedestrian_path_table.h"

// @implements spec/feature/npc-conversations-and-placement-feedback.md Pedestrian path walking

namespace konbini::sim {

// 経路選択の結果。Routed 以外は「歩けない」ことを明示する状態で、
// 呼び出し側は直線へ置き換えてはならない。
enum class PedestrianRouteStatus : std::uint8_t {
    Routed = 0,
    // 出発 / 到着 facility の entrance が path table に無い。
    MissingEntrance,
    // entrance の node 同士が graph 上で連結していない。
    Unreachable,
};

struct PedestrianRoute {
    PedestrianRouteStatus status = PedestrianRouteStatus::Unreachable;
    // from entrance → node 列 → to entrance。連続する同一点は 1 つにまとめる。
    // Routed 以外では空。
    std::vector<Vec3> waypointsMeters;
    // 通った edge の stable key (歩く順)。
    std::vector<PedestrianEdgeKey> edges;
    // waypoint 間の XZ 距離の和 (entrance と node の間も含む)。
    double lengthMeters = 0.0;
};

// `from` の entrance から `to` の entrance までの最短経路を選ぶ。
//
// - edge cost は整数 mm の和で比較する
// - 同距離の候補がある node では、直前 edge の stable key が小さい方を採る。
//   edge cost は正なので、この規則は探索の取り出し順に依存しない
// - from == to は entrance 1 点・length 0 の Routed
//
// 純関数で、state も乱数も持たない。
[[nodiscard]] PedestrianRoute selectPedestrianRoute(
    const PedestrianPathTable& table, FigmentumFacilityKey from,
    FigmentumFacilityKey to);

}  // namespace konbini::sim
