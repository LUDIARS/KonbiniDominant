#include "konbini/sim/dominant_triangle.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <stdexcept>

namespace konbini::sim {
namespace {
struct Point { double x; double z; };
struct Face { std::array<std::size_t, 3> vertices; };
double orientation(const Point a, const Point b, const Point c) noexcept {
    return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
}
double orientation(const Vec3 a, const Vec3 b, const Vec3 c) noexcept {
    return orientation(Point{a.x, a.z}, Point{b.x, b.z}, Point{c.x, c.z});
}
double distanceSquared(const Vec3 a, const Vec3 b) noexcept {
    return (a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z);
}
// All faces are counterclockwise in normalized XZ coordinates. Inserting in
// stable ID order and treating cocircular points consistently fixes the diagonal.
bool circumcircleContains(const Face& face, const std::vector<Point>& points,
                          const Point p) noexcept {
    const Point a{points[face.vertices[0]].x - p.x, points[face.vertices[0]].z - p.z};
    const Point b{points[face.vertices[1]].x - p.x, points[face.vertices[1]].z - p.z};
    const Point c{points[face.vertices[2]].x - p.x, points[face.vertices[2]].z - p.z};
    const double determinant =
        (a.x*a.x + a.z*a.z) * (b.x*c.z - b.z*c.x) -
        (b.x*b.x + b.z*b.z) * (a.x*c.z - a.z*c.x) +
        (c.x*c.x + c.z*c.z) * (a.x*b.z - a.z*b.x);
    return determinant >= -1e-12;
}
std::vector<Face> triangulate(const std::vector<StoreRow>& rows) {
    double minX = rows.front().positionMeters.x, maxX = minX;
    double minZ = rows.front().positionMeters.z, maxZ = minZ;
    for (const auto& row : rows) {
        if (std::abs(row.positionMeters.x) > 1e9 || std::abs(row.positionMeters.z) > 1e9) {
            throw std::invalid_argument("triangle coordinates exceed the city domain");
        }
        minX = std::min(minX, row.positionMeters.x);
        maxX = std::max(maxX, row.positionMeters.x);
        minZ = std::min(minZ, row.positionMeters.z);
        maxZ = std::max(maxZ, row.positionMeters.z);
    }
    const double span = std::max(maxX - minX, maxZ - minZ);
    if (span == 0.0) { return {}; }
    std::vector<Point> points;
    for (const auto& row : rows) {
        points.push_back({(row.positionMeters.x - minX) / span,
                          (row.positionMeters.z - minZ) / span});
    }
    const std::size_t count = rows.size();
    points.insert(points.end(), {{-32.0, -16.0}, {32.0, -16.0}, {0.0, 32.0}});
    std::vector<Face> faces{{{count, count + 1, count + 2}}};
    for (std::size_t next = 0; next < count; ++next) {
        std::map<std::pair<std::size_t, std::size_t>, unsigned> edges;
        std::vector<Face> surviving;
        for (const Face& face : faces) {
            if (!circumcircleContains(face, points, points[next])) {
                surviving.push_back(face);
                continue;
            }
            for (std::size_t edge = 0; edge < 3; ++edge) {
                const auto a = face.vertices[edge], b = face.vertices[(edge + 1) % 3];
                ++edges[{std::min(a, b), std::max(a, b)}];
            }
        }
        for (const auto& [edge, uses] : edges) {
            if (uses != 1) { continue; }
            auto a = edge.first, b = edge.second;
            const double area = orientation(points[a], points[b], points[next]);
            if (std::abs(area) <= 1e-14) { continue; }
            if (area < 0.0) { std::swap(a, b); }
            surviving.push_back({{a, b, next}});
        }
        faces = std::move(surviving);
    }
    std::erase_if(faces, [count](const Face& face) {
        return std::ranges::any_of(face.vertices, [count](auto index) { return index >= count; });
    });
    return faces;
}
void appendGroup(const std::vector<StoreRow>& group, const Phase1Content& rules,
                 std::vector<DominantTriangle>& output) {
    // Coincident sites have one geometric representative (lowest stable ID).
    std::vector<StoreRow> sites;
    for (const auto& row : group) {
        if (std::ranges::none_of(sites, [&](const auto& site) {
            return site.positionMeters.x == row.positionMeters.x &&
                   site.positionMeters.z == row.positionMeters.z;
        })) { sites.push_back(row); }
    }
    if (sites.size() < 3) { return; }
    const double maxEdgeSquared = rules.triangleMaxEdgeMeters * rules.triangleMaxEdgeMeters;
    for (auto face : triangulate(sites)) {
        std::sort(face.vertices.begin(), face.vertices.end());
        const auto& a = sites[face.vertices[0]];
        const auto& b = sites[face.vertices[1]];
        const auto& c = sites[face.vertices[2]];
        if (std::abs(orientation(a.positionMeters, b.positionMeters, c.positionMeters)) / 2.0 <
                rules.triangleMinAreaSquareMeters ||
            distanceSquared(a.positionMeters, b.positionMeters) > maxEdgeSquared ||
            distanceSquared(b.positionMeters, c.positionMeters) > maxEdgeSquared ||
            distanceSquared(c.positionMeters, a.positionMeters) > maxEdgeSquared) { continue; }
        output.push_back({{a.id, b.id, c.id},
                          {a.positionMeters, b.positionMeters, c.positionMeters},
                          a.chain, a.dimension});
    }
}
}  // namespace

// Boundary points are included for both influence and encirclement.
// @implements spec/feature/phase-1-game-loop.md Match contract
bool triangleContains(const DominantTriangle& triangle, const Vec3 point) noexcept {
    if (orientation(triangle.points[0], triangle.points[1], triangle.points[2]) == 0.0) { return false; }
    const double a = orientation(triangle.points[0], triangle.points[1], point);
    const double b = orientation(triangle.points[1], triangle.points[2], point);
    const double c = orientation(triangle.points[2], triangle.points[0], point);
    return (a >= 0.0 && b >= 0.0 && c >= 0.0) || (a <= 0.0 && b <= 0.0 && c <= 0.0);
}
std::vector<DominantTriangle> buildDominantTriangles(
    const StoreTable& stores, const Phase1Content& rules) {
    std::vector<StoreRow> rows;
    for (std::size_t index = 0; index < stores.size(); ++index) {
        const auto row = stores.row(index);
        if (row.isActive) { rows.push_back(row); }
    }
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return std::tuple{a.dimension, a.chain, a.id.value()} <
               std::tuple{b.dimension, b.chain, b.id.value()};
    });
    std::vector<DominantTriangle> triangles;
    for (std::size_t start = 0; start < rows.size();) {
        std::size_t end = start + 1;
        while (end < rows.size() && rows[end].chain == rows[start].chain &&
               rows[end].dimension == rows[start].dimension) { ++end; }
        appendGroup(std::vector<StoreRow>(rows.begin() + static_cast<std::ptrdiff_t>(start),
                                          rows.begin() + static_cast<std::ptrdiff_t>(end)),
                    rules, triangles);
        start = end;
    }
    std::sort(triangles.begin(), triangles.end(), [](const auto& a, const auto& b) {
        return std::tuple{a.dimension, a.chain, a.stores} < std::tuple{b.dimension, b.chain, b.stores};
    });
    return triangles;
}
void applyTriangleRevenue(StoreTable& stores,
                          const std::span<const DominantTriangle> triangles,
                          const Phase1Content& rules) {
    for (std::size_t i = 0; i < stores.size(); ++i) { stores.setRevenuePermille(i, 1000); }
    for (const auto& triangle : triangles) {
        for (const auto id : triangle.stores) {
            const auto index = stores.find(id);
            if (!index || !stores.row(*index).isActive) {
                throw std::logic_error("triangle references an inactive store");
            }
            stores.setRevenuePermille(*index, rules.triangleRevenuePermille);
        }
    }
}
}  // namespace konbini::sim
