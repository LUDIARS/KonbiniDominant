add_library(
  konbini_sim
  STATIC
    commands/command_queue.cpp
    commands/structural_command_buffer.cpp
    content/first_playable_content.cpp
    content/phase1_content.cpp
    content/skill_content.cpp
    content/skill_content_parser.cpp
    systems/skill_progression.cpp
    systems/skill_modifiers.cpp
    systems/skill_pulses.cpp
    content/campaign_content.cpp
    content/campaign_content_parser.cpp
    content/json_document.cpp
    content/json_document.h
    counter_rng.cpp
    economy/revenue_math.cpp
    economy/vertical_build_cost.cpp
    first_playable_simulation.cpp
    snapshots/canonical_snapshot.cpp
    snapshots/placement_cue_projection.cpp
    snapshots/resident_presentation.cpp
    snapshots/render_snapshot.cpp
    snapshots/campaign_hud.cpp
    storage/chain_economy_table.cpp
    storage/facility_table.cpp
    storage/population_cell_table.cpp
    storage/store_table.cpp
    storage/store_growth.cpp
    systems/competitive_zoc_system.cpp
    systems/dimension_system.cpp
    systems/dimension_conquest.cpp
    systems/campaign_command_system.cpp
    systems/campaign_progression.cpp
    systems/aion_invasion_system.cpp
    systems/aion_maxvalue_system.cpp
    systems/faith_system.cpp
    systems/dominant_triangle.cpp
    systems/encirclement_system.cpp
    systems/opponent_ai_system.cpp
    systems/phase1_outcome_system.cpp
    systems/population_change_system.cpp
    systems/economy_system.cpp
    systems/placement_system.cpp
    systems/population_initializer.cpp
    systems/select_chain_system.cpp
    systems/zoc_system.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/chain_id.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/chain_economy_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/canonical_snapshot.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/command_queue.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/counter_rng.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/dense_column.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/economy_system.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/entity_id.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/facility_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/figmentum_facility_key.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/first_playable_content.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/first_playable_simulation.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/game_state.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/math_types.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/placement_system.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/placement_cue_projection.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/player_command.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/population_cell_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/population_initializer.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/render_snapshot.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/render_store_placement_cue.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/resident_presentation.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/revenue_math.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/select_chain_system.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/store_table.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/structural_command_buffer.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/world_entity_ids.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/zoc_system.h"
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
