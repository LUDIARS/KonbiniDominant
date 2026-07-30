#pragma once

#include <cstddef>
#include <vector>

// @implements spec/data/world-state.md Dense tables

namespace konbini::sim {

// Reserves capacity ahead of a single push_back so that the per-column pushes
// of one row cannot fail half-way with bad_alloc. Calling reserve(size() + 1)
// directly would request an exact capacity and therefore reallocate on every
// append, turning a dense-table fill into O(n^2) copies; grow geometrically
// instead and only when the column is actually full.
// @implements spec/data/world-state.md Dense tables
template <typename T>
void reserveForAppend(std::vector<T>& column, const std::size_t nextSize) {
    if (column.capacity() >= nextSize) {
        return;
    }
    const std::size_t doubled = column.capacity() * 2;
    column.reserve(doubled > nextSize ? doubled : nextSize);
}

}  // namespace konbini::sim
