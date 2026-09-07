#include "konbini/app/hud_text_model.h"

#include <cctype>
#include <stdexcept>

#include "konbini/render/bitmap_font.h"

// @implements spec/feature/ui-ux.md Common HUD

namespace konbini::app {
namespace {

[[nodiscard]] std::string formatSigned(const std::int64_t value) {
    return std::to_string(value);
}

void requireRenderable(const std::string& line) {
    for (const char character : line) {
        if (!render::isBitmapFontCharacter(character)) {
            throw std::invalid_argument(
                "hud line contains a character the bitmap font cannot draw");
        }
    }
}

}  // namespace

// @implements spec/feature/chain-selection.md 共通
std::string hudChainLabel(const sim::ChainId chain) {
    switch (chain) {
        case sim::ChainId::Losan: return "MOONPANTRY";
        case sim::ChainId::Famoma: return "SUNFOLD";
        case sim::ChainId::SebanIleban: return "DAYLARK";
    }
    throw std::invalid_argument("hud chain label requires a known chain");
}

// @implements spec/feature/ui-ux.md Common HUD
std::string_view hudPlacementFailureText(
    const sim::PlacementFailure failure) noexcept {
    switch (failure) {
        case sim::PlacementFailure::None:
            return "OK";
        case sim::PlacementFailure::WrongPhase:
            return "WRONG PHASE";
        case sim::PlacementFailure::NoPlayerChain:
            return "SELECT A CHAIN FIRST";
        case sim::PlacementFailure::InvalidChain:
            return "INVALID CHAIN";
        case sim::PlacementFailure::WrongChain:
            return "NOT YOUR CHAIN";
        case sim::PlacementFailure::UnsupportedVerticalSlot:
            return "SLOT NOT SUPPORTED";
        case sim::PlacementFailure::FacilityNotFound:
            return "FACILITY NOT FOUND";
        case sim::PlacementFailure::FacilityProtected:
            return "FACILITY PROTECTED";
        case sim::PlacementFailure::FacilityUnavailable:
            return "FACILITY UNAVAILABLE";
        case sim::PlacementFailure::FacilityOccupied:
            return "FACILITY OCCUPIED";
        case sim::PlacementFailure::InsufficientCash:
            return "NOT ENOUGH CASH";
    }
    return "UNKNOWN PLACEMENT RESULT";
}

// @implements spec/feature/chain-selection.md 共通
std::string_view hudChainFailureText(
    const sim::SelectChainFailure failure) noexcept {
    switch (failure) {
        case sim::SelectChainFailure::None:
            return "OK";
        case sim::SelectChainFailure::WrongPhase:
            return "CHAIN ALREADY LOCKED";
        case sim::SelectChainFailure::ChainAlreadySelected:
            return "CHAIN ALREADY SELECTED";
        case sim::SelectChainFailure::ChainAlreadyActive:
            return "CHAIN TAKEN";
        case sim::SelectChainFailure::InvalidChain:
            return "INVALID CHAIN";
    }
    return "UNKNOWN CHAIN RESULT";
}

// @implements spec/feature/ui-ux.md Common HUD
std::vector<std::string> buildHudLines(const HudTextInput& input) {
    std::vector<std::string> lines;

    if (input.hud.playerChain.has_value()) {
        lines.push_back("CHAIN " + hudChainLabel(*input.hud.playerChain));
    } else {
        lines.emplace_back("CHAIN NONE - PRESS 1 2 3");
    }

    lines.push_back("CASH " + formatSigned(input.hud.cashCredits));
    lines.push_back("STORES " + std::to_string(input.hud.storeCount));
    lines.push_back(
        "INCOME " + formatSigned(input.hud.predictedEconomyIncomeCredits) +
        " IN " + std::to_string(input.hud.ticksUntilEconomy) + " TICKS");
    lines.push_back("TICK " + std::to_string(input.hud.completedTicks));

    if (input.selectedFacility.has_value()) {
        // stable ID は index/generation の対。片方だけ出すと、破棄と再利用で
        // 同じ表示になってしまう。
        std::string line =
            "SELECTED " +
            std::to_string(input.selectedFacility->value().index) + "." +
            std::to_string(input.selectedFacility->value().generation);
        if (input.selectionIsPlacementCandidate) {
            line += " COST " + formatSigned(input.selectedBuildCostCredits) +
                    " - CLICK AGAIN";
        } else {
            line += " NOT BUILDABLE";
        }
        lines.push_back(std::move(line));
    } else {
        lines.emplace_back("SELECTED NONE");
    }

    if (input.lastChainFailure.has_value() &&
        *input.lastChainFailure != sim::SelectChainFailure::None) {
        lines.push_back(
            "CHAIN REJECTED - " +
            std::string(hudChainFailureText(*input.lastChainFailure)));
    }
    if (input.lastPlacementFailure.has_value() &&
        *input.lastPlacementFailure != sim::PlacementFailure::None) {
        // 色だけでなく理由文を出す (ui-ux.md#Common HUD)。
        lines.push_back(
            "PLACEMENT REJECTED - " +
            std::string(
                hudPlacementFailureText(*input.lastPlacementFailure)));
    }
    if (input.droppedTicks != 0) {
        // catch-up 上限で捨てた tick は表示する。silent に落とすと、重い
        // フレームで時間が飛んだことに気付けない。
        lines.push_back(
            "DROPPED TICKS " + std::to_string(input.droppedTicks));
    }

    if (input.showControls) {
        lines.emplace_back("");
        lines.emplace_back("1 2 3 SELECT CHAIN");
        lines.emplace_back("LEFT CLICK SELECT / CONFIRM");
        lines.emplace_back("RIGHT CLICK OR ESC CANCEL");
        lines.emplace_back("WASD OR MIDDLE DRAG MOVE");
        lines.emplace_back("WHEEL ZOOM");
        lines.emplace_back("F1 TOGGLE THIS HELP");
    }

    for (const std::string& line : lines) {
        requireRenderable(line);
    }
    return lines;
}

}  // namespace konbini::app
