#pragma once
#include <filesystem>
#include <memory>
#include "konbini/app/playtest_options.h"
#include "konbini/app/frame_input.h"
#include "konbini/render/viewport_extent.h"
namespace konbini::adapters::ergo { class WorldFrameGraph; }
namespace konbini::app {
// Platform hosts supply pointer input and a Pictor-backed frame graph.
class GameSession {
public:
    explicit GameSession(const std::filesystem::path& contentFile,std::uint32_t maxTicksPerFrame=5,PlaytestOptions playtest={});
    ~GameSession();
    void uploadGeometry(adapters::ergo::WorldFrameGraph& graph);
    void frame(FrameInput input,render::ViewportExtent extent,adapters::ergo::WorldFrameGraph& graph);
    void suspend() noexcept;
    void notifyPresented();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
