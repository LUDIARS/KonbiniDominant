#pragma once
#include <cstdint>
namespace konbini::sim {
// @implements spec/feature/phase-1-game-loop.md Adjustable baseline
struct Phase1Content {
    std::uint32_t durationTicks = 0;
    std::uint32_t captureDelayTicks = 0;
    std::uint32_t dominationPercent = 0;
    std::uint32_t aiPeriodTicks = 0;
    std::uint32_t aiOpeningPeriodTicks = 0;
    std::uint32_t aiRampTicks = 0;
    std::uint32_t aiTriangleScore = 0;
    std::uint32_t aiEncirclementScore = 0;
    std::uint32_t aiExposurePenalty = 0;
    double triangleMaxEdgeMeters = 0.0;
    double triangleMinAreaSquareMeters = 0.0;
    std::uint32_t triangleInfluence = 0;
    std::uint32_t triangleRevenuePermille = 0;
    std::uint32_t destructionPopulationLossPercent = 0;
    std::uint32_t populationRecoveryPerPeriod = 0;
};
void validatePhase1Content(const Phase1Content& content);
}  // namespace konbini::sim
