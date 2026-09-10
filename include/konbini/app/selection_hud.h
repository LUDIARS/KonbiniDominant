#pragma once
#include "konbini/app/hud_text_model.h"
namespace konbini::app {
HudTextInput makeSelectionHud(const sim::RenderSnapshot& snapshot,const sim::FirstPlayableContent& content,
    std::optional<sim::FacilityId> selected,std::uint32_t floor);
}
