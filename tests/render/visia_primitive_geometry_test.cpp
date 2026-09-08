#include "konbini/render/visia_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/interface/visia-presentation.md CPU primitive geometry
// @implements spec/interface/visia-presentation.md First playable definitions

namespace {

using konbini::render::BoxPrimitiveDefinition;
using konbini::render::ResidentPrimitiveVisiaDefinition;
using konbini::render::StoreLandingEffectPrimitiveVisiaDefinition;
using konbini::render::VisiaDefinition;
using konbini::render::VisiaGeometry;
using konbini::render::VisiaId;
using konbini::render::VisiaInstance;
using konbini::render::VisiaPose;
using konbini::render::VisiaPrimitiveKind;
using konbini::render::buildPrimitiveVisiaGeometry;
using konbini::render::buildResidentPrimitiveGeometry;
using konbini::render::buildStoreLandingEffectPrimitiveGeometry;
using konbini::render::residentPrimitiveVisia;
using konbini::render::storeLandingEffectPrimitiveVisia;
using konbini::render::visiaDefinition;
using konbini::sim::Vec3;
using konbini::test::approxEqual;

// A resident is two boxes; a box is six quads; a quad is four unshared
// vertices and two triangles. Sharing vertices between faces would break the
// per-face normal, so the counts are part of the contract.
constexpr std::size_t kBoxVertices = 6 * 4;
constexpr std::size_t kBoxIndices = 6 * 6;
constexpr std::size_t kResidentVertices = 2 * kBoxVertices;
constexpr std::size_t kResidentIndices = 2 * kBoxIndices;

[[nodiscard]] Vec3 positionOf(const VisiaGeometry& geometry,
                              const std::size_t index) {
    return {
        static_cast<double>(geometry.vertices[index].position[0]),
        static_cast<double>(geometry.vertices[index].position[1]),
        static_cast<double>(geometry.vertices[index].position[2]),
    };
}

[[nodiscard]] Vec3 normalOf(const VisiaGeometry& geometry,
                            const std::size_t index) {
    return {
        static_cast<double>(geometry.vertices[index].normal[0]),
        static_cast<double>(geometry.vertices[index].normal[1]),
        static_cast<double>(geometry.vertices[index].normal[2]),
    };
}

[[nodiscard]] bool everyIndexIsInRange(const VisiaGeometry& geometry) {
    for (const std::uint32_t index : geometry.indices) {
        if (static_cast<std::size_t>(index) >= geometry.vertices.size()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool everyVertexIsFinite(const VisiaGeometry& geometry) {
    for (const auto& vertex : geometry.vertices) {
        for (const float value : vertex.position) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        for (const float value : vertex.normal) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        for (const float value : vertex.color) {
            if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
                return false;
            }
        }
    }
    return true;
}

// Winding is the only thing that tells a back-face-culling pipeline which
// side of a triangle is outside. Testing it as "the triangle's own winding
// agrees with the normal it carries" keeps the assertion independent of the
// order the faces happen to be emitted in.
[[nodiscard]] bool everyTriangleWindsTowardItsNormal(
    const VisiaGeometry& geometry) {
    if (geometry.indices.size() % 3 != 0) {
        return false;
    }
    for (std::size_t base = 0; base < geometry.indices.size(); base += 3) {
        const Vec3 first = positionOf(geometry, geometry.indices[base]);
        const Vec3 second = positionOf(geometry, geometry.indices[base + 1]);
        const Vec3 third = positionOf(geometry, geometry.indices[base + 2]);
        const Vec3 normal = normalOf(geometry, geometry.indices[base]);
        const Vec3 edgeA{second.x - first.x, second.y - first.y,
                         second.z - first.z};
        const Vec3 edgeB{third.x - first.x, third.y - first.y,
                         third.z - first.z};
        const Vec3 cross{
            edgeA.y * edgeB.z - edgeA.z * edgeB.y,
            edgeA.z * edgeB.x - edgeA.x * edgeB.z,
            edgeA.x * edgeB.y - edgeA.y * edgeB.x,
        };
        const double alignment = cross.x * normal.x + cross.y * normal.y +
                                 cross.z * normal.z;
        if (!(alignment > 0.0)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool everyQuadUsesItsOwnFourVertices(
    const VisiaGeometry& geometry) {
    if (geometry.indices.size() % 6 != 0) {
        return false;
    }
    for (std::size_t quad = 0; quad * 6 < geometry.indices.size(); ++quad) {
        const std::uint32_t base = static_cast<std::uint32_t>(quad * 4);
        const std::size_t offset = quad * 6;
        if (geometry.indices[offset] != base ||
            geometry.indices[offset + 1] != base + 1U ||
            geometry.indices[offset + 2] != base + 2U ||
            geometry.indices[offset + 3] != base ||
            geometry.indices[offset + 4] != base + 2U ||
            geometry.indices[offset + 5] != base + 3U) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool sameGeometry(const VisiaGeometry& left,
                                const VisiaGeometry& right) {
    if (left.vertices.size() != right.vertices.size() ||
        left.indices != right.indices) {
        return false;
    }
    for (std::size_t index = 0; index < left.vertices.size(); ++index) {
        if (left.vertices[index].position !=
                right.vertices[index].position ||
            left.vertices[index].normal != right.vertices[index].normal ||
            left.vertices[index].color != right.vertices[index].color) {
            return false;
        }
    }
    return true;
}

// --- Visia definitions -------------------------------------------------

void testDefinitionsResolveExplicitly() {
    const VisiaDefinition& resident =
        visiaDefinition(VisiaId::ResidentPrimitive);
    CHECK(resident.id == VisiaId::ResidentPrimitive);
    CHECK(resident.primitiveKind == VisiaPrimitiveKind::ResidentBoxes);

    const VisiaDefinition& landing =
        visiaDefinition(VisiaId::StoreLandingEffectPrimitive);
    CHECK(landing.id == VisiaId::StoreLandingEffectPrimitive);
    CHECK(landing.primitiveKind == VisiaPrimitiveKind::HorizontalAnnulus);

    // An unknown Visia must not fall back to a placeholder that renders as a
    // different object.
    CHECK_THROWS(std::invalid_argument,
                 (void)visiaDefinition(VisiaId::Invalid));
    CHECK_THROWS(std::invalid_argument,
                 (void)visiaDefinition(static_cast<VisiaId>(200)));
}

// --- resident boxes ----------------------------------------------------

void testResidentGeometryShape() {
    const VisiaPose pose{.positionMeters = {3.0, 0.5, -2.0},
                         .yawDegrees = 0.0};
    const VisiaGeometry geometry =
        buildResidentPrimitiveGeometry(residentPrimitiveVisia(), pose);

    CHECK(geometry.vertices.size() == kResidentVertices);
    CHECK(geometry.indices.size() == kResidentIndices);
    CHECK(everyIndexIsInRange(geometry));
    CHECK(everyQuadUsesItsOwnFourVertices(geometry));
    CHECK(everyTriangleWindsTowardItsNormal(geometry));
    CHECK(everyVertexIsFinite(geometry));
}

// The pose is measured from the centre of the feet, so an unrotated resident
// occupies exactly the definition's extents translated by the pose.
void testResidentGeometryIsPlacedAtItsPose() {
    const ResidentPrimitiveVisiaDefinition& definition =
        residentPrimitiveVisia();
    const VisiaPose pose{.positionMeters = {3.0, 0.5, -2.0},
                         .yawDegrees = 0.0};
    const VisiaGeometry geometry =
        buildResidentPrimitiveGeometry(definition, pose);

    double minimumX = std::numeric_limits<double>::infinity();
    double maximumX = -std::numeric_limits<double>::infinity();
    double minimumY = std::numeric_limits<double>::infinity();
    double maximumY = -std::numeric_limits<double>::infinity();
    double minimumZ = std::numeric_limits<double>::infinity();
    double maximumZ = -std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < geometry.vertices.size(); ++index) {
        const Vec3 position = positionOf(geometry, index);
        minimumX = std::min(minimumX, position.x);
        maximumX = std::max(maximumX, position.x);
        minimumY = std::min(minimumY, position.y);
        maximumY = std::max(maximumY, position.y);
        minimumZ = std::min(minimumZ, position.z);
        maximumZ = std::max(maximumZ, position.z);
    }

    const double expectedHalfWidth =
        std::max(definition.body.halfExtentsMeters.x,
                 definition.head.halfExtentsMeters.x);
    const double expectedHalfDepth =
        std::max(definition.body.halfExtentsMeters.z,
                 definition.head.halfExtentsMeters.z);
    constexpr double kTolerance = 1.0e-5;

    CHECK(approxEqual(minimumX, pose.positionMeters.x - expectedHalfWidth,
                      kTolerance));
    CHECK(approxEqual(maximumX, pose.positionMeters.x + expectedHalfWidth,
                      kTolerance));
    CHECK(approxEqual(minimumZ, pose.positionMeters.z - expectedHalfDepth,
                      kTolerance));
    CHECK(approxEqual(maximumZ, pose.positionMeters.z + expectedHalfDepth,
                      kTolerance));
    // The feet sit on the pose height, and the head top is the tallest point.
    CHECK(approxEqual(minimumY, pose.positionMeters.y, kTolerance));
    CHECK(approxEqual(maximumY,
                      pose.positionMeters.y +
                          definition.head.centerMeters.y +
                          definition.head.halfExtentsMeters.y,
                      kTolerance));
}

// A quarter turn swaps the width and depth extents; anything else means the
// yaw was applied after the world translation.
void testResidentGeometryRotatesAboutItsOwnOrigin() {
    const ResidentPrimitiveVisiaDefinition& definition =
        residentPrimitiveVisia();
    const VisiaPose pose{.positionMeters = {0.0, 0.0, 0.0},
                         .yawDegrees = 90.0};
    const VisiaGeometry geometry =
        buildResidentPrimitiveGeometry(definition, pose);

    double maximumX = 0.0;
    double maximumZ = 0.0;
    for (std::size_t index = 0; index < geometry.vertices.size(); ++index) {
        const Vec3 position = positionOf(geometry, index);
        maximumX = std::max(maximumX, std::abs(position.x));
        maximumZ = std::max(maximumZ, std::abs(position.z));
    }

    constexpr double kTolerance = 1.0e-5;
    CHECK(approxEqual(maximumX,
                      std::max(definition.body.halfExtentsMeters.z,
                               definition.head.halfExtentsMeters.z),
                      kTolerance));
    CHECK(approxEqual(maximumZ,
                      std::max(definition.body.halfExtentsMeters.x,
                               definition.head.halfExtentsMeters.x),
                      kTolerance));
    CHECK(everyTriangleWindsTowardItsNormal(geometry));
}

void testResidentGeometryRejectsInvalidInput() {
    const ResidentPrimitiveVisiaDefinition& definition =
        residentPrimitiveVisia();
    const VisiaPose validPose{.positionMeters = {0.0, 0.0, 0.0},
                              .yawDegrees = 0.0};

    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(
                     definition,
                     {.positionMeters =
                          {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
                      .yawDegrees = 0.0}));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(
                     definition,
                     {.positionMeters = {0.0, 0.0, 0.0},
                      .yawDegrees =
                          std::numeric_limits<double>::infinity()}));

    ResidentPrimitiveVisiaDefinition wrongId = definition;
    wrongId.id = VisiaId::StoreLandingEffectPrimitive;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(wrongId, validPose));

    ResidentPrimitiveVisiaDefinition flatBody = definition;
    flatBody.body.halfExtentsMeters.y = 0.0;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(flatBody, validPose));

    ResidentPrimitiveVisiaDefinition transparentHead = definition;
    transparentHead.head.color[3] = 0.0F;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(transparentHead,
                                                      validPose));

    ResidentPrimitiveVisiaDefinition outOfRangeColor = definition;
    outOfRangeColor.body.color[0] = 1.5F;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(outOfRangeColor,
                                                      validPose));

    // Sunk below the ground or overlapping the body: both would show as a
    // broken silhouette rather than a resident.
    ResidentPrimitiveVisiaDefinition sunkBody = definition;
    sunkBody.body.centerMeters.y = sunkBody.body.halfExtentsMeters.y - 0.01;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(sunkBody, validPose));

    ResidentPrimitiveVisiaDefinition overlappingHead = definition;
    overlappingHead.head.centerMeters.y =
        definition.body.centerMeters.y;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildResidentPrimitiveGeometry(overlappingHead,
                                                      validPose));
}

