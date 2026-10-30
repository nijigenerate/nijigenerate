#include <string_view>
#include <nijigenerate/compat/native_access_fault.hpp>
#include "node_value_action.hpp"
#include <nijigenerate/ext/nodes/node_name.hpp>

namespace nijigenerate::actions::node {
namespace {

[[noreturn]] void failNullActionAccess(bool writing) {
    // The original D operation dereferences a null pointer/reference. Preserve
    // the native fault boundary instead of throwing an ordinary C++ exception
    // or invoking undefined behavior by dereferencing a null C++ pointer.
    compat::ngFailNullAccess(writing);
}

[[maybe_unused]] bool isNodeTransformActionName(std::string_view name) noexcept {
    if (name == "X" || name == "Y" || name == "Z" ||
        name == "translationX" || name == "translationY" || name == "translationZ" ||
        name == "rotationX" || name == "rotationY" || name == "rotationZ" ||
        name == "scaleX" || name == "scaleY" || name == "lockToRoot") {
        return true;
    }
    return false;
}

}

namespace detail {
void notifyDepthBoneNodeTransform(std::shared_ptr<nicxlive::core::nodes::Node> node,
    compat::RetainedStringSlice reason) {
    depthboneinvalidation::ngNotifyDepthBoneTargetChanged(node,
        depthboneinvalidation::DepthBoneMutationKind::TargetTransform, reason);
}

[[noreturn]] void failActionValuePointer() {
    failNullActionAccess(true);
}

[[noreturn]] void failActionObjectReference() {
    failNullActionAccess(false);
}

bool isTransformActionName(std::string_view name) noexcept {
    return isNodeTransformActionName(name);
}
}
}

namespace nijigenerate::core::translation {
TypedFormatArgument ngFormatArgument(const nicxlive::core::common::NullableString& value) {
    return ext::nodes::ngRetainNodeName(value);
}
}
