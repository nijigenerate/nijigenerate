#include "detail/bone_input.hpp"
#include "detail/bone_math.hpp"

#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <array>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::commands::depth::bone::detail {
using nicxlive::core::nodes::Node;
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;
using ext::nodes::ExDepthTargetKind;

[[noreturn]] void failInput(std::string_view text, std::size_t sourceLine) {
    // Phobos enforce duplicates nonempty messages before constructing Exception.
    compat::RetainedString message;
    try {
        message = text.empty()
            ? compat::RetainedString::fromStaticLiteral("Enforcement failed")
            : compat::RetainedString(std::string(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
}

nicxlive::core::math::Vec3 vec3From(compat::Slice<float> values, compat::RetainedStringSlice name) {
    if (values.size() != 3) {
        // The selected Phobos enforce takes a lazy message: translate/format
        // only on failure, before construction or reads of vector components.
        auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("%s must be [x, y, z]"));
        std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(std::move(name))};
        failInput(core::translation::ngFormatTyped(pattern.view(), arguments), 101);
    }
    return {values[0], values[1], values[2]};
}

std::shared_ptr<ExDepthRigRoot> requireRoot(std::shared_ptr<Node> node) {
    auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(node);
    if (!root) failInput("Node is not a DepthRigRoot", 107);
    return root;
}

std::shared_ptr<ExDepthBone> requireBone(std::shared_ptr<Node> node) {
    auto bone = std::dynamic_pointer_cast<ExDepthBone>(node);
    if (!bone) failInput("Node is not a DepthBone", 113);
    return bone;
}

std::shared_ptr<ExDepthBone> requireSourceBone(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> node) {
    auto bone = requireBone(node);
    if (findBoneByUuid(root, bone->getUUID()) != bone) {
        failInput("DepthBone source must belong to the selected DepthRigRoot", 119);
    }
    return bone;
}

ExDepthTargetKind targetKindOf(std::shared_ptr<Node> node) {
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(node)) return ExDepthTargetKind::Grid;
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::PathDeformer>(node)) return ExDepthTargetKind::Path;
    failInput("target must be GridDeformer or PathDeformer", 127);
}

} // namespace nijigenerate::commands::depth::bone::detail
