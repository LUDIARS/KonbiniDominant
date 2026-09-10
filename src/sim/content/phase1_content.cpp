#include "konbini/sim/phase1_content.h"
#include <cmath>
#include <stdexcept>
namespace konbini::sim {
// Limits bound numeric work as well as preventing invalid match profiles.
// @implements spec/feature/phase-1-game-loop.md Adjustable baseline
void validatePhase1Content(const Phase1Content& content) {
    if (content.durationTicks == 0 || content.captureDelayTicks == 0 ||
        content.captureDelayTicks >= content.durationTicks ||
        content.dominationPercent == 0 || content.dominationPercent > 100 ||
        content.aiPeriodTicks == 0 || content.aiPeriodTicks >= content.durationTicks ||
        content.aiOpeningPeriodTicks < content.aiPeriodTicks ||
        content.aiOpeningPeriodTicks >= content.durationTicks ||
        content.aiRampTicks == 0 || content.aiRampTicks > content.durationTicks ||
        content.aiTriangleScore == 0 || content.aiEncirclementScore == 0 ||
        content.aiExposurePenalty == 0 ||
        !std::isfinite(content.triangleMaxEdgeMeters) ||
        content.triangleMaxEdgeMeters <= 0.0 || content.triangleMaxEdgeMeters > 1000000.0 ||
        !std::isfinite(content.triangleMinAreaSquareMeters) ||
        content.triangleMinAreaSquareMeters <= 0.0 ||
        content.triangleMinAreaSquareMeters >=
            content.triangleMaxEdgeMeters * content.triangleMaxEdgeMeters / 2.0 ||
        content.triangleInfluence == 0 || content.triangleInfluence > 1000 ||
        content.triangleRevenuePermille < 1000 || content.triangleRevenuePermille > 10000 ||
        content.destructionPopulationLossPercent > 100 ||
        content.populationRecoveryPerPeriod == 0) {
        throw std::invalid_argument("invalid phase1 match rules");
    }
}
}  // namespace konbini::sim
