#pragma once
#include <boost/json.hpp>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace nijigenerate::autorig::patterns {
using Json = boost::json::value;
using Checkpoint = std::function<void()>;

// Preserve serialized binary64 values across task and INX boundaries.
Json ngParsePatternJson(std::string_view text);

struct PatternSpec {
    const char* id;
    const char* label;
    const char* mechanism;
    std::vector<std::string> roles;
};

const std::vector<PatternSpec>& ngPatternSpecs();
const PatternSpec& ngPatternSpec(std::string_view id);

// The document is the serialized INX JSON, not a live editor object.
// All mutations are copy-on-success and keep source UUIDs and texture payloads.
Json ngDetectPattern(std::string_view pattern, const Json& document,
    const Json& request, Checkpoint checkpoint = {});
// Plans are derived from the current imported document, never a reference INX.
Json ngPlanPattern(std::string_view pattern, const Json& document,
    const Json& selection, Checkpoint checkpoint = {});
Json ngDeformPattern(std::string_view pattern, const Json& document,
    const Json& selection, const Json& request, Checkpoint checkpoint = {});
Json ngVerifyPattern(std::string_view pattern, const Json& before,
    const Json& after, const Json& selection, const Json& receipt,
    Checkpoint checkpoint = {});

struct InxDocument {
    Json document;
    std::vector<std::uint8_t> prefix;
    std::vector<std::uint8_t> payload;
};
InxDocument ngReadPatternInx(std::span<const std::uint8_t> bytes);
std::vector<std::uint8_t> ngWritePatternInx(const InxDocument& source, const Json& document);
}
