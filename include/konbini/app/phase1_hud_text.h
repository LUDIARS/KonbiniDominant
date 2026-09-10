#pragma once
#include <string>
#include <vector>
#include "konbini/app/hud_text_model.h"
namespace konbini::app {
[[nodiscard]] std::vector<std::string> buildPhase1HudLines(const HudTextInput& input);
}  // namespace konbini::app
