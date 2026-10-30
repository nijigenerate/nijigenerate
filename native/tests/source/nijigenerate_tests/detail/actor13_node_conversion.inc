// Existing regression.d3519-3604; include inside regression::detail after node/world helpers.
void testNodeConvertCommandUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::ConvertToCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Transform;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    using nicxlive::core::nodes::MeshData;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::DynamicComposite;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto node = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    node->setName(NullableString::fromStaticLiteral("convert-source"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{node});
    auto result = constructSourceCommand<ConvertToCommand>(text("GridDeformer"))->run(ctx);
    check(ngRequireObject(result)->succeeded, "ConvertToCommand should succeed");
    check(result->created.size() == 1, "ConvertToCommand should return converted node");
    auto converted = result->created[0];
    check(std::dynamic_pointer_cast<GridDeformer>(converted) != nullptr, "ConvertToCommand should create target node type");
    check(isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, converted),
        "ConvertToCommand should insert converted node under original parent");
    check(!isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, node),
        "ConvertToCommand should remove source node from parent");
    incActionUndo();
    check(isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, node),
        "undo ConvertToCommand should restore source node");
    check(!isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, converted),
        "undo ConvertToCommand should remove converted node");
    incActionRedo();
    check(isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, converted),
        "redo ConvertToCommand should restore converted node");
    check(!isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, node),
        "redo ConvertToCommand should remove source node again");
    resetCase();
    MeshData quad;
    quad.vertices = Vec2Array{Vec2{-5, -5}, Vec2{-5, 5}, Vec2{5, -5}, Vec2{5, 5}};
    quad.uvs = Vec2Array{Vec2{0, 0}, Vec2{0, 1}, Vec2{1, 0}, Vec2{1, 1}};
    quad.indices = {0, 1, 2, 2, 1, 3};
    quad.origin = Vec2{0, 0};
    auto sourceNode = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    sourceNode->setName(NullableString::fromStaticLiteral("node-to-dynamic-source"));
    sourceNode->localTransform = Transform(Vec3{30, -12, 0}, Vec3{0, 0, 0.45f}, Vec2{2.0f, 0.5f});
    sourceNode->transformChanged();
    auto child = Part::createDataSourceDeferred([&] { return quad; },
        [] { return nicxlive::core::common::SharedArray<std::shared_ptr<nicxlive::core::Texture>>{}; },
        [] { return Node::inCreateUUID(); }, [&] { return sourceNode; });
    child->setName(NullableString::fromStaticLiteral("node-to-dynamic-child"));
    child->localTransform = Transform(Vec3{40, -15, 1}, Vec3{0, 0, 0.2f}, Vec2{1.2f, 0.9f});
    child->transformChanged();
    auto before = child->transform();
    auto beforeTranslation = nodeWorldTranslation(child);
    auto convertCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    convertCtx->nodes(Context::Nodes{sourceNode});
    auto dynamicResult = constructSourceCommand<ConvertToCommand>(text("DynamicComposite"))->run(convertCtx);
    check(ngRequireObject(dynamicResult)->succeeded, "ConvertToCommand should convert Node to DynamicComposite");
    check(dynamicResult->created.size() == 1, "Node to DynamicComposite conversion should return one node");
    auto dynamic = std::dynamic_pointer_cast<DynamicComposite>(dynamicResult->created[0]);
    check(dynamic != nullptr, "converted node should be DynamicComposite");
    check(child->parentRef() == dynamic, "conversion should move the child under DynamicComposite");
    check(nearVec3(nodeWorldTranslation(child), beforeTranslation), "Node to DynamicComposite conversion should preserve child world translation");
    check(nearVec3(child->transform().rotation, before.rotation), "Node to DynamicComposite conversion should preserve child world rotation");
    check(nearVec2(child->transform().scale, before.scale), "Node to DynamicComposite conversion should preserve child world scale");
    incActionUndo();
    check(child->parentRef() == sourceNode, "undo Node to DynamicComposite conversion should restore the child under the source node");
    check(nearVec3(nodeWorldTranslation(child), beforeTranslation), "undo Node to DynamicComposite conversion should preserve child world translation");
    check(nearVec3(child->transform().rotation, before.rotation), "undo Node to DynamicComposite conversion should preserve child world rotation");
    check(nearVec2(child->transform().scale, before.scale), "undo Node to DynamicComposite conversion should preserve child world scale");
    incActionRedo();
    check(child->parentRef() == dynamic, "redo Node to DynamicComposite conversion should restore the child under DynamicComposite");
    check(nearVec3(nodeWorldTranslation(child), beforeTranslation), "redo Node to DynamicComposite conversion should preserve child world translation");
    check(nearVec3(child->transform().rotation, before.rotation), "redo Node to DynamicComposite conversion should preserve child world rotation");
    check(nearVec2(child->transform().scale, before.scale), "redo Node to DynamicComposite conversion should preserve child world scale");
}

