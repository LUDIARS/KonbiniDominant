#include "konbini/render/world_palette.h"

#include <cmath>
#include <stdexcept>

// @implements spec/interface/pictor-rendering.md Game-owned render domain

namespace konbini::render {

// BASE-FP-PALETTE-01: first playable の暫定色。chain 差の gameplay 分岐では
// なく presentation 値なので、chain 追加時はここだけを増やす。
// @implements spec/interface/pictor-rendering.md Game-owned render domain
WorldColor chainColor(const sim::ChainId chain, const float alpha) {
    if (!sim::isSimulationChainId(chain) || !std::isfinite(alpha) ||
        alpha < 0.0F || alpha > 1.0F) {
        throw std::invalid_argument("invalid first-playable chain color");
    }
    switch (chain) {
        case sim::ChainId::Aion:
            return {0.72F, 0.20F, 0.96F, alpha};
        case sim::ChainId::Losan:
            return {0.43F, 0.22F, 0.39F, alpha};
        case sim::ChainId::Famoma:
            return {0.91F, 0.39F, 0.32F, alpha};
        case sim::ChainId::SebanIleban:
            return {0.92F, 0.61F, 0.18F, alpha};
    }
    throw std::invalid_argument("unknown first-playable chain");
}

// @implements spec/interface/pictor-rendering.md Game-owned render domain
WorldColor facilityColor(
    const bool isBuildable, const sim::FacilityState state) {
    switch (state) {
        case sim::FacilityState::Intact:
            return isBuildable
                       ? WorldColor{0.55F, 0.64F, 0.58F, 1.0F}
                       : WorldColor{0.37F, 0.41F, 0.46F, 1.0F};
        case sim::FacilityState::Replaced:
            return {0.30F, 0.34F, 0.38F, 1.0F};
        case sim::FacilityState::Destroyed:
            return {0.18F, 0.09F, 0.09F, 0.45F};
    }
    throw std::invalid_argument("unknown facility state");
}

// @implements spec/interface/pictor-rendering.md Game-owned render domain
WorldColor selectedFacilityColor(const float alpha) {
    if (!std::isfinite(alpha) || alpha < 0.0F || alpha > 1.0F) {
        throw std::invalid_argument("invalid selection highlight alpha");
    }
    return {1.0F, 0.72F, 0.16F, alpha};
}

}  // namespace konbini::render
