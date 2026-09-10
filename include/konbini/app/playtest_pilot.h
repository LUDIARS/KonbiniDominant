#pragma once
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include "konbini/app/behavior_tree.h"
#include "konbini/app/command_composer.h"
#include "konbini/sim/render_snapshot.h"
namespace konbini::app {
// Deterministic behavior tree. No network, model, or authoritative-state access.
class PlaytestPilot {
public:
    std::optional<sim::PlayerCommand> next(const sim::RenderSnapshot&,CommandComposer&);
    std::string_view activeNode() const noexcept {return activeNode_;}
private:
    struct Context {
        const sim::RenderSnapshot& snapshot;
        CommandComposer& commands;
        std::uint64_t anchor(sim::FacilityId) const;
        bool occupied(sim::FacilityId,std::uint32_t) const;
    };
    struct Decision {
        bt::Status status=bt::Status::Failure;
        std::optional<sim::PlayerCommand> command;
        Decision()=default;
        Decision(bt::Status value):status(value) {}
        Decision(sim::PlayerCommand value):status(bt::Status::Success),command(std::move(value)) {}
    };
    Decision selectChain(const Context&);
    Decision chooseSkill(const Context&);
    Decision escapeBoss(const Context&);
    Decision conquerForeign(const Context&);
    Decision expandStores(const Context&);
    std::set<std::pair<std::uint64_t,std::uint32_t>> originStores_;
    std::optional<std::pair<std::uint64_t,std::uint32_t>> neededOrigin_;
    std::set<std::pair<std::uint64_t,std::uint32_t>> blockedOrigin_;
    std::uint32_t foreign_=0;
    std::string_view activeNode_="idle";
};
}
