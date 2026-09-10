#pragma once
#include <string>
#include "konbini/sim/campaign_hud.h"
namespace konbini::app {
std::string skillLabel(sim::SkillId id);
std::string skillDescription(sim::SkillId id, std::uint32_t rank, const sim::SkillContent& rules);
}