// --- landing annulus ---------------------------------------------------

void testLandingEffectGeometryShape() {
    const StoreLandingEffectPrimitiveVisiaDefinition& definition =
        storeLandingEffectPrimitiveVisia();
    const Vec3 origin{5.0, 1.0, -3.0};
    const VisiaGeometry geometry =
        buildStoreLandingEffectPrimitiveGeometry(definition, origin, 0.0);

    const std::size_t expectedVertices =
        (static_cast<std::size_t>(definition.segmentCount) + 1U) * 2U;
    const std::size_t expectedIndices =
        static_cast<std::size_t>(definition.segmentCount) * 6U;
    CHECK(geometry.vertices.size() == expectedVertices);
    CHECK(geometry.indices.size() == expectedIndices);
    CHECK(everyIndexIsInRange(geometry));
    CHECK(everyTriangleWindsTowardItsNormal(geometry));
    CHECK(everyVertexIsFinite(geometry));

    constexpr double kTolerance = 1.0e-5;
    for (std::size_t index = 0; index < geometry.vertices.size(); ++index) {
        const Vec3 normal = normalOf(geometry, index);
        CHECK(approxEqual(normal.x, 0.0, kTolerance));
        CHECK(approxEqual(normal.y, 1.0, kTolerance));
        CHECK(approxEqual(normal.z, 0.0, kTolerance));
        // The ring is horizontal, so it sits at one height above the origin.
        CHECK(approxEqual(positionOf(geometry, index).y,
                          origin.y + definition.heightOffsetMeters,
                          kTolerance));
    }
}

