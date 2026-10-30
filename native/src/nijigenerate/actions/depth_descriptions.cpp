/*
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
#include "depth.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nodes/node.hpp>
#include <array>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::actions::depth {
namespace {
ActionText retainFormatted(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

ActionText DepthMappedChangeAction::describe() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Changed depth map of %s"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(node)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText DepthMappedChangeAction::describeUndo() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Depth map of %s was changed"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(node)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText DepthOperationMappedChangeAction::describe() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Changed depth operations of %s"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(node)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText DepthOperationMappedChangeAction::describeUndo() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Depth operations of %s were changed"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(node)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

} // namespace nijigenerate::actions::depth
