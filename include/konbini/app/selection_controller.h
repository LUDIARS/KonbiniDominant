#pragma once

#include <optional>

#include "konbini/render/facility_picker.h"
#include "konbini/sim/entity_id.h"
#include "konbini/sim/render_snapshot.h"

// @implements spec/plan/tasks/first-playable.md Minimal controls
// @implements spec/feature/ui-ux.md Phase 1

namespace konbini::app {

// click 1 回の結果。`placementRequested` が true のときだけ host は
// `PlaceStoreCommand` を積む。選択そのものは presentation state なので、
// simulation へは何も送らない。
struct SelectionOutcome {
    std::optional<sim::FacilityId> selected;
    bool placementRequested = false;
};

// 「1 回目の click で選択、同じ有効候補を再 click で配置確定」という 2 段
// 操作の状態だけを持つ。picking (ray 交差) は `render::pickFacility` の、
// 配置可否の正本は simulation の責務で、ここでは重複させない。
class SelectionController {
public:
    [[nodiscard]] SelectionOutcome onPrimaryClick(
        const std::optional<render::FacilityPick>& pick,
        const sim::RenderSnapshot& snapshot);

    // 右 click / Esc。
    void clear() noexcept;

    // tick 後に呼ぶ。選択中の facility が snapshot から消えた、または配置
    // 候補でなくなった (店舗へ置換された等) 場合は選択を落とす。残すと
    // 「選択中なのに置けない」状態が画面に残り続ける。
    void reconcile(const sim::RenderSnapshot& snapshot) noexcept;

    [[nodiscard]] std::optional<sim::FacilityId> selected() const noexcept;

    // snapshot 上で配置候補として扱えるか。HUD と draw list の両方が同じ
    // 判定を使う。
    [[nodiscard]] static bool isPlacementCandidate(
        const sim::RenderFacility& facility) noexcept;

private:
    std::optional<sim::FacilityId> selected_;
};

}  // namespace konbini::app
