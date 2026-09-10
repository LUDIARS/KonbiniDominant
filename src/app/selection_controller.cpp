#include "konbini/app/selection_controller.h"

#include <algorithm>

// @implements spec/plan/tasks/first-playable.md Minimal controls

namespace konbini::app {
namespace {

[[nodiscard]] const sim::RenderFacility* findFacility(
    const sim::RenderSnapshot& snapshot,
    const sim::FacilityId id) noexcept {
    for (const sim::RenderFacility& facility : snapshot.facilities()) {
        if (facility.id == id) {
            return &facility;
        }
    }
    return nullptr;
}

}  // namespace

bool SelectionController::isPlacementCandidate(
    const sim::RenderFacility& facility) noexcept {
    // 建て替え可能かどうかの最終判定は simulation 側の `validatePlacement`。
    // ここは「選択を保持してよい候補か」だけを見る前段で、資金や chain の
    // 条件は複製しない。
    return facility.isBuildable &&
           (facility.allowsStacking || facility.state == sim::FacilityState::Intact ||
            (facility.isLotRepresentation && facility.state == sim::FacilityState::Destroyed));
}

// @implements spec/plan/tasks/first-playable.md Minimal controls
SelectionOutcome SelectionController::onPrimaryClick(
    const std::optional<render::FacilityPick>& pick,
    const sim::RenderSnapshot& snapshot, const bool placeImmediately) {
    if (!pick.has_value()) {
        // 何も無い場所への click は選択解除。選択を残すと、次の click が
        // 「見えていない選択の確定」になってしまう。
        selected_.reset();
        return {};
    }

    const sim::RenderFacility* const facility =
        findFacility(snapshot, pick->facilityId);
    if (facility == nullptr) {
        selected_.reset();
        return {};
    }

    if (!isPlacementCandidate(*facility)) {
        // 候補外の facility も選択はできる (状態を見るため) が、再 click でも
        // 配置は要求しない。
        selected_ = facility->id;
        return {.selected = selected_, .placementRequested = false};
    }

    const bool confirming =
        selected_.has_value() && *selected_ == facility->id;
    selected_ = facility->id;
    return {.selected = selected_, .placementRequested = placeImmediately || confirming};
}

void SelectionController::clear() noexcept {
    selected_.reset();
}

// @implements spec/interface/pictor-rendering.md Object lifecycle
void SelectionController::reconcile(
    const sim::RenderSnapshot& snapshot) noexcept {
    if (!selected_.has_value()) {
        return;
    }
    const sim::RenderFacility* const facility =
        findFacility(snapshot, *selected_);
    if (facility == nullptr || !isPlacementCandidate(*facility)) {
        selected_.reset();
    }
}

std::optional<sim::FacilityId> SelectionController::selected()
    const noexcept {
    return selected_;
}

}  // namespace konbini::app
