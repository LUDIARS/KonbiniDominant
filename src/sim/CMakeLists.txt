add_library(
  konbini_sim
  STATIC
    counter_rng.cpp
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/chain_id.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/counter_rng.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/figmentum_facility_key.h"
    "${PROJECT_SOURCE_DIR}/include/konbini/sim/math_types.h"
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