[[nodiscard]] double outerRadiusAt(
    const StoreLandingEffectPrimitiveVisiaDefinition& definition,
    const Vec3& origin, const double normalizedAge) {
    const VisiaGeometry geometry =
        buildStoreLandingEffectPrimitiveGeometry(definition, origin,
                                                 normalizedAge);
    double maximum = 0.0;
    for (std::size_t index = 0; index < geometry.vertices.size(); ++index) {
        const Vec3 position = positionOf(geometry, index);
        maximum = std::max(maximum, std::hypot(position.x - origin.x,
                                               position.z - origin.z));
    }
    return maximum;
}

void testLandingEffectExpandsAndFades() {
    const StoreLandingEffectPrimitiveVisiaDefinition& definition =
        storeLandingEffectPrimitiveVisia();
    const Vec3 origin{0.0, 0.0, 0.0};
    constexpr double kTolerance = 1.0e-4;

    CHECK(approxEqual(outerRadiusAt(definition, origin, 0.0),
                      definition.startOuterRadiusMeters, kTolerance));
    CHECK(approxEqual(outerRadiusAt(definition, origin, 1.0),
                      definition.endOuterRadiusMeters, kTolerance));

    double previous = 0.0;
    for (int step = 0; step <= 10; ++step) {
        const double age = static_cast<double>(step) / 10.0;
        const double radius = outerRadiusAt(definition, origin, age);
        CHECK(radius >= previous);
        previous = radius;
    }

    const VisiaGeometry youngest =
        buildStoreLandingEffectPrimitiveGeometry(definition, origin, 0.0);
    const VisiaGeometry oldest =
        buildStoreLandingEffectPrimitiveGeometry(definition, origin, 1.0);
    CHECK(youngest.vertices.front().color[3] == definition.color[3]);
    // The ring is fully faded out at the end of its life.
    CHECK(oldest.vertices.front().color[3] == 0.0F);
}

