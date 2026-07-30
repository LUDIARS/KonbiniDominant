#include "konbini/sim/first_playable_content.h"

#include <charconv>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "json_document.h"

// @implements spec/data/content-schema.md First playable profile v1

namespace konbini::sim {

namespace {

using Object = json::Value::Object;

const json::Value& requireField(const Object& object, const std::string_view name) {
    const auto found = object.find(name);
    if (found == object.end()) {
        throw std::invalid_argument("required content field is missing: " +
                                    std::string(name));
    }
    return found->second;
}

void requireOnlyKeys(const Object& object,
                     const std::initializer_list<std::string_view> allowed) {
    for (const auto& [key, unused] : object) {
        (void)unused;
        bool isAllowed = false;
        for (const std::string_view candidate : allowed) {
            if (key == candidate) {
                isAllowed = true;
                break;
            }
        }
        if (!isAllowed) {
            throw std::invalid_argument("unknown content field: " + key);
        }
    }
}

std::int64_t requireInteger(const json::Value& value,
                            const std::string_view fieldName) {
    if (!value.isNumber()) {
        throw std::invalid_argument(std::string(fieldName) + " must be an integer");
    }
    const std::string_view lexeme = value.numberLexeme();
    std::int64_t number = 0;
    const auto parsed = std::from_chars(
        lexeme.data(), lexeme.data() + lexeme.size(), number);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != lexeme.data() + lexeme.size()) {
        throw std::invalid_argument(std::string(fieldName) +
                                    " must be a canonical signed 64-bit integer");
    }
    return number;
}

std::uint32_t requireUint32(const json::Value& value,
                            const std::string_view fieldName) {
    const std::int64_t number = requireInteger(value, fieldName);
    if (number < 0 ||
        number > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::invalid_argument(std::string(fieldName) +
                                    " is outside the unsigned 32-bit integer range");
    }
    return static_cast<std::uint32_t>(number);
}

double requirePositiveNumber(const json::Value& value,
                             const std::string_view fieldName) {
    if (!value.isNumber() || !std::isfinite(value.number()) ||
        value.number() <= 0.0) {
        throw std::invalid_argument(std::string(fieldName) +
                                    " must be a positive finite number");
    }
    return value.number();
}

SimulationContent parseSimulation(const json::Value& value) {
    const Object& object = value.object();
    requireOnlyKeys(
        object, {"ticksPerSecond", "economyPeriodTicks", "randomAlgorithm"});
    return SimulationContent{
        .ticksPerSecond =
            requireUint32(requireField(object, "ticksPerSecond"), "ticksPerSecond"),
        .economyPeriodTicks = requireUint32(
            requireField(object, "economyPeriodTicks"), "economyPeriodTicks"),
        .randomAlgorithm = requireField(object, "randomAlgorithm").string(),
    };
}

PopulationContent parsePopulation(const json::Value& value) {
    const Object& object = value.object();
    requireOnlyKeys(
        object, {"basePopulation", "randomPopulationCount", "randomStream"});
    return PopulationContent{
        .basePopulation =
            requireUint32(requireField(object, "basePopulation"), "basePopulation"),
        .randomPopulationCount = requireUint32(
            requireField(object, "randomPopulationCount"), "randomPopulationCount"),
        .randomStream = requireField(object, "randomStream").string(),
    };
}

ChainContent parseChain(const json::Value& value) {
    const Object& object = value.object();
    requireOnlyKeys(object,
                    {"id",
                     "displayName",
                     "buildCostCredits",
                     "zocRadiusMeters",
                     "revenueMilliCreditsPerPerson"});

    const std::string& slug = requireField(object, "id").string();
    const std::optional<ChainId> id = chainIdFromSlug(slug);
    if (!id.has_value()) {
        throw std::invalid_argument("unknown first-playable chain id: " + slug);
    }

    return ChainContent{
        .id = *id,
        .displayName = requireField(object, "displayName").string(),
        .buildCostCredits =
            requireInteger(requireField(object, "buildCostCredits"), "buildCostCredits"),
        .zocRadiusMeters =
            requirePositiveNumber(requireField(object, "zocRadiusMeters"),
                                  "zocRadiusMeters"),
        .revenueMilliCreditsPerPerson = requireInteger(
            requireField(object, "revenueMilliCreditsPerPerson"),
            "revenueMilliCreditsPerPerson"),
    };
}

void requireBaselineV1(const FirstPlayableContent& content) {
    if (content.schemaVersion != 1 || content.contentVersion != 1) {
        throw std::invalid_argument(
            "unsupported first-playable schemaVersion/contentVersion");
    }
    if (content.simulation.ticksPerSecond != 10 ||
        content.simulation.economyPeriodTicks != 10 ||
        content.simulation.randomAlgorithm != "splitmix64-counter-v1") {
        throw std::invalid_argument("simulation values do not match contentVersion 1");
    }
    if (content.population.basePopulation != 50 ||
        content.population.randomPopulationCount != 101 ||
        content.population.randomStream != "FP_POPULATION") {
        throw std::invalid_argument("population values do not match contentVersion 1");
    }
    if (content.startingStoreEquivalent != 5) {
        throw std::invalid_argument(
            "startingStoreEquivalent does not match contentVersion 1");
    }

    struct ExpectedChain {
        ChainId id;
        std::string_view displayName;
        std::int64_t buildCost;
        double radius;
        std::int64_t revenueMilli;
    };
    constexpr std::array<ExpectedChain, kFirstPlayableChainCount> expected{{
        {ChainId::Losan, "ローサン", 1000, 18.0, 500},
        {ChainId::Famoma, "ファモマ", 1250, 24.0, 450},
        {ChainId::SebanIleban, "セバンイレバン", 800, 18.0, 400},
    }};

    for (const ExpectedChain& expectedChain : expected) {
        const ChainContent& actual = content.chain(expectedChain.id);
        if (actual.id != expectedChain.id ||
            actual.displayName != expectedChain.displayName ||
            actual.buildCostCredits != expectedChain.buildCost ||
            actual.zocRadiusMeters != expectedChain.radius ||
            actual.revenueMilliCreditsPerPerson != expectedChain.revenueMilli) {
            throw std::invalid_argument(
                "chain values do not match first-playable contentVersion 1");
        }
    }
}

}  // namespace

