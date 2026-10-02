#pragma once

#include "konbini/render/world_palette.h"

#ifndef NOGDI
#define NOGDI
#endif
#include "pictor/core/types.h"

// @implements spec/interface/pictor-rendering.md Presentation objects

namespace konbini::adapters::pictor {

// Per-instance tint of the Pictor objects one sync owns. The batch plan asks
// every owner; an object no owner knows is a missing mapping, never white.
class IObjectTintSource {
public:
    virtual ~IObjectTintSource() = default;

    // nullptr when this owner did not register `object`.
    [[nodiscard]] virtual const render::WorldColor* tintFor(
        ::pictor::ObjectId object) const noexcept = 0;
};

}  // namespace konbini::adapters::pictor
