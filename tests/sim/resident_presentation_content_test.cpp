#include "konbini/sim/first_playable_content.h"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../check.h"

// @implements spec/test/verification-strategy.md 1. Data / ID unit tests
// @implements spec/data/content-schema.md Validation
// @implements spec/feature/npc-conversations-and-placement-feedback.md Ambient resident baseline

namespace {

using konbini::sim::FirstPlayableContent;
using konbini::sim::ResidentPresentationContent;
using konbini::sim::kResidentRemarkCount;
using konbini::sim::parseFirstPlayableContent;
using konbini::sim::validateResidentPresentationContent;

// Documents are assembled from ordered key/value pairs so that "missing key",
// "unknown key" and "value out of range" are the same edit applied to one
// baseline instead of three hand-maintained literals that can drift apart.
using Field = std::pair<std::string, std::string>;

void setField(std::vector<Field>& fields, const std::string_view key,
              std::string value) {
    for (Field& field : fields) {
        if (field.first == key) {
            field.second = std::move(value);
            return;
        }
    }
    fields.emplace_back(std::string(key), std::move(value));
}

void removeField(std::vector<Field>& fields, const std::string_view key) {
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (fields[index].first == key) {
            fields.erase(fields.begin() + static_cast<std::ptrdiff_t>(index));
            return;
        }
    }
}

[[nodiscard]] std::string makeObject(const std::vector<Field>& fields) {
    std::string text = "{";
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index != 0) {
            text += ",";
        }
        text += "\"" + fields[index].first + "\":" + fields[index].second;
    }
    text += "}";
    return text;
}

// The pinned contentVersion 2 profile from data/content/first-playable.json.
[[nodiscard]] std::vector<Field> baselineResidentFields() {
    return {
        {"samplesPerPopulationCell", "1"},
        {"walkingSpeedMetersPerSecond", "1.5"},
        {"homeDwellTicks", "30"},
        {"storeDwellTicks", "40"},
        {"speechDurationTicks", "30"},
        {"bubbleHeightMeters", "2.2"},
        {"bubbleMaxDistanceMeters", "220"},
        {"remarks",
         "[\"NICE AND CLOSE\",\"EASY TO REACH\",\"HANDY LOCATION\"]"},
    };
}

[[nodiscard]] std::vector<Field> baselineRootFields(
    const std::vector<Field>& residentFields) {
    return {
        {"schemaVersion", "1"},
        {"contentVersion", "2"},
        {"simulation",
         "{\"ticksPerSecond\":10,\"economyPeriodTicks\":10,"
         "\"randomAlgorithm\":\"splitmix64-counter-v1\"}"},
        {"population",
         "{\"basePopulation\":50,\"randomPopulationCount\":101,"
         "\"randomStream\":\"FP_POPULATION\"}"},
        {"residentPresentation", makeObject(residentFields)},
        {"startingStoreEquivalent", "5"},
        {"chains",
         "[{\"id\":\"losan\",\"displayName\":\"\\u30ed\\u30fc\\u30b5\\u30f3\","
         "\"buildCostCredits\":1000,\"zocRadiusMeters\":18,"
         "\"revenueMilliCreditsPerPerson\":500},"
         "{\"id\":\"famoma\",\"displayName\":"
         "\"\\u30d5\\u30a1\\u30e2\\u30de\",\"buildCostCredits\":1250,"
         "\"zocRadiusMeters\":24,\"revenueMilliCreditsPerPerson\":450},"
         "{\"id\":\"seban_ileban\",\"displayName\":"
         "\"\\u30bb\\u30d0\\u30f3\\u30a4\\u30ec\\u30d0\\u30f3\","
         "\"buildCostCredits\":800,\"zocRadiusMeters\":18,"
         "\"revenueMilliCreditsPerPerson\":400}]"},
    };
}

[[nodiscard]] std::string documentWithResident(
    const std::vector<Field>& residentFields) {
    return makeObject(baselineRootFields(residentFields));
}

