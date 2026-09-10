#include "konbini/app/game_session.h"
#include "konbini/adapters/ergo/world_frame_graph.h"
#include "konbini/adapters/figmentum/figmentum_city_adapter.h"
#include "konbini/adapters/pictor/world_geometry_loader.h"
#include "konbini/render/world_render_layer.h"
#include "konbini/render/hud_overlay_layer.h"
#include <cstdio>
#include <utility>

namespace konbini::app {
GameSession::GameSession(const std::filesystem::path& file,
                         const std::uint32_t maxTicks, PlaytestOptions playtest)
    : GameSession(file, adapters::figmentum::FigmentumCityAdapter{},
                  maxTicks, std::move(playtest)) {}

void GameSession::uploadGeometry(adapters::ergo::WorldFrameGraph& graph) {
    const auto report = adapters::pictor::loadCityGeometry(city(), graph.geometryCache());
    std::fprintf(stdout, "[konbini] uploaded %zu facility meshes (%zu shared)\n",
                 report.uploaded, report.deduplicated);
}
void GameSession::frame(FrameInput input, const render::ViewportExtent extent,
                        adapters::ergo::WorldFrameGraph& graph) {
    auto prepared = frame(std::move(input), extent);
    graph.worldLayer().publishFrame(prepared.camera, std::move(prepared.world));
    graph.hudLayer().publishFrame(std::move(prepared.hud), extent);
}
}
