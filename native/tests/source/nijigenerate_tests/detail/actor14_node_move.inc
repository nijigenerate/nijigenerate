// Existing regression.d2835-2879,2963-3001; include inside regression::detail after helpers.
void testNodeCommandMoveUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::MoveNodeCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto parentA = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto parentB = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto child = Node::createSourceDeferred([&] { return parentA; });
    parentA->setName(NullableString::fromStaticLiteral("parentA"));
    parentB->setName(NullableString::fromStaticLiteral("parentB"));
    child->setName(NullableString::fromStaticLiteral("child"));
    parentA->zSort(10);
    parentB->zSort(30);
    child->zSort(7);
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{child});
    require(ngRequireObject(constructSourceCommand<MoveNodeCommand>(parentB, 0)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("MoveNodeCommand should succeed"));
    require(child->parentRef() == parentB,
        RetainedString::fromStaticLiteral("MoveNodeCommand should reparent node"));
    require(isChildOf(parentB, child),
        RetainedString::fromStaticLiteral("MoveNodeCommand should insert node into new parent"));
    auto movedSort = child->relZSort();
    parentA->zSort(20);
    parentB->zSort(40);
    incActionUndo();
    require(child->parentRef() == parentA,
        RetainedString::fromStaticLiteral("undo MoveNodeCommand should restore old parent"));
    require(isChildOf(parentA, child),
        RetainedString::fromStaticLiteral("undo MoveNodeCommand should restore child list"));
    require(child->relZSort() == 7,
        RetainedString::fromStaticLiteral("undo should restore the saved relative draw order"));
    incActionRedo();
    require(child->parentRef() == parentB,
        RetainedString::fromStaticLiteral("redo MoveNodeCommand should restore new parent"));
    require(isChildOf(parentB, child),
        RetainedString::fromStaticLiteral("redo MoveNodeCommand should restore new child list"));
    require(child->relZSort() == movedSort,
        RetainedString::fromStaticLiteral("redo should restore draw order without recomputing parent offsets"));
    parentB->localTransform.translation = nicxlive::core::math::Vec3{15, -20, 4};
    parentB->localTransform.update();
    auto created = Node::createSourceDeferred([] { return std::shared_ptr<Node>{}; });
    nijigenerate::actions::node::incAddChildWithHistory(created, parentB,
        RetainedString::fromStaticLiteral("created-frame"));
    auto createdFrame = created->localTransform;
    auto createdSort = created->relZSort();
    incActionUndo();
    incActionRedo();
    // D's generated Transform equality includes its private cached matrix first.
    const auto sameFrame = [](const auto& lhs, const auto& rhs) {
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                if (lhs.trs.a.a[row][column] != rhs.trs.a.a[row][column]) return false;
            }
        }
        return lhs.translation.x == rhs.translation.x && lhs.translation.y == rhs.translation.y &&
            lhs.translation.z == rhs.translation.z && lhs.rotation.x == rhs.rotation.x &&
            lhs.rotation.y == rhs.rotation.y && lhs.rotation.z == rhs.rotation.z &&
            lhs.scale.x == rhs.scale.x && lhs.scale.y == rhs.scale.y && lhs.pixelSnap == rhs.pixelSnap;
    };
    require(sameFrame(created->localTransform, createdFrame),
        RetainedString::fromStaticLiteral("redo creation should restore the initialized local frame"));
    require(created->relZSort() == createdSort,
        RetainedString::fromStaticLiteral("redo creation should preserve the initialized relative draw order"));
}

void testNodeCentralizeCommandUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::CentralizeNodeCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto parent = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    parent->setName(NullableString::fromStaticLiteral("centralize-parent"));
    auto childA = Node::createSourceDeferred([&] { return parent; });
    childA->setName(NullableString::fromStaticLiteral("centralize-child-a"));
    auto childB = Node::createSourceDeferred([&] { return parent; });
    childB->setName(NullableString::fromStaticLiteral("centralize-child-b"));
    parent->localTransform.translation = Vec3{0, 0, 0};
    childA->localTransform.translation = Vec3{10, 0, 0};
    childB->localTransform.translation = Vec3{30, 0, 0};
    parent->transformChanged();
    childA->transformChanged();
    childB->transformChanged();
    auto parentBefore = parent->localTransform;
    auto childABefore = childA->localTransform;
    auto childBBefore = childB->localTransform;
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{parent});
    require(ngRequireObject(constructSourceCommand<CentralizeNodeCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("CentralizeNodeCommand should succeed"));
    require(!nearVec2(Vec2{parent->localTransform.translation.x, parent->localTransform.translation.y},
        Vec2{parentBefore.translation.x, parentBefore.translation.y}),
        RetainedString::fromStaticLiteral("centralize should move parent pivot"));
    auto parentAfter = parent->localTransform;
    auto childAAfter = childA->localTransform;
    auto childBAfter = childB->localTransform;
    incActionUndo();
    require(nearVec3(parent->localTransform.translation, parentBefore.translation),
        RetainedString::fromStaticLiteral("centralize undo should restore parent transform"));
    require(nearVec3(childA->localTransform.translation, childABefore.translation),
        RetainedString::fromStaticLiteral("centralize undo should restore child A transform"));
    require(nearVec3(childB->localTransform.translation, childBBefore.translation),
        RetainedString::fromStaticLiteral("centralize undo should restore child B transform"));
    incActionRedo();
    require(nearVec3(parent->localTransform.translation, parentAfter.translation),
        RetainedString::fromStaticLiteral("centralize redo should restore parent pivot"));
    require(nearVec3(childA->localTransform.translation, childAAfter.translation),
        RetainedString::fromStaticLiteral("centralize redo should restore child A transform"));
    require(nearVec3(childB->localTransform.translation, childBAfter.translation),
        RetainedString::fromStaticLiteral("centralize redo should restore child B transform"));
}
