#pragma once
#include <filesystem>
namespace konbini::app {
struct PlaytestOptions {
    double timeScale=1.0;
    bool autoplay=false;
    std::filesystem::path report;
};
PlaytestOptions readPlaytestOptions(const std::filesystem::path& file);
}
