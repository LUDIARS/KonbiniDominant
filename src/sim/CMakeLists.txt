add_library(
  konbini_sim
  STATIC
    counter_rng.cpp
    snapshots/render_snapshot.cpp
    storage/chain_economy_table.cpp
    storage/facility_table.cpp
    storage/population_cell_table.cpp
    storage/store_table.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/chain_id.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/chain_economy_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/counter_rng.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/dense_column.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/entity_id.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/facility_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/figmentum_facility_key.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/first_playable_content.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/game_state.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/math_types.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/population_cell_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/render_snapshot.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/store_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/world_entity_ids.h"
)

target_include_directories(
  konbini_sim
  PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_features(konbini_sim PUBLIC cxx_std_20)

# public headerがUTF-8のnon-ASCII commentを含むため、`/utf-8` はwarning設定
# ではなくinterfaceの一部。konbini_enable_warnings は PRIVATE なので、
# consumer target (spec/setup/native-development.md#Compiler) へは届かない。
target_compile_options(konbini_sim PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

konbini_enable_warnings(konbini_sim)
