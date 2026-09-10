#include "figmentum_mesh_cleanup.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
// @implements spec/interface/figmentum-city-generation.md Geometry generation
namespace konbini::adapters::figmentum {
namespace {
sim::Vec3 subtract(const sim::Vec3 a,const sim::Vec3 b) noexcept {return {a.x-b.x,a.y-b.y,a.z-b.z};}
sim::Vec3 cross(const sim::Vec3 a,const sim::Vec3 b) noexcept {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
double squaredLength(const sim::Vec3 a) noexcept {return a.x*a.x+a.y*a.y+a.z*a.z;}
void add(sim::Vec3& a,const sim::Vec3 b) noexcept {a.x+=b.x;a.y+=b.y;a.z+=b.z;}
}
MeshCleanupReport finalizeFacilityMesh(city::FacilityGeometry& geometry) {
    if(geometry.indices.empty() || geometry.indices.size()%3!=0)
        throw std::runtime_error("Figmentum mesh has no complete triangles");
    const auto& positions=geometry.positionsMeters;
    for(const auto& position:positions) if(!sim::isFinite(position))
        throw std::runtime_error("Figmentum mesh has a non-finite vertex");
    std::vector<std::uint32_t> retained;
    retained.reserve(geometry.indices.size());
    std::vector<sim::Vec3> normals(positions.size());
    std::vector<bool> used(positions.size(),false);
    MeshCleanupReport report;
    for(std::size_t i=0;i<geometry.indices.size();i+=3) {
        const auto a=geometry.indices[i],b=geometry.indices[i+1],c=geometry.indices[i+2];
        if(a>=positions.size() || b>=positions.size() || c>=positions.size())
            throw std::runtime_error("Figmentum mesh has an out-of-range index");
        const auto normal=cross(subtract(positions[b],positions[a]),subtract(positions[c],positions[a]));
        const double lengthSquared=squaredLength(normal);
        if(!sim::isFinite(normal) || !std::isfinite(lengthSquared))
            throw std::runtime_error("Figmentum mesh has a non-finite triangle");
        // Marching cubes snaps intersections onto grid corners. A triangle with
        // coincident corners has no surface to render and must not create normals.
        if(lengthSquared==0) {++report.degenerateTriangles;continue;}
        retained.insert(retained.end(),{a,b,c});
        used[a]=used[b]=used[c]=true;
        add(normals[a],normal);add(normals[b],normal);add(normals[c],normal);
    }
    if(retained.empty())
        throw std::runtime_error("Figmentum mesh has no non-degenerate triangles");
    std::vector<sim::Vec3> compactPositions,compactNormals;
    std::vector<std::uint32_t> remap(positions.size());
    compactPositions.reserve(positions.size());compactNormals.reserve(positions.size());
    for(std::size_t i=0;i<positions.size();++i) {
        if(!used[i]) {++report.unusedVertices;continue;}
        const double lengthSquared=squaredLength(normals[i]);
        if(!std::isfinite(lengthSquared) || lengthSquared<=0)
            throw std::runtime_error("Figmentum mesh has cancelling vertex normals");
        const double inverse=1/std::sqrt(lengthSquared);
        if(compactPositions.size()>=std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("Figmentum mesh exceeds the vertex index range");
        remap[i]=static_cast<std::uint32_t>(compactPositions.size());
        compactPositions.push_back(positions[i]);
        compactNormals.push_back({normals[i].x*inverse,normals[i].y*inverse,normals[i].z*inverse});
    }
    for(auto& index:retained) index=remap[index];
    geometry.positionsMeters=std::move(compactPositions);
    geometry.normals=std::move(compactNormals);
    geometry.indices=std::move(retained);
    return report;
}
}
