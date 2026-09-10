#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

#include "konbini/city/facility_geometry.h"
#include "konbini/city/i_city_generator.h"
#include "konbini/sim/first_playable_content.h"
#include "konbini/sim/first_playable_simulation.h"
#include "konbini/sim/world_entity_ids.h"

// @implements spec/plan/tasks/first-playable.md Simulation
// @implements spec/plan/tasks/first-playable.md City

namespace konbini::app {

// content 読み込み、都市生成、simulation の所有をまとめた起動側の owner。
//
// 責務は「起動時に決まる正本を組み立て、tick を進めて snapshot を出す」
// ことだけで、入力解釈・描画・camera は持たない。
class SimulationHost {
public:
    // 生成は 1 回だけで、frame loop 中には行わない
    // (first-playable.md#City: mesh 生成は startup)。
    SimulationHost(
        const std::filesystem::path& contentFile,
        const city::ICityGenerator& cityGenerator);

    [[nodiscard]] const sim::FirstPlayableContent& content() const noexcept;
    [[nodiscard]] const city::GeneratedCity& city() const noexcept;

    void submit(sim::PlayerCommand command);
    [[nodiscard]] sim::CompletedTick tick();
    // Reuse immutable geometry; reset the complete authoritative match.
    void retry();

    // 未 tick でも描ける現在状態の snapshot。tick 後は `CompletedTick` の
    // snapshot が正本になる。
    [[nodiscard]] std::shared_ptr<const sim::RenderSnapshot> snapshot() const;

    [[nodiscard]] std::uint64_t completedTicks() const noexcept;

private:
    sim::FirstPlayableContent content_;
    sim::WorldEntityIds identities_;
    city::GeneratedCity city_;
    std::optional<sim::FirstPlayableSimulation> simulation_;
    std::shared_ptr<const sim::RenderSnapshot> latestSnapshot_;
};

}  // namespace konbini::app
