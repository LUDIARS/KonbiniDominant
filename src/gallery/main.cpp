#include <array>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "frame_export.h"
#include "reference_store_geometry.h"

#include "konbini/adapters/ergo/render_device_host.h"
#include "konbini/adapters/ergo/world_frame_graph.h"
#include "konbini/render/hud_overlay_layer.h"
#include "konbini/render/hud_text_geometry.h"
#include "konbini/render/store_marker_geometry.h"
#include "konbini/render/store_tower_geometry.h"
#include "konbini/render/world_render_layer.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// @implements spec/feature/three-store-brands.md Comparison delivery
// A presentation-only scene: no simulation, Figmentum generation or game saves.
int main() {
    try {
        namespace kd = konbini;
        kd::adapters::ergo::RenderDeviceHost device;
        kd::adapters::ergo::RenderDeviceConfig config;
        config.windowWidth = 1600;
        config.windowHeight = 900;
        config.windowTitle = "KD - Original Store Gallery - Pictor";
        config.shaderDirectory = KONBINI_GALLERY_SHADER_DIR;
        device.initialize(config);
        // Declared after the device: graph resources are destroyed first on
        // success, close, device loss, and every exception path.
        kd::adapters::ergo::WorldFrameGraph graph;
        graph.initialize(device);
        constexpr std::array chains{kd::sim::ChainId::SebanIleban,
                                    kd::sim::ChainId::Losan,
                                    kd::sim::ChainId::Famoma};
        constexpr std::array names{"DAYLARK", "MOONPANTRY", "SUNFOLD"};
        int selected = 0;
        unsigned int renderedFrames = 0;
        while (!device.shouldClose()) {
            device.pollEvents();
            if (glfwGetKey(device.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
            const auto windowExtent = device.framebufferExtent();
            if (windowExtent.width == 0 || windowExtent.height == 0) {
                device.waitEvents(0.05);
                continue;
            }
            if (windowExtent != device.swapchainExtent()) graph.requestRebuild();
            if (graph.rebuildPending()) {
                const auto outcome = graph.runFrame(0.0F);
                if (kd::adapters::ergo::isFatal(outcome)) {
                    throw std::runtime_error("gallery swapchain rebuild failed");
                }
                continue;
            }
            const auto extent = graph.extent();
            const bool isLineup = selected == 0 || selected == 4;
            const std::string sceneName = selected == 5 ? "TOWER-30"
                : selected == 6 ? "TOWER-DETAIL" : selected == 4 ? "FABLE-CURRENT"
                : selected == 0 ? "lineup" : names[selected - 1];
            std::vector<kd::sim::RenderStore> stores;
            for (std::uint32_t i = 0; i < chains.size(); ++i) {
                if (!isLineup && selected != static_cast<int>(i) + 1) continue;
                stores.push_back({
                    .id = {{i, 1}}, .facilityId = {{i, 1}}, .chain = chains[i],
                    .positionMeters = {isLineup ? (static_cast<double>(i) - 1.0) * 8.0 : 0.0, 0, 0},
                });
            }
            kd::render::IsometricCameraConfig cameraConfig;
            cameraConfig.targetMeters = {0, 1.4, 0};
            cameraConfig.azimuthDegrees = 72.0;
            cameraConfig.elevationDegrees = 22.0;
            cameraConfig.verticalSpanMeters = isLineup ? 17.0 : 8.0;
            kd::render::WorldDrawList drawList;
            if (selected >= 5) {
                const kd::render::StoreTowerSpec tower;
                const double totalHeight = tower.floorCount * tower.floor.heightMeters;
                cameraConfig.targetMeters = {0.5, selected == 5 ? totalHeight * 0.5 : totalHeight - 7.0, 0};
                cameraConfig.verticalSpanMeters = selected == 5 ? totalHeight + 12.0 : 36.0;
                if (selected == 6) cameraConfig.targetMeters.y = totalHeight - 14.0;
                cameraConfig.elevationDegrees = selected == 5 ? 8.0 : 15.0;
                drawList.storeMesh = kd::render::buildStoreTowerGeometry({}, tower);
            } else if (selected == 4) {
                drawList.overlayMesh = kd::gallery::buildReferenceStores(stores);
            } else {
                drawList.storeMesh = kd::render::buildStoreMarkerGeometry(
                    stores, kd::render::defaultStoreMarkerSpec());
            }
            graph.worldLayer().publishFrame(
                kd::render::buildIsometricCamera(cameraConfig, extent), std::move(drawList));
            const std::vector<std::string> labels{
                selected == 0 ? "LINEUP" : sceneName,
                "AUTOMATIC PICTOR CAPTURE / ESC CLOSE",
            };
            auto style = kd::render::defaultHudTextStyle();
            style.glyphPixelScale = 2.0F;
            graph.hudLayer().publishFrame(kd::render::buildHudTextMesh(labels, style, extent), extent);
            const auto outcome = graph.runFrame(1.0F / 60.0F);
            if (kd::adapters::ergo::isFatal(outcome)) {
                throw std::runtime_error(kd::adapters::ergo::describeFrameOutcome(outcome));
            }
            if (++renderedFrames % 8U == 0) {
                kd::gallery::exportFrame(device.vulkan(), graph.sceneTargets(),
                    std::filesystem::path(KONBINI_GALLERY_OUTPUT_DIR) / (sceneName + ".ppm"));
                if (++selected == 7) break;
                if (selected == 5) glfwSetWindowSize(device.window(), 900, 1600);
                if (selected == 6) glfwSetWindowSize(device.window(), 1600, 900);
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[store-gallery] %s\n", error.what());
        return 1;
    }
}
