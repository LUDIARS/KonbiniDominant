#pragma once
#include <chrono>
#include <fstream>
#include <string_view>
#include "konbini/app/playtest_options.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::app {
class PlaytestReport {
public:
    explicit PlaytestReport(const PlaytestOptions&);
    void observe(const sim::RenderSnapshot&,std::string_view node="manual");
    void restart();
    void presented(const sim::RenderSnapshot&);
private:
    void write(const char*,const sim::RenderSnapshot&);
    std::ofstream output_;
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
    int lastPhase_=-1;
    unsigned run_=1;
    std::string_view node_="idle";
    std::uint64_t lastTick_=0,frames_=0;
    std::uint64_t lastStoreCount_=0;
    bool lastSkillPending_=false;
    bool resultPresented_=false;
};
}
