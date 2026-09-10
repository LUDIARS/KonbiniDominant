#pragma once
#include <cstddef>
#include "konbini/city/facility_geometry.h"
namespace konbini::adapters::figmentum {
struct MeshCleanupReport {
    std::size_t degenerateTriangles=0, unusedVertices=0;
};
// Remove zero-area triangles before normalizing; corrupt/empty meshes still fail.
MeshCleanupReport finalizeFacilityMesh(city::FacilityGeometry& geometry);
}
