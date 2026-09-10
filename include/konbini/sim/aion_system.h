#pragma once
#include "konbini/sim/campaign_world.h"
#include "konbini/sim/dominant_triangle.h"
namespace konbini::sim {
void advanceAionInvasion(CampaignWorld world);
void resolveAionMaxValue(CampaignWorld world,std::span<const DominantTriangle> triangles);
}
