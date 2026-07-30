#pragma once

#include "konbini/sim/math_types.h"

namespace konbini::render {

struct WorldRay {
    sim::Vec3 originMeters{};
    sim::Vec3 direction{};
};

}  // namespace konbini::render