// @implements spec/data/content-schema.md Validation
void validateFirstPlayableContent(const FirstPlayableContent& content) {
    requireBaselineV1(content);
}

// @implements spec/data/content-schema.md First playable profile v1
// @implements spec/data/content-schema.md Validation
FirstPlayableContent parseFirstPlayableContent(const std::string_view input) {
    const json::Value document = json::parseDocument(input);
    const Object& root = document.object();
    requireOnlyKeys(root,
                    {"schemaVersion",
                     "contentVersion",
                     "simulation",
                     "population",
                     "startingStoreEquivalent",
                     "chains"});

    FirstPlayableContent content;
    content.schemaVersion =
        requireUint32(requireField(root, "schemaVersion"), "schemaVersion");
    content.contentVersion =
        requireUint32(requireField(root, "contentVersion"), "contentVersion");
    content.simulation = parseSimulation(requireField(root, "simulation"));
    content.population = parsePopulation(requireField(root, "population"));
    content.startingStoreEquivalent = requireUint32(
        requireField(root, "startingStoreEquivalent"), "startingStoreEquivalent");

    const json::Value::Array& chains = requireField(root, "chains").array();
    if (chains.size() != kFirstPlayableChainCount) {
        throw std::invalid_argument(
            "first-playable content must contain exactly three chains");
    }
    std::array<bool, kFirstPlayableChainCount> seen{};
    for (const json::Value& value : chains) {
        ChainContent chain = parseChain(value);
        const std::size_t index = chainIndex(chain.id);
        if (seen[index]) {
            throw std::invalid_argument("duplicate first-playable chain id");
        }
        seen[index] = true;
        content.chains[index] = std::move(chain);
    }

    validateFirstPlayableContent(content);
    return content;
}

// @implements spec/data/content-schema.md First playable profile v1
FirstPlayableContent loadFirstPlayableContent(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to open first-playable content: " +
                                 path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();
    // `operator<<(std::streambuf*)` never records anything on `input`, so the
    // read state has to be read back from the destination stream. failbit
    // covers both "inserted nothing" and "extraction failed"; only the latter
    // is an I/O error here. An empty file falls through to the JSON document
    // check, which reports it with a more specific message.
    if (buffer.fail() && !text.empty()) {
        throw std::runtime_error("unable to read first-playable content: " +
                                 path.string());
    }
    return parseFirstPlayableContent(text);
}

}  // namespace konbini::sim