void testLandingEffectRejectsInvalidInput() {
    const StoreLandingEffectPrimitiveVisiaDefinition& definition =
        storeLandingEffectPrimitiveVisia();
    const Vec3 origin{0.0, 0.0, 0.0};

    // normalizedAge is inclusive [0, 1]; anything outside is a sampler bug,
    // not a value to clamp.
    CHECK_NO_THROW((void)buildStoreLandingEffectPrimitiveGeometry(definition,
                                                                  origin,
                                                                  0.0));
    CHECK_NO_THROW((void)buildStoreLandingEffectPrimitiveGeometry(definition,
                                                                  origin,
                                                                  1.0));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     definition, origin, -0.000001));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     definition, origin, 1.000001));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     definition, origin,
                     std::numeric_limits<double>::quiet_NaN()));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     definition,
                     {std::numeric_limits<double>::infinity(), 0.0, 0.0},
                     0.5));

    StoreLandingEffectPrimitiveVisiaDefinition wrongId = definition;
    wrongId.id = VisiaId::ResidentPrimitive;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(wrongId,
                                                                origin, 0.5));

    // Fewer than three segments cannot close a ring, and an unbounded segment
    // count would let one effect dominate the frame's index budget.
    StoreLandingEffectPrimitiveVisiaDefinition tooFewSegments = definition;
    tooFewSegments.segmentCount = 2;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     tooFewSegments, origin, 0.5));

    StoreLandingEffectPrimitiveVisiaDefinition tooManySegments = definition;
    tooManySegments.segmentCount = 4097;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     tooManySegments, origin, 0.5));

    StoreLandingEffectPrimitiveVisiaDefinition minimumSegments = definition;
    minimumSegments.segmentCount = 3;
    CHECK_NO_THROW((void)buildStoreLandingEffectPrimitiveGeometry(
        minimumSegments, origin, 0.5));

    StoreLandingEffectPrimitiveVisiaDefinition invertedRing = definition;
    invertedRing.endOuterRadiusMeters = invertedRing.endInnerRadiusMeters;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     invertedRing, origin, 0.5));

    // The ring only ever grows; a shrinking definition would run the easing
    // backwards.
    StoreLandingEffectPrimitiveVisiaDefinition shrinkingRing = definition;
    shrinkingRing.endInnerRadiusMeters =
        shrinkingRing.startInnerRadiusMeters - 0.01;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     shrinkingRing, origin, 0.5));

    StoreLandingEffectPrimitiveVisiaDefinition belowGround = definition;
    belowGround.heightOffsetMeters = -0.01;
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     belowGround, origin, 0.5));

    StoreLandingEffectPrimitiveVisiaDefinition nonFiniteRadius = definition;
    nonFiniteRadius.startOuterRadiusMeters =
        std::numeric_limits<double>::infinity();
    CHECK_THROWS(std::invalid_argument,
                 (void)buildStoreLandingEffectPrimitiveGeometry(
                     nonFiniteRadius, origin, 0.5));
}