// Existing regression.d3606-3633; preserve the defining conversion map traversal.
void testNodeTypeConversionMapUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::ConvertToCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::core::actionstack;
    namespace format = nijigenerate::core::translation;
    auto map = nijigenerate::commands::node::base::conversionMap();
    map.forEachRetained([&](RetainedString fromType, nijigenerate::compat::Slice<RetainedString> toTypes) {
        for (const auto toType : toTypes) {
            const auto message = [&](std::string_view pattern) {
                const std::array<format::TypedFormatArgument, 2> arguments{
                    format::ngFormatArgument(fromType), format::ngFormatArgument(toType)};
                auto rendered = format::ngFormatTyped(pattern, arguments);
                return nijigenerate::platform::detail::ngPathAllocation([&] { return RetainedString(std::move(rendered)); });
            };
            resetCase();
            auto node = Node::inInstantiateNodeRetained(fromType,
                ngRequireObject(nijigenerate::project::incActivePuppet())->root);
            const bool present = node != nullptr;
            require(present, concatenateSourceStrings({RetainedString::fromStaticLiteral("conversion source type should instantiate: "), fromType}));
            node->setName(concatenateSourceStrings({fromType, RetainedString::fromStaticLiteral("-to-"), toType}).toNullableString());
            auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
            ctx->nodes(Context::Nodes{node});
            auto command = constructSourceCommand<ConvertToCommand>(toType);
            const bool succeeded = ngRequireObject(command->run(ctx))->succeeded;
            require(succeeded, message("ConvertToCommand should convert %s to %s"));
            const bool oneAction = incActionHistory().size() == 1;
            require(oneAction, message("node conversion should push one undoable action for %s -> %s"));
            const auto children = ngRequireObject(ngRequireObject(nijigenerate::project::incActivePuppet())->root)->children;
            auto converted = children[children.size() - 1];
            const bool sameType = ngRequireObject(converted)->typeId().view() == toType.view();
            require(sameType, message("converted node should have destination type %s -> %s"));
            const bool detached = !isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, node);
            require(detached, message("converted source should be detached for %s -> %s"));
            incActionUndo();
            const bool sourceRestored = isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, node);
            require(sourceRestored, message("undo should restore source node for %s -> %s"));
            const bool convertedDetached = !isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, converted);
            require(convertedDetached, message("undo should detach converted node for %s -> %s"));
            incActionRedo();
            const bool convertedRestored = isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, converted);
            require(convertedRestored, message("redo should restore converted node for %s -> %s"));
            const bool sourceDetached = !isChildOf(ngRequireObject(nijigenerate::project::incActivePuppet())->root, node);
            require(sourceDetached, message("redo should detach source again for %s -> %s"));
        }
    });
}

