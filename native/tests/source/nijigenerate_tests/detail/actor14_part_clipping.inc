// Existing regression.d3003-3100,3382-3413; include inside regression::detail after helpers.
void testPartClippingMaskPropertiesUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::inspector::apply_node;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    using nicxlive::core::nodes::BlendMode;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto part = newMeshPart(RetainedString::fromStaticLiteral("part-clipping-target"));
    auto inspector = nijigenerate::panels::inspector::ngCreateInspectorDeferred<PartInspector, Part>(
        [&] { return Selection<Part>{part}; }, [] { return ModelEditSubMode::Layout; });
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{part});
    ctx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(inspector)});
    require(ngRequireObject(constructSourceCommand<PartMaskAlphaThresholdCommandBase>(0.81f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Part mask threshold inspector command should succeed"));
    require(near(part->maskAlphaThreshold, 0.81f),
        RetainedString::fromStaticLiteral("Part mask threshold inspector command should apply"));
    incActionUndo();
    require(near(part->maskAlphaThreshold, 0.5f),
        RetainedString::fromStaticLiteral("undo Part mask threshold should restore default"));
    incActionRedo();
    require(near(part->maskAlphaThreshold, 0.81f),
        RetainedString::fromStaticLiteral("redo Part mask threshold should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<PartBlendingModeCommandBase>(BlendMode::ClipToLower)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("ClipToLower blend mode command should succeed"));
    require(part->blendMode == BlendMode::ClipToLower,
        RetainedString::fromStaticLiteral("ClipToLower blend mode command should apply"));
    incActionUndo();
    require(part->blendMode == BlendMode::Normal,
        RetainedString::fromStaticLiteral("undo ClipToLower blend mode should restore default"));
    incActionRedo();
    require(part->blendMode == BlendMode::ClipToLower,
        RetainedString::fromStaticLiteral("redo ClipToLower blend mode should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<PartBlendingModeCommandBase>(BlendMode::SliceFromLower)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("SliceFromLower blend mode command should succeed"));
    require(part->blendMode == BlendMode::SliceFromLower,
        RetainedString::fromStaticLiteral("SliceFromLower blend mode command should apply"));
    incActionUndo();
    require(part->blendMode == BlendMode::ClipToLower,
        RetainedString::fromStaticLiteral("undo SliceFromLower blend mode should restore previous value"));
    incActionRedo();
    require(part->blendMode == BlendMode::SliceFromLower,
        RetainedString::fromStaticLiteral("redo SliceFromLower blend mode should restore new value"));
}

void testPartInspectorPropertiesUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::inspector::apply_node;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    using nicxlive::core::nodes::BlendMode;
    using nicxlive::core::math::Vec3;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto part = newMeshPart(RetainedString::fromStaticLiteral("part-inspector-target"));
    auto inspector = nijigenerate::panels::inspector::ngCreateInspectorDeferred<PartInspector, Part>(
        [&] { return Selection<Part>{part}; }, [] { return ModelEditSubMode::Layout; });
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{part});
    ctx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(inspector)});
    require(ngRequireObject(constructSourceCommand<PartTintCommandBase>(Vec3{0.25f, 0.5f, 0.75f})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Part tint inspector command should succeed"));
    require(nearVec3(part->tint, Vec3{0.25f, 0.5f, 0.75f}),
        RetainedString::fromStaticLiteral("Part tint inspector command should apply"));
    incActionUndo();
    require(nearVec3(part->tint, Vec3{1, 1, 1}),
        RetainedString::fromStaticLiteral("undo Part tint should restore default"));
    incActionRedo();
    require(nearVec3(part->tint, Vec3{0.25f, 0.5f, 0.75f}),
        RetainedString::fromStaticLiteral("redo Part tint should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<PartScreenTintCommandBase>(Vec3{0.1f, 0.2f, 0.3f})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Part screen tint inspector command should succeed"));
    require(nearVec3(part->screenTint, Vec3{0.1f, 0.2f, 0.3f}),
        RetainedString::fromStaticLiteral("Part screen tint inspector command should apply"));
    incActionUndo();
    require(nearVec3(part->screenTint, Vec3{0, 0, 0}),
        RetainedString::fromStaticLiteral("undo Part screen tint should restore default"));
    incActionRedo();
    require(nearVec3(part->screenTint, Vec3{0.1f, 0.2f, 0.3f}),
        RetainedString::fromStaticLiteral("redo Part screen tint should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<PartEmissionStrengthCommandBase>(2.5f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Part emission strength inspector command should succeed"));
    require(near(part->emissionStrength, 2.5f),
        RetainedString::fromStaticLiteral("Part emission strength inspector command should apply"));
    incActionUndo();
    require(near(part->emissionStrength, 1.0f),
        RetainedString::fromStaticLiteral("undo Part emission strength should restore default"));
    incActionRedo();
    require(near(part->emissionStrength, 2.5f),
        RetainedString::fromStaticLiteral("redo Part emission strength should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<PartOpacityCommandBase>(0.42f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Part opacity inspector command should succeed"));
    require(near(part->opacity, 0.42f),
        RetainedString::fromStaticLiteral("Part opacity inspector command should apply"));
    incActionUndo();
    require(near(part->opacity, 1.0f),
        RetainedString::fromStaticLiteral("undo Part opacity should restore default"));
    incActionRedo();
    require(near(part->opacity, 0.42f),
        RetainedString::fromStaticLiteral("redo Part opacity should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<PartBlendingModeCommandBase>(BlendMode::Multiply)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("Part blending mode inspector command should succeed"));
    require(part->blendMode == BlendMode::Multiply,
        RetainedString::fromStaticLiteral("Part blending mode inspector command should apply"));
    incActionUndo();
    require(part->blendMode == BlendMode::Normal,
        RetainedString::fromStaticLiteral("undo Part blending mode should restore default"));
    incActionRedo();
    require(part->blendMode == BlendMode::Multiply,
        RetainedString::fromStaticLiteral("redo Part blending mode should restore new value"));
}

void testNodeInspectorTransformUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::inspector::apply_node;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::common::NullableString;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto node = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    node->setName(NullableString::fromStaticLiteral("inspector-target"));
    auto inspector = nijigenerate::panels::inspector::ngCreateInspectorDeferred<NodeApplyInspector, Node>(
        [&] { return Selection<Node>{node}; }, [] { return ModelEditSubMode::Layout; });
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{node});
    ctx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(inspector)});
    require(ngRequireObject(constructSourceCommand<TranslationXCommandBase>(12.5f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("TranslationX inspector command should succeed"));
    require(near(node->localTransform.translation.x, 12.5f),
        RetainedString::fromStaticLiteral("translation X inspector command should apply"));
    incActionUndo();
    require(near(node->localTransform.translation.x, 0.0f),
        RetainedString::fromStaticLiteral("undo translation X inspector command should restore old value"));
    incActionRedo();
    require(near(node->localTransform.translation.x, 12.5f),
        RetainedString::fromStaticLiteral("redo translation X inspector command should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<RotationZCommandBase>(0.5f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("RotationZ inspector command should succeed"));
    require(near(node->localTransform.rotation.z, 0.5f),
        RetainedString::fromStaticLiteral("rotation Z inspector command should apply"));
    incActionUndo();
    require(near(node->localTransform.rotation.z, 0.0f),
        RetainedString::fromStaticLiteral("undo rotation Z inspector command should restore old value"));
    incActionRedo();
    require(near(node->localTransform.rotation.z, 0.5f),
        RetainedString::fromStaticLiteral("redo rotation Z inspector command should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<ScaleXCommandBase>(1.75f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("ScaleX inspector command should succeed"));
    require(near(node->localTransform.scale.x, 1.75f),
        RetainedString::fromStaticLiteral("scale X inspector command should apply"));
    incActionUndo();
    require(near(node->localTransform.scale.x, 1.0f),
        RetainedString::fromStaticLiteral("undo scale X inspector command should restore old value"));
    incActionRedo();
    require(near(node->localTransform.scale.x, 1.75f),
        RetainedString::fromStaticLiteral("redo scale X inspector command should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<ZSortCommandBase>(3.0f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("ZSort inspector command should succeed"));
    require(near(node->zSort(), 3.0f),
        RetainedString::fromStaticLiteral("z-sort inspector command should apply"));
    incActionUndo();
    require(near(node->zSort(), 0.0f),
        RetainedString::fromStaticLiteral("undo z-sort inspector command should restore old value"));
    incActionRedo();
    require(near(node->zSort(), 3.0f),
        RetainedString::fromStaticLiteral("redo z-sort inspector command should restore new value"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<LockToRootCommandBase>(true)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("LockToRoot inspector command should succeed"));
    require(node->lockToRoot,
        RetainedString::fromStaticLiteral("lock-to-root inspector command should apply"));
    incActionUndo();
    require(!node->lockToRoot,
        RetainedString::fromStaticLiteral("undo lock-to-root inspector command should restore old value"));
    incActionRedo();
    require(node->lockToRoot,
        RetainedString::fromStaticLiteral("redo lock-to-root inspector command should restore new value"));
}
