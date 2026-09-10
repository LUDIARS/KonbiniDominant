#include "browser_input.h"
#include "campaign_city_generator.h"
#include "pictor_web_renderer.h"
#include "konbini/app/game_session.h"
#include "konbini/city/grid_town.h"
#include <emscripten.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
EM_JS(int, autoplayRequested, (), {
    return new URLSearchParams(location.search).get('autoplay') === '1' ? 1 : 0;
});
EM_JS(int, speedRequested, (), {
    return new URLSearchParams(location.search).get('speed') === '100' ? 100 : 1;
});
EM_JS(void, showFailure, (const char* message), {
    window.konbiniFailure(UTF8ToString(message));
});
EM_JS(void, ready, (), { window.konbiniReady(); });
struct BrowserGame {
    explicit BrowserGame()
        : game("/data/content/first-playable.json", generator, 100,
            {static_cast<double>(speedRequested()), autoplayRequested() != 0, "/playtest-report.jsonl"}) {}
    konbini::web::CampaignCityGenerator generator;
    konbini::app::GameSession game;
    konbini::web::PictorWebRenderer renderer;
    konbini::web::BrowserInput input;
    double previous = 0;
    bool suspended = false, failed = false;
};
std::unique_ptr<BrowserGame> browser;
void fail(const char* message) {
    if (browser) { browser->failed = true; browser->input.cancel(); browser->game.suspend(); }
    showFailure(message);
}
void frame(void*) {
    if (!browser || browser->failed) return;
    const double now = emscripten_get_now() / 1000.0;
    const double dt = browser->previous > 0 ? std::clamp(now - browser->previous, 0.0, 0.1) : 0;
    browser->previous = now;
    if (browser->suspended) return;
    try {
        auto prepared = browser->game.frame(browser->input.consume(dt), {1280, 720});
        browser->renderer.draw(prepared);
        browser->game.notifyPresented();
    } catch (const std::exception& error) { fail(error.what()); }
}
}
extern "C" {
EMSCRIPTEN_KEEPALIVE void kd_pointer(int id, int phase, double x, double y, int touch) {
    if (!browser || browser->failed || browser->suspended) return;
    try { browser->input.pointer(id, phase, x, y, touch != 0); }
    catch (const std::exception& error) { fail(error.what()); }
}
EMSCRIPTEN_KEEPALIVE void kd_wheel(double steps) {
    if (browser && !browser->failed && !browser->suspended) browser->input.wheel(steps);
}
EMSCRIPTEN_KEEPALIVE void kd_suspend(int value) {
    if (!browser) return;
    browser->suspended = value != 0;
    browser->previous = 0;
    browser->input.cancel();
    browser->game.suspend();
}
EMSCRIPTEN_KEEPALIVE double kd_ticks() {
    return browser ? static_cast<double>(browser->game.completedTicks()) : 0;
}
EMSCRIPTEN_KEEPALIVE void kd_context_lost() { fail("Graphics interrupted. Reload to restart."); }
}
int main() {
    try {
        browser = std::make_unique<BrowserGame>();
        ready();
        emscripten_set_main_loop_arg(frame, nullptr, 0, false);
        return 0;
    } catch (const std::exception& error) {
        fail(error.what());
        return 1;
    }
}