// Existing regression.d3635-3690; each conversion uses the original transformed child.
void testNodeTypeConversionPreservesChildWorldTransform() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::ConvertToCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Transform;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using namespace nijigenerate::core::actionstack;
    namespace format = nijigenerate::core::translation;
    auto map = nijigenerate::commands::node::base::conversionMap();
    map.forEachRetained([&](RetainedString fromType, nijigenerate::compat::Slice<RetainedString> toTypes) {
        for (const auto toType : toTypes) {
            const auto message = [&](std::string_view pattern) {
                const std::array<format::TypedFormatArgument, 2> arguments{
                    format::ngFormatArgument(fromType), format::ngFormatArgument(toType)};
                auto rendered = format::ngFormatTyped(pattern, arguments);
                return nijigenerate::platform::detail::ngPathAllocation([&] { return RetainedString(std::move(rendered)); });
            };
            resetCase();
            auto node = Node::inInstantiateNodeRetained(fromType,
                ngRequireObject(nijigenerate::project::incActivePuppet())->root);
            const bool present = node != nullptr;
            require(present, concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "conversion source type should instantiate for child transform fixture: "), fromType}));
            node->setName(concatenateSourceStrings({fromType, RetainedString::fromStaticLiteral("-to-"), toType,
                RetainedString::fromStaticLiteral("-world-source")}).toNullableString());
            node->localTransform = Transform(Vec3{30, -12, 0}, Vec3{0, 0, 0.45f}, Vec2{2.0f, 0.5f});
            node->transformChanged();
            auto child = Node::createSourceDeferred([&] { return node; });
            child->setName(concatenateSourceStrings({fromType, RetainedString::fromStaticLiteral("-to-"), toType,
                RetainedString::fromStaticLiteral("-world-child")}).toNullableString());
            child->localTransform = Transform(Vec3{40, -15, 1}, Vec3{0, 0, 0.2f}, Vec2{1.2f, 0.9f});
            child->transformChanged();
            auto before = child->transform();
            auto beforeTranslation = nodeWorldTranslation(child);
            auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
            ctx->nodes(Context::Nodes{node});
            auto command = constructSourceCommand<ConvertToCommand>(toType);
            const bool succeeded = ngRequireObject(command->run(ctx))->succeeded;
            require(succeeded, message("ConvertToCommand should convert %s to %s with a transformed child"));
            const auto children = ngRequireObject(ngRequireObject(nijigenerate::project::incActivePuppet())->root)->children;
            auto converted = children[children.size() - 1];
            const bool sameType = ngRequireObject(converted)->typeId().view() == toType.view();
            require(sameType, message("converted node should have destination type in child transform fixture %s -> %s"));
            const bool translation = nearVec3(nodeWorldTranslation(child), beforeTranslation);
            require(translation, message("conversion should preserve child world translation for %s -> %s"));
            const bool rotation = nearVec3(child->transform().rotation, before.rotation);
            require(rotation, message("conversion should preserve child world rotation for %s -> %s"));
            const bool scale = nearVec2(child->transform().scale, before.scale);
            require(scale, message("conversion should preserve child world scale for %s -> %s"));
            incActionUndo();
            const bool undoTranslation = nearVec3(nodeWorldTranslation(child), beforeTranslation);
            require(undoTranslation, message("undo conversion should preserve child world translation for %s -> %s"));
            const bool undoRotation = nearVec3(child->transform().rotation, before.rotation);
            require(undoRotation, message("undo conversion should preserve child world rotation for %s -> %s"));
            const bool undoScale = nearVec2(child->transform().scale, before.scale);
            require(undoScale, message("undo conversion should preserve child world scale for %s -> %s"));
            incActionRedo();
            const bool redoTranslation = nearVec3(nodeWorldTranslation(child), beforeTranslation);
            require(redoTranslation, message("redo conversion should preserve child world translation for %s -> %s"));
            const bool redoRotation = nearVec3(child->transform().rotation, before.rotation);
            require(redoRotation, message("redo conversion should preserve child world rotation for %s -> %s"));
            const bool redoScale = nearVec2(child->transform().scale, before.scale);
            require(redoScale, message("redo conversion should preserve child world scale for %s -> %s"));
        }
    });
}