// --- instance dispatch -------------------------------------------------

void testInstanceDispatchMatchesDirectBuilders() {
    const VisiaPose pose{.positionMeters = {2.0, 0.0, 7.0},
                         .yawDegrees = 33.0};
    const VisiaInstance resident{
        .id = VisiaId::ResidentPrimitive, .pose = pose, .normalizedAge = 0.0};
    CHECK(sameGeometry(
        buildPrimitiveVisiaGeometry(resident),
        buildResidentPrimitiveGeometry(residentPrimitiveVisia(), pose)));

    const VisiaInstance landing{
        .id = VisiaId::StoreLandingEffectPrimitive,
        .pose = pose,
        .normalizedAge = 0.4};
    CHECK(sameGeometry(buildPrimitiveVisiaGeometry(landing),
                       buildStoreLandingEffectPrimitiveGeometry(
                           storeLandingEffectPrimitiveVisia(),
                           pose.positionMeters, 0.4)));
}

void testInstanceDispatchRejectsMeaninglessFields() {
    const VisiaPose pose{.positionMeters = {0.0, 0.0, 0.0},
                         .yawDegrees = 0.0};

    // A resident has no lifetime, so an age on that instance means the caller
    // confused it with the transient landing effect.
    CHECK_THROWS(std::invalid_argument,
                 (void)buildPrimitiveVisiaGeometry(
                     {.id = VisiaId::ResidentPrimitive,
                      .pose = pose,
                      .normalizedAge = 0.5}));

    CHECK_THROWS(std::invalid_argument,
                 (void)buildPrimitiveVisiaGeometry(
                     {.id = VisiaId::Invalid,
                      .pose = pose,
                      .normalizedAge = 0.0}));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildPrimitiveVisiaGeometry(
                     {.id = static_cast<VisiaId>(77),
                      .pose = pose,
                      .normalizedAge = 0.0}));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildPrimitiveVisiaGeometry(
                     {.id = VisiaId::ResidentPrimitive,
                      .pose = {.positionMeters = {0.0, 0.0, 0.0},
                               .yawDegrees =
                                   std::numeric_limits<double>::quiet_NaN()},
                      .normalizedAge = 0.0}));
    CHECK_THROWS(std::invalid_argument,
                 (void)buildPrimitiveVisiaGeometry(
                     {.id = VisiaId::StoreLandingEffectPrimitive,
                      .pose = pose,
                      .normalizedAge =
                          std::numeric_limits<double>::infinity()}));
}

}  // namespace

int main() {
    testDefinitionsResolveExplicitly();
    testResidentGeometryShape();
    testResidentGeometryIsPlacedAtItsPose();
    testResidentGeometryRotatesAboutItsOwnOrigin();
    testResidentGeometryRejectsInvalidInput();
    testLandingEffectGeometryShape();
    testLandingEffectExpandsAndFades();
    testLandingEffectRejectsInvalidInput();
    testInstanceDispatchMatchesDirectBuilders();
    testInstanceDispatchRejectsMeaninglessFields();
    return konbini::test::summarize("visia_primitive_geometry_test");
}
