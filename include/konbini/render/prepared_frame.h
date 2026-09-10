#pragma once
#include "konbini/render/isometric_camera.h"
#include "konbini/render/world_draw_list.h"

namespace konbini::render {
// CPU presentation shared by Pictor Vulkan and Pictor WebGL2 hosts.
struct PreparedFrame {
    IsometricCamera camera;
    WorldDrawList world;
    WorldMesh hud;
};
}
