#pragma once
namespace konbini::app::bt {
enum class Status { Failure, Success, Running };
// Reactive priority selector. A waiting child owns this tick; lower-priority
// actions cannot consume resources while an earlier action is still running.
template<class Children,class Tick>
auto selector(const Children& children,Tick tick) -> decltype(tick(children.front())) {
    for(const auto& child:children) {
        auto result=tick(child);
        if(result.status!=Status::Failure) return result;
    }
    return {};
}
}