[[nodiscard]] std::string documentWithResidentField(
    const std::string_view key, std::string value) {
    std::vector<Field> fields = baselineResidentFields();
    setField(fields, key, std::move(value));
    return documentWithResident(fields);
}

[[nodiscard]] std::string documentWithoutResidentField(
    const std::string_view key) {
    std::vector<Field> fields = baselineResidentFields();
    removeField(fields, key);
    return documentWithResident(fields);
}

[[nodiscard]] ResidentPresentationContent shapeValidContent() {
    return {
        .samplesPerPopulationCell = 2,
        .walkingSpeedMetersPerSecond = 1.25,
        .homeDwellTicks = 7,
        .storeDwellTicks = 9,
        .speechDurationTicks = 9,
        .bubbleHeightMeters = 2.0,
        .bubbleMaxDistanceMeters = 40.0,
        .remarks = {{std::string("ALPHA"), std::string("BETA MIX"),
                     std::string("GAMMA")}},
    };
}

// --- normal profile ----------------------------------------------------

void testBaselineProfileParses() {
    const FirstPlayableContent content =
        parseFirstPlayableContent(documentWithResident(
            baselineResidentFields()));
    const ResidentPresentationContent& residents =
        content.residentPresentation;

    CHECK(content.schemaVersion == 1);
    CHECK(content.contentVersion == 2);
    CHECK(residents.samplesPerPopulationCell == 1);
    CHECK(residents.walkingSpeedMetersPerSecond == 1.5);
    CHECK(residents.homeDwellTicks == 30);
    CHECK(residents.storeDwellTicks == 40);
    CHECK(residents.speechDurationTicks == 30);
    CHECK(residents.bubbleHeightMeters == 2.2);
    CHECK(residents.bubbleMaxDistanceMeters == 220.0);
    CHECK(residents.remarks.size() == kResidentRemarkCount);
    CHECK(residents.remarks[0] == "NICE AND CLOSE");
    CHECK(residents.remarks[1] == "EASY TO REACH");
    CHECK(residents.remarks[2] == "HANDY LOCATION");
    CHECK_NO_THROW(validateResidentPresentationContent(residents));
}

// The point of pinning contentVersion 2 is that a value edit fails loudly
// instead of silently changing how far residents walk.
void testPinnedValuesRejectDrift() {
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("homeDwellTicks", "31")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField(
                         "walkingSpeedMetersPerSecond", "1.6")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField(
                         "bubbleMaxDistanceMeters", "221")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField(
                         "remarks",
                         "[\"NICE AND CLOSE\",\"EASY TO REACH\","
                         "\"HANDY SPOT\"]")));
}

// --- missing / unknown keys -------------------------------------------

void testMissingKeysAreRejected() {
    for (const std::string_view key :
         {"samplesPerPopulationCell", "walkingSpeedMetersPerSecond",
          "homeDwellTicks", "storeDwellTicks", "speechDurationTicks",
          "bubbleHeightMeters", "bubbleMaxDistanceMeters", "remarks"}) {
        CHECK_THROWS(std::invalid_argument,
                     (void)parseFirstPlayableContent(
                         documentWithoutResidentField(key)));
    }

    std::vector<Field> root = baselineRootFields(baselineResidentFields());
    removeField(root, "residentPresentation");
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(makeObject(root)));
}

void testUnknownKeysAreRejected() {
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("bubbleWidthMeters", "1")));
    // A near-miss key must be reported, not accepted as an extra field while
    // the real key silently falls back to its default.
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("homeDwellTick", "30")));

    std::vector<Field> root = baselineRootFields(baselineResidentFields());
    setField(root, "residentPresentationV3", "{}");
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(makeObject(root)));
}

// --- non-finite values -------------------------------------------------

