#pragma once
#include <filesystem>
namespace pictor { class VulkanContext; }
namespace konbini::adapters::pictor { class WorldSceneTargets; }
namespace konbini::gallery {
// Exports the last completed world image, without HUD, as an sRGB P6 PPM.
void exportFrame(::pictor::VulkanContext& context,
                 const adapters::pictor::WorldSceneTargets& targets,
                 const std::filesystem::path& path);
}
