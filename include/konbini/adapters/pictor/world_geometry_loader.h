#pragma once

#include <cstddef>

#include "konbini/adapters/pictor/world_geometry_cache.h"
#include "konbini/city/facility_geometry.h"

// @implements spec/interface/pictor-rendering.md Geometry conversion
// @implements spec/plan/tasks/first-playable.md City

namespace konbini::adapters::pictor {

struct WorldGeometryLoadReport {
    std::size_t uploaded = 0;
    // 同じ Figmentum key を持つ facility は同一 geometry なので 1 回だけ
    // upload する。key は geometry の同一性そのものを表す。
    std::size_t deduplicated = 0;
};

// 生成済み都市の CPU geometry を GPU 常駐 cache へ載せる。startup で 1 回
// 呼び、frame loop では呼ばない (first-playable.md#City)。
//
// null geometry、変換に失敗する mesh は例外で、無言で飛ばさない。飛ばすと
// 描画時に「未登録 key」として初めて分かることになる。
[[nodiscard]] WorldGeometryLoadReport loadCityGeometry(
    const city::GeneratedCity& city, WorldGeometryCache& cache);

}  // namespace konbini::adapters::pictor
