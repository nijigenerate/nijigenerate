// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "depth.hpp"
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nodes/node.hpp>
#include <utility>

namespace nijigenerate::actions::depth {
DepthMappedChangeAction::DepthMappedChangeAction(SourceAllocationTag) {}

DepthMappedChangeAction::DepthMappedChangeAction(std::shared_ptr<nicxlive::core::nodes::Node> value,
    compat::RetainedStringSlice text) : DepthMappedChangeAction(SourceAllocationTag{}) {
    initializeSource(std::move(value), std::move(text));
}

void DepthMappedChangeAction::initializeSource(std::shared_ptr<nicxlive::core::nodes::Node> value,
    compat::RetainedStringSlice text) {
    node = std::move(value);
    reason = std::move(text);
    depthMapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(node);
#ifndef NDEBUG
    if (!depthMapped) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/depth.d"), 40);
#endif
    oldDepths = capture();
}
DepthOperationMappedChangeAction::DepthOperationMappedChangeAction(SourceAllocationTag) {}

DepthOperationMappedChangeAction::DepthOperationMappedChangeAction(
        std::shared_ptr<nicxlive::core::nodes::Node> value) : DepthOperationMappedChangeAction(SourceAllocationTag{}) {
    initializeSource(std::move(value));
}

void DepthOperationMappedChangeAction::initializeSource(std::shared_ptr<nicxlive::core::nodes::Node> value) {
    node = std::move(value);
    depthOperated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(node);
#ifndef NDEBUG
    if (!depthOperated) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/depth.d"), 107);
#endif
    oldState = capture();
}
}
