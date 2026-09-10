#pragma once
#include <filesystem>
#include <memory>
#include "konbini/app/playtest_options.h"
#include "konbini/app/frame_input.h"
#include "konbini/render/viewport_extent.h"
#include "konbini/render/prepared_frame.h"
#include "konbini/city/i_city_generator.h"
namespace konbini::adapters::ergo { class WorldFrameGraph; }
namespace konbini::app {
// Platform hosts share simulation and CPU presentation; native overloads publish
// the prepared frame to their Pictor-backed graph.
class GameSession {
public:
    explicit GameSession(const std::filesystem::path& contentFile,std::uint32_t maxTicksPerFrame=5,PlaytestOptions playtest={});
    GameSession(const std::filesystem::path& contentFile,
                const city::ICityGenerator& cityGenerator,
                std::uint32_t maxTicksPerFrame=5, PlaytestOptions playtest={});
    ~GameSession();
    [[nodiscard]] render::PreparedFrame frame(FrameInput input, render::ViewportExtent extent);
    [[nodiscard]] const city::GeneratedCity& city() const noexcept;
    void uploadGeometry(adapters::ergo::WorldFrameGraph& graph);
    void frame(FrameInput input,render::ViewportExtent extent,adapters::ergo::WorldFrameGraph& graph);
    void suspend() noexcept;
    void notifyPresented();
    [[nodiscard]] std::uint64_t completedTicks() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
