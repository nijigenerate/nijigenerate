#include "utils.hpp"

#include <array>
#include <utility>

namespace nijigenerate::utils {
namespace {
std::string_view iconFor(std::string_view typeId) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 13> icons{{
        {"Part", "\xEE\x9A\x91"},
        {"Composite", "\xEE\x98\xBB"},
        {"Mask", "\xEE\x85\x8E"},
        {"SimplePhysics", "\xEE\x94\x81"},
        {"Camera", "\xEE\x81\x8B"},
        {"MeshGroup", "\xEE\xA2\x86"},
        {"DynamicComposite", "\xEE\x8B\x87"},
        {"PathDeformer", "\xEE\xAB\xB5"},
        {"GridDeformer", "\xEE\x8F\xAC"},
        {"DepthRigRoot", "\xEE\x94\xB6"},
        {"DepthBone", "\xEE\xAD\x83"},
        {"Parameter", "\xEE\x90\xA9"},
        {"Binding", "\xEE\x89\x92"},
    }};
    for (const auto& [name, icon] : icons) {
        if (typeId == name) return icon;
    }
    return "\xEE\xA5\xBA";
}
}
std::string_view incTypeIdToIcon(std::string_view typeId) { return iconFor(typeId); }
compat::RetainedString ngTypeIdToIconSource(compat::RetainedString typeId) {
    // Every lookup branch returns the same process-static original literal.
    return compat::RetainedString::fromStaticView(iconFor(typeId.view()));
}
}
