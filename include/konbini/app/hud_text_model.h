#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "konbini/sim/chain_id.h"
#include "konbini/sim/entity_id.h"
#include "konbini/sim/placement_system.h"
#include "konbini/sim/render_snapshot.h"
#include "konbini/sim/select_chain_system.h"
#include "konbini/sim/campaign_command_system.h"

// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::app {

// HUD 1 フレーム分の入力。simulation から読むのは `HudViewModel` の値だけで、
// table を直接参照しない。
struct HudTextInput {
    sim::HudViewModel hud;
    std::optional<sim::FacilityId> selectedFacility;
    bool selectionIsPlacementCandidate = false;
    bool gridPlacement = false;
    std::int64_t selectedBuildCostCredits = 0;
    std::optional<sim::PlacementFailure> lastPlacementFailure;
    std::optional<sim::SelectChainFailure> lastChainFailure;
    bool showControls = true;
    std::uint32_t droppedTicks = 0;
    bool isPaused = false;
    std::uint32_t selectedFloor = 0, selectedFaith = 0, selectedBossForm = 0;
    bool selectedAntiStore = false;
    std::optional<sim::ChainId> selectedStoreChain;
    std::optional<sim::CampaignFailure> lastCampaignFailure;
};

// chain の HUD 表記。現在焼き込んでいるベクタフォントは ASCII しか持たない
// ので、brand 名を ASCII literal として直接持つ (BASE-FP-HUD-ASCII-01)。
// slug 由来の派生をやめたのは、slug が simulation 側の stable ID であり、
// presentation の brand 名とは別に動くため。content の `displayName` と
// 同じ文字列を保つ責任はここにある (data/content/first-playable.json)。
// 未知の chain は無言で空ラベルにせず `std::invalid_argument`。
[[nodiscard]] std::string hudChainLabel(sim::ChainId chain);

[[nodiscard]] std::string_view hudPlacementFailureText(
    sim::PlacementFailure failure) noexcept;
[[nodiscard]] std::string_view hudChainFailureText(
    sim::SelectChainFailure failure) noexcept;

// HUD の行を組む。返る文字列は ベクタフォントが扱える文字だけで、
// 表示できない文字が混ざった場合は `std::invalid_argument`。
[[nodiscard]] std::vector<std::string> buildHudLines(
    const HudTextInput& input);

}  // namespace konbini::app
