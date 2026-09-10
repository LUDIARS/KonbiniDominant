#pragma once
#include <array>
#include <cstdint>
#include "konbini/app/pointer_sample.h"
namespace konbini::app {
enum class TouchPhase { Down, Move, Up, Cancel };
class TouchContacts {
public:
    void update(std::uint64_t id,TouchPhase phase,double x,double y);
    void cancel() noexcept;
    PointerSample consume() noexcept;
private:
    struct Contact {std::uint64_t id=0;double x=0,y=0;bool used=false;};
    struct Position {double x=0,y=0,distance=0;std::uint64_t first=0,second=0;unsigned count=0;};
    Position position() const noexcept;
    std::array<Contact,32> contacts_{};
    PointerSample pending_;
    bool multiple_=false;
};
}
