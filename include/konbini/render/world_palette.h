#pragma once

#include "konbini/render/world_vertex.h"
#include "konbini/sim/chain_id.h"
#include "konbini/sim/facility_table.h"

namespace konbini::render {

using WorldColor = WorldVertex::ColorRgba;

[[nodiscard]] WorldColor chainColor(sim::ChainId chain, float alpha);
[[nodiscard]] WorldColor facilityColor(
    bool isBuildable, sim::FacilityState state);

// selection は facility の上に重ねる overlay なので、下の geometry を隠さない
// alpha を呼び出し側が選べるようにする。`chainColor` と同じく alpha は
// [0, 1] の閉区間のみ受け付ける。
[[nodiscard]] WorldColor selectedFacilityColor(float alpha);

}  // namespace konbini::render