void testNonFiniteValuesAreRejected() {
    // The JSON reader refuses to produce a non-finite double at all, so the
    // fail-fast happens before any field reaches the content struct.
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("bubbleHeightMeters", "1e999")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField(
                         "walkingSpeedMetersPerSecond", "-1e999")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField(
                         "bubbleMaxDistanceMeters", "NaN")));

    // Programmatically built content bypasses the reader, so the struct-level
    // validation has to reject the same values on its own.
    ResidentPresentationContent content = shapeValidContent();
    content.walkingSpeedMetersPerSecond =
        std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content = shapeValidContent();
    content.bubbleHeightMeters = std::numeric_limits<double>::infinity();
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content = shapeValidContent();
    content.bubbleMaxDistanceMeters =
        -std::numeric_limits<double>::infinity();
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));
}

void testNonPositiveValuesAreRejected() {
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(documentWithResidentField(
                     "samplesPerPopulationCell", "0")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("homeDwellTicks", "0")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("storeDwellTicks", "0")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("speechDurationTicks", "0")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(documentWithResidentField(
                     "walkingSpeedMetersPerSecond", "0")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(documentWithResidentField(
                     "walkingSpeedMetersPerSecond", "-1.5")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("bubbleHeightMeters", "-2.2")));

    ResidentPresentationContent content = shapeValidContent();
    content.samplesPerPopulationCell = 0;
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content = shapeValidContent();
    content.homeDwellTicks = 0;
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content = shapeValidContent();
    content.storeDwellTicks = 0;
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content = shapeValidContent();
    content.speechDurationTicks = 0;
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));
}

// --- duration consistency ----------------------------------------------

// Speech starts at the beginning of the store dwell, so a speech window
// longer than that dwell would leave a bubble attached to a resident who has
// already started walking home.
void testSpeechDurationCannotExceedStoreDwell() {
    ResidentPresentationContent content = shapeValidContent();
    content.storeDwellTicks = 9;
    content.speechDurationTicks = 9;
    CHECK_NO_THROW(validateResidentPresentationContent(content));

    content.speechDurationTicks = 10;
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content.storeDwellTicks = 1;
    content.speechDurationTicks = 2;
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(
                     documentWithResidentField("speechDurationTicks", "41")));
}

// --- remarks -----------------------------------------------------------

void testRemarkShapeIsEnforced() {
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(documentWithResidentField(
                     "remarks", "[\"NICE AND CLOSE\",\"EASY TO REACH\"]")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(documentWithResidentField(
                     "remarks",
                     "[\"NICE AND CLOSE\",\"EASY TO REACH\","
                     "\"HANDY LOCATION\",\"EXTRA LINE\"]")));
    CHECK_THROWS(std::invalid_argument,
                 (void)parseFirstPlayableContent(documentWithResidentField(
                     "remarks", "[\"\",\"EASY TO REACH\","
                                "\"HANDY LOCATION\"]")));

    ResidentPresentationContent content = shapeValidContent();
    content.remarks[0] = "";
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    // The bubble font only carries ASCII A-Z and space, so anything else in
    // content would reach the glyph lookup and throw while rendering.
    content = shapeValidContent();
    content.remarks[1] = "nice and close";
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content = shapeValidContent();
    content.remarks[1] = "STORE 24H";
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    // Whitespace carries no glyph, so it is not a remark.
    content = shapeValidContent();
    content.remarks[2] = "   ";
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));

    content = shapeValidContent();
    content.remarks[2] = std::string(24, 'A');
    CHECK_NO_THROW(validateResidentPresentationContent(content));

    content.remarks[2] = std::string(25, 'A');
    CHECK_THROWS(std::invalid_argument,
                 validateResidentPresentationContent(content));
}

}  // namespace

int main() {
    testBaselineProfileParses();
    testPinnedValuesRejectDrift();
    testMissingKeysAreRejected();
    testUnknownKeysAreRejected();
    testNonFiniteValuesAreRejected();
    testNonPositiveValuesAreRejected();
    testSpeechDurationCannotExceedStoreDwell();
    testRemarkShapeIsEnforced();
    return konbini::test::summarize("resident_presentation_content_test");
}
