#include <cstdio>
#include <cstdlib>
#include <exception>
#include <utility>

#include "konbini/app/app_paths.h"
#include "konbini/app/app_runner.h"

// @implements spec/plan/tasks/first-playable.md Build and process boundary

// native entry point.
//
// 依存 / shader / content の欠落はここで明示的に落とす。debug placeholder へ
// 縮退しない (pictor-rendering.md#Failure)。
// @implements spec/plan/tasks/first-playable.md Build and process boundary
// @spec Build and process boundary
int main() {
    try {
        konbini::app::AppRunnerConfig config;
        config.paths = konbini::app::resolveAppPaths(
            konbini::app::readAppPathOverridesFromEnvironment());
        konbini::app::AppRunner runner(std::move(config));
        return runner.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[konbini] startup failed: %s\n", error.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "[konbini] startup failed: unknown error\n");
        return EXIT_FAILURE;
    }
}
