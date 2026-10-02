#pragma once
#include <string>
#include <vector>
#include "konbini/app/hud_text_model.h"
// @implements spec/feature/ui-ux.md Smartphone interaction
namespace konbini::app {
// Touch has no hover. What the desktop shows on lot hover (placeability,
// cost, the store already there) is read from the selection state only,
// never from the pointer position, so the same lines appear for mouse and
// touch and survive a rotation.
[[nodiscard]] std::vector<std::string> buildSelectionPreviewLines(const HudTextInput& input);
// The next-step hint. Touch taps never place, so it points at Place.
[[nodiscard]] std::string placementHintLine(const HudTextInput& input);
}
