#include "konbini/app/playtest_report.h"
#include <stdexcept>
namespace konbini::app {
PlaytestReport::PlaytestReport(const PlaytestOptions& options) {
    if(options.report.empty()) return;
    output_.open(options.report,std::ios::trunc);
    if(!output_) throw std::runtime_error("cannot create playtest report");
    output_<<"{\"event\":\"configuration\",\"timeScale\":"<<options.timeScale
           <<",\"autoplay\":"<<(options.autoplay?"true":"false")<<"}\n"<<std::flush;
}
void PlaytestReport::restart() {
    started_=std::chrono::steady_clock::now();lastPhase_=-1;lastTick_=0;frames_=0;resultPresented_=false;
    lastStoreCount_=0;lastSkillPending_=false;++run_;
}
void PlaytestReport::write(const char* event,const sim::RenderSnapshot& snapshot) {
    const auto& h=snapshot.hud();
    output_<<"{\"event\":\""<<event<<"\",\"tick\":"<<h.completedTicks
        <<",\"run\":"<<run_<<",\"phase\":"<<static_cast<int>(h.phase)<<",\"outcome\":"<<static_cast<int>(h.outcome)
        <<",\"elapsedTicks\":"<<h.campaign.elapsedTicks<<",\"ticksPerSecond\":"<<h.ticksPerSecond
        <<",\"endReason\":"<<static_cast<int>(h.endReason)<<",\"btNode\":\""<<node_<<"\""
        <<",\"skillPending\":"<<(h.campaign.skills.pending?"true":"false")
        <<",\"stores\":"<<h.storeCount<<",\"cash\":"<<h.cashCredits
        <<",\"foreignDestroyed\":"<<h.campaign.destroyedForeign
        <<",\"framesPresented\":"<<frames_<<",\"wallSeconds\":"
        <<std::chrono::duration<double>(std::chrono::steady_clock::now()-started_).count()<<"}\n"<<std::flush;
    if(!output_) throw std::runtime_error("cannot write playtest report");
}
void PlaytestReport::observe(const sim::RenderSnapshot& snapshot,std::string_view node) {
    node_=node;
    if(!output_.is_open()) return;
    const auto phase=static_cast<int>(snapshot.hud().phase);
    const auto& hud=snapshot.hud();
    const bool changed=hud.storeCount!=lastStoreCount_ || hud.campaign.skills.pending!=lastSkillPending_;
    if(phase!=lastPhase_ || changed || snapshot.completedTicks()>=lastTick_+100) {
        write(phase!=lastPhase_?"phase":changed?"state":"progress",snapshot);
        lastPhase_=phase;lastTick_=snapshot.completedTicks();
        lastStoreCount_=hud.storeCount;lastSkillPending_=hud.campaign.skills.pending;
    }
}
void PlaytestReport::presented(const sim::RenderSnapshot& snapshot) {
    if(!output_.is_open()) return;
    ++frames_;
    if(snapshot.hud().phase==sim::GamePhase::Result && !resultPresented_) {
        write("resultPresented",snapshot);resultPresented_=true;
    }
}
}
