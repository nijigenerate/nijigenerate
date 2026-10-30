// Faithful body of regression.d2890-2961; include once outside namespaces.
#include "root_node_move_world.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/node/node.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <compat/source_allocator.hpp>
#include <math/transform.hpp>
#include <nodes/dynamic_composite.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>

namespace nijigenerate_tests::regression::detail {
nicxlive::core::math::Vec3 nodeWorldTranslation(std::shared_ptr<nicxlive::core::nodes::Node> node);

void testNodeCommandMovePreservesWorldTransform() {
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::node::node::MoveNodeCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::project::incActivePuppet;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Transform;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::nodes::DynamicComposite;
    using nicxlive::core::nodes::Node;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    // std.conv.toChars!10(int).array; this original loop only supplies 0..4.
    const auto iterationText = [](int input) {
        std::array<char, 11> buffer;
        auto first = buffer.size();
        std::uint64_t value = static_cast<std::uint64_t>(input);
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (value);
        auto bytes = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(bytes.data(), buffer.data() + first, bytes.size());
        return RetainedString::retainedView(bytes.retentionOwner(), bytes.data(), bytes.size());
    };
    const auto requireIteration = [&](bool condition, RetainedString prefix, int iteration) {
        const auto number = iterationText(iteration);
        require(condition, concatenateSourceStrings({prefix, number}));
    };
    resetCase();

    auto parentA = Node::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    auto parentB = Node::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    auto child = Node::createSourceDeferred([&] { return parentA; });
    parentA->setName(NullableString::fromStaticLiteral("transform-parent-a"));
    parentB->setName(NullableString::fromStaticLiteral("transform-parent-b"));
    child->setName(NullableString::fromStaticLiteral("transform-child"));
    parentA->localTransform = Transform(Vec3{15, -6, 2}, Vec3{0, 0, 0.35f}, Vec2{1.4f, 0.8f});
    parentB->localTransform = Transform(Vec3{-8, 20, -3}, Vec3{0, 0, -0.5f}, Vec2{0.6f, 1.7f});
    child->localTransform = Transform(Vec3{4, 7, 1}, Vec3{0, 0, 0.2f}, Vec2{1.2f, 0.9f});
    parentA->transformChanged();
    parentB->transformChanged();
    child->transformChanged();
    auto before = nodeWorldTranslation(child);
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{child});
    require(ngRequireObject(constructSourceCommand<MoveNodeCommand>(parentB, 0)->run(ctx))->succeeded,
        text("MoveNodeCommand should move transformed child"));
    require(child->parentRef() == parentB, text("MoveNodeCommand should reparent transformed child"));
    require(nearVec3(nodeWorldTranslation(child), before), text("MoveNodeCommand should preserve child world translation"));
    incActionUndo();
    require(child->parentRef() == parentA, text("undo MoveNodeCommand should restore transformed child's parent"));
    require(nearVec3(nodeWorldTranslation(child), before), text("undo MoveNodeCommand should restore child world translation"));
    incActionRedo();
    require(child->parentRef() == parentB, text("redo MoveNodeCommand should reparent transformed child again"));
    require(nearVec3(nodeWorldTranslation(child), before),
        text("redo MoveNodeCommand should preserve child world translation again"));

    auto dynamicParent = DynamicComposite::createSourceDeferred([] {
        return ngRequireObject(incActivePuppet())->root;
    });
    dynamicParent->setName(NullableString::fromStaticLiteral("transform-dynamic-parent"));
    dynamicParent->localTransform = Transform(Vec3{30, -12, 0}, Vec3{0, 0, 0.45f}, Vec2{2.0f, 0.5f});
    dynamicParent->transformChanged();
    auto dynamicBefore = nodeWorldTranslation(child);
    for (int i = 0; i < 5; ++i) {
        ctx->nodes(Context::Nodes{child});
        requireIteration(ngRequireObject(constructSourceCommand<MoveNodeCommand>(dynamicParent, 0)->run(ctx))->succeeded,
            text("MoveNodeCommand should move child into DynamicComposite on iteration "), i);
        require(child->parentRef() == dynamicParent, text("MoveNodeCommand should parent child under DynamicComposite"));
        requireIteration(nearVec3(nodeWorldTranslation(child), dynamicBefore),
            text("MoveNodeCommand should preserve world translation inside DynamicComposite on iteration "), i);
        ctx->nodes(Context::Nodes{child});
        requireIteration(ngRequireObject(constructSourceCommand<MoveNodeCommand>(parentA, 0)->run(ctx))->succeeded,
            text("MoveNodeCommand should move child out of DynamicComposite on iteration "), i);
        require(child->parentRef() == parentA, text("MoveNodeCommand should restore child under normal parent"));
        requireIteration(nearVec3(nodeWorldTranslation(child), dynamicBefore),
            text("MoveNodeCommand should not accumulate DynamicComposite drift on iteration "), i);
    }
}
}
