#include "native_signature.hpp"

namespace nijigenerate::autorig::deterministic::native_detail {

void authoredState(core::settings::SettingsJsonValue node) {
    auto& object = node.sourceObjectRef();
    // Generated composite meshes are excluded only for an explicit true flag.
    if (auto* resized = object.if_contains("auto_resized"))
        if (resized->as_bool()) object.erase("mesh");
    // Look up children after erasure; keep mutable references into the real array.
    if (auto* children = object.if_contains("children"))
        for (auto& child : children->as_array()) authoredState(child);
}

} // namespace nijigenerate::autorig::deterministic::native_detail