// Existing regression.d3692-3812; retain both authored locked and active-offset phases.
void testNodeTypeConversionPreservesLockedAndOffsetChildBaseTransform() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::ConvertToCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Transform;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using namespace nijigenerate::core::actionstack;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    auto map = nijigenerate::commands::node::base::conversionMap();
    map.forEachRetained([&](RetainedString fromType, nijigenerate::compat::Slice<RetainedString> toTypes) {
        for (const auto toType : toTypes) {
            if (fromType.view() == "DynamicComposite" && (toType.view() == "MeshGroup" || toType.view() == "Part")) continue;
            const auto report = [&](bool condition, std::string_view pattern) {
                const std::array<format::TypedFormatArgument, 2> arguments{
                    format::ngFormatArgument(fromType), format::ngFormatArgument(toType)};
                auto rendered = format::ngFormatTyped(pattern, arguments);
                require(condition, nijigenerate::platform::detail::ngPathAllocation([&] {
                    return RetainedString(std::move(rendered));
                }));
            };
            resetCase();
            auto node = Node::inInstantiateNodeRetained(fromType, ngRequireObject(nijigenerate::project::incActivePuppet())->root);
            const bool present = node != nullptr;
            require(present, concatenateSourceStrings({text("conversion source type should instantiate for lockToRoot fixture: "), fromType}));
            node->setName(concatenateSourceStrings({fromType, text("-to-"), toType, text("-locked-source")}).toNullableString());
            node->localTransform = Transform(Vec3{30, -12, 0}, Vec3{0, 0, 0.45f}, Vec2{2.0f, 0.5f});
            node->transformChanged();
            auto lockedChild = Node::createSourceDeferred([&] { return node; });
            lockedChild->setName(concatenateSourceStrings({fromType, text("-to-"), toType, text("-locked-child")}).toNullableString());
            lockedChild->localTransform = Transform(Vec3{40, -15, 1}, Vec3{0, 0, 0.2f}, Vec2{1.2f, 0.9f});
            lockedChild->transformChanged();
            lockedChild->setLockToRoot(true);
            lockedChild->transformChanged();
            auto lockedLocalBefore = lockedChild->localTransform;
            auto lockedWorldBefore = lockedChild->transform();
            auto lockedWorldTranslationBefore = nodeRenderedWorldTranslation(lockedChild);
            auto lockedCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
            lockedCtx->nodes(Context::Nodes{node});
            auto lockedCommand = constructSourceCommand<ConvertToCommand>(toType);
            report(ngRequireObject(lockedCommand->run(lockedCtx))->succeeded,
                "ConvertToCommand should convert %s to %s with a lockToRoot child");
            report(nearVec3(lockedChild->localTransform.translation, lockedLocalBefore.translation),
                "conversion should not rewrite lockToRoot child local translation for %s -> %s");
            report(nearVec3(nodeRenderedWorldTranslation(lockedChild), lockedWorldTranslationBefore),
                "conversion should preserve lockToRoot child world translation for %s -> %s");
            report(nearVec3(lockedChild->transform().rotation, lockedWorldBefore.rotation),
                "conversion should preserve lockToRoot child world rotation for %s -> %s");
            report(nearVec2(lockedChild->transform().scale, lockedWorldBefore.scale),
                "conversion should preserve lockToRoot child world scale for %s -> %s");
            incActionUndo();
            report(nearVec3(lockedChild->localTransform.translation, lockedLocalBefore.translation),
                "undo conversion should not rewrite lockToRoot child local translation for %s -> %s");
            report(nearVec3(nodeRenderedWorldTranslation(lockedChild), lockedWorldTranslationBefore),
                "undo conversion should preserve lockToRoot child world translation for %s -> %s");
            incActionRedo();
            report(nearVec3(lockedChild->localTransform.translation, lockedLocalBefore.translation),
                "redo conversion should not rewrite lockToRoot child local translation for %s -> %s");
            report(nearVec3(nodeRenderedWorldTranslation(lockedChild), lockedWorldTranslationBefore),
                "redo conversion should preserve lockToRoot child world translation for %s -> %s");
            resetCase();
            node = Node::inInstantiateNodeRetained(fromType, ngRequireObject(nijigenerate::project::incActivePuppet())->root);
            const bool offsetPresent = node != nullptr;
            require(offsetPresent, concatenateSourceStrings({text("conversion source type should instantiate for offset fixture: "), fromType}));
            node->setName(concatenateSourceStrings({fromType, text("-to-"), toType, text("-offset-source")}).toNullableString());
            node->localTransform = Transform(Vec3{30, -12, 0}, Vec3{0, 0, 0.45f}, Vec2{2.0f, 0.5f});
            node->transformChanged();
            auto offsetChild = Node::createSourceDeferred([&] { return node; });
            offsetChild->setName(concatenateSourceStrings({fromType, text("-to-"), toType, text("-offset-child")}).toNullableString());
            offsetChild->localTransform = Transform(Vec3{40, -15, 1}, Vec3{0, 0, 0.2f}, Vec2{1.2f, 0.9f});
            check(offsetChild->setValueSource(text("transform.t.x"), 6.0f), "offset child should accept active X translation offset");
            check(offsetChild->setValueSource(text("transform.t.y"), -4.0f), "offset child should accept active Y translation offset");
            check(offsetChild->setValueSource(text("transform.r.z"), 0.15f), "offset child should accept active Z rotation offset");
            check(offsetChild->setValueSource(text("transform.s.x"), 1.1f), "offset child should accept active X scale offset");
            check(offsetChild->setValueSource(text("transform.s.y"), 0.8f), "offset child should accept active Y scale offset");
            auto offsetAppliedBefore = offsetChild->transform();
            auto offsetAppliedTranslationBefore = nodeRenderedWorldTranslation(offsetChild);
            auto offsetCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
            offsetCtx->nodes(Context::Nodes{node});
            auto offsetCommand = constructSourceCommand<ConvertToCommand>(toType);
            report(ngRequireObject(offsetCommand->run(offsetCtx))->succeeded,
                "ConvertToCommand should convert %s to %s with an offset child");
            report(nearVec3(nodeRenderedWorldTranslation(offsetChild), offsetAppliedTranslationBefore),
                "conversion should preserve offset-applied child world translation for %s -> %s");
            report(nearVec3(offsetChild->transform().rotation, offsetAppliedBefore.rotation),
                "conversion should preserve offset-applied child world rotation for %s -> %s");
            report(nearVec2(offsetChild->transform().scale, offsetAppliedBefore.scale),
                "conversion should preserve offset-applied child world scale for %s -> %s");
            report(near(offsetChild->getValueSource(text("transform.t.x")), 6.0f) &&
                near(offsetChild->getValueSource(text("transform.t.y")), -4.0f) &&
                near(offsetChild->getValueSource(text("transform.r.z")), 0.15f) &&
                near(offsetChild->getValueSource(text("transform.s.x")), 1.1f) &&
                near(offsetChild->getValueSource(text("transform.s.y")), 0.8f),
                "conversion should preserve active child transform offset values for %s -> %s");
            incActionUndo();
            report(nearVec3(nodeRenderedWorldTranslation(offsetChild), offsetAppliedTranslationBefore),
                "undo conversion should preserve offset-applied child world translation for %s -> %s");
            report(nearVec3(offsetChild->transform().rotation, offsetAppliedBefore.rotation),
                "undo conversion should preserve offset-applied child world rotation for %s -> %s");
            report(nearVec2(offsetChild->transform().scale, offsetAppliedBefore.scale),
                "undo conversion should preserve offset-applied child world scale for %s -> %s");
            incActionRedo();
            report(nearVec3(nodeRenderedWorldTranslation(offsetChild), offsetAppliedTranslationBefore),
                "redo conversion should preserve offset-applied child world translation for %s -> %s");
            report(nearVec3(offsetChild->transform().rotation, offsetAppliedBefore.rotation),
                "redo conversion should preserve offset-applied child world rotation for %s -> %s");
            report(nearVec2(offsetChild->transform().scale, offsetAppliedBefore.scale),
                "redo conversion should preserve offset-applied child world scale for %s -> %s");
        }
    });
}
