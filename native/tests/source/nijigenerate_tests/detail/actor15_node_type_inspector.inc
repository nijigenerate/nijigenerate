// Existing regression.d3217-3380. Include once inside detail after original helpers.
void testNodeTypeInspectorCommandsUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Drawable;
    using nicxlive::core::nodes::Composite;
    using nicxlive::core::nodes::BlendMode;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Transform;
    using nijigenerate::ext::nodes::ExCamera;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::panels::inspector::ngCreateInspectorDeferred;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nijigenerate::actions::camera::CameraResizeAction;
    using namespace nijigenerate::commands::inspector::apply_node;
    namespace history = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto drawable = std::dynamic_pointer_cast<Drawable>(newMeshPart(text("drawable-inspector-target")));
    auto drawableInspector = ngCreateInspectorDeferred<DrawableInspector, Drawable>(
        [&] { return Selection<Drawable>{drawable}; }, [] { return ModelEditSubMode::Layout; });
    auto drawableCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    drawableCtx->nodes(Context::Nodes{drawable});
    drawableCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(drawableInspector)});
    check(constructSourceCommand<OffsetXCommandBase>(4.5f)->run(drawableCtx)->succeeded,
        "Drawable offsetX inspector command should succeed");
    check(near(drawable->getMesh().origin.x, 4.5f), "Drawable offsetX inspector command should apply");
    history::incActionUndo();
    check(near(drawable->getMesh().origin.x, 0.0f), "undo Drawable offsetX should restore old value");
    history::incActionRedo();
    check(near(drawable->getMesh().origin.x, 4.5f), "redo Drawable offsetX should restore new value");
    history::incActionClearHistory();
    check(constructSourceCommand<OffsetYCommandBase>(-3.25f)->run(drawableCtx)->succeeded,
        "Drawable offsetY inspector command should succeed");
    check(near(drawable->getMesh().origin.y, -3.25f), "Drawable offsetY inspector command should apply");
    history::incActionUndo();
    check(near(drawable->getMesh().origin.y, 0.0f), "undo Drawable offsetY should restore old value");
    history::incActionRedo();
    check(near(drawable->getMesh().origin.y, -3.25f), "redo Drawable offsetY should restore new value");
    auto composite = Composite::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    composite->setName(text("composite-inspector-target").toNullableString());
    auto compositeInspector = ngCreateInspectorDeferred<CompositeInspector, Composite>(
        [&] { return Selection<Composite>{composite}; }, [] { return ModelEditSubMode::Layout; });
    auto compositeCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    compositeCtx->nodes(Context::Nodes{composite});
    compositeCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(compositeInspector)});
    history::incActionClearHistory();
    check(constructSourceCommand<CompositeTintCommandBase>(Vec3{0.2f, 0.4f, 0.6f})->run(compositeCtx)->succeeded,
        "Composite tint inspector command should succeed");
    check(nearVec3(composite->tint, Vec3{0.2f, 0.4f, 0.6f}), "Composite tint inspector command should apply");
    history::incActionUndo();
    check(nearVec3(composite->tint, Vec3{1, 1, 1}), "undo Composite tint should restore default");
    history::incActionRedo();
    check(nearVec3(composite->tint, Vec3{0.2f, 0.4f, 0.6f}), "redo Composite tint should restore new value");
    history::incActionClearHistory();
    check(constructSourceCommand<CompositeScreenTintCommandBase>(Vec3{0.3f, 0.2f, 0.1f})->run(compositeCtx)->succeeded,
        "Composite screenTint inspector command should succeed");
    check(nearVec3(composite->screenTint, Vec3{0.3f, 0.2f, 0.1f}), "Composite screenTint inspector command should apply");
    history::incActionUndo();
    check(nearVec3(composite->screenTint, Vec3{0, 0, 0}), "undo Composite screenTint should restore default");
    history::incActionRedo();
    check(nearVec3(composite->screenTint, Vec3{0.3f, 0.2f, 0.1f}), "redo Composite screenTint should restore new value");
    history::incActionClearHistory();
    check(constructSourceCommand<CompositeOpacityCommandBase>(0.35f)->run(compositeCtx)->succeeded,
        "Composite opacity inspector command should succeed");
    check(near(composite->opacity, 0.35f), "Composite opacity inspector command should apply");
    history::incActionUndo();
    check(near(composite->opacity, 1.0f), "undo Composite opacity should restore default");
    history::incActionRedo();
    check(near(composite->opacity, 0.35f), "redo Composite opacity should restore new value");
    history::incActionClearHistory();
    check(constructSourceCommand<CompositeThresholdCommandBase>(0.7f)->run(compositeCtx)->succeeded,
        "Composite threshold inspector command should succeed");
    check(near(composite->threshold(), 0.7f), "Composite threshold inspector command should apply");
    history::incActionUndo();
    check(near(composite->threshold(), 0.5f), "undo Composite threshold should restore default");
    history::incActionRedo();
    check(near(composite->threshold(), 0.7f), "redo Composite threshold should restore new value");
    history::incActionClearHistory();
    check(constructSourceCommand<CompositeBlendingModeCommandBase>(BlendMode::Screen)->run(compositeCtx)->succeeded,
        "Composite blendingMode inspector command should succeed");
    check(composite->blendMode == BlendMode::Screen, "Composite blendingMode inspector command should apply");
    history::incActionUndo();
    check(composite->blendMode == BlendMode::Normal, "undo Composite blendingMode should restore default");
    history::incActionRedo();
    check(composite->blendMode == BlendMode::Screen, "redo Composite blendingMode should restore new value");
    auto camera = ExCamera::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    camera->setName(text("camera-inspector-target").toNullableString());
    auto cameraInspector = ngCreateInspectorDeferred<CameraInspector, ExCamera>(
        [&] { return Selection<ExCamera>{camera}; }, [] { return ModelEditSubMode::Layout; });
    auto cameraCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    cameraCtx->nodes(Context::Nodes{camera});
    cameraCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(cameraInspector)});
    history::incActionClearHistory();
    check(constructSourceCommand<CameraViewportCommandBase>(Vec2{1280, 720})->run(cameraCtx)->succeeded,
        "Camera viewport inspector command should succeed");
    check(nearVec2(camera->getViewport(), Vec2{1280, 720}), "Camera viewport inspector command should apply");
    history::incActionUndo();
    check(nearVec2(camera->getViewport(), Vec2{1920, 1080}), "undo Camera viewport should restore default");
    history::incActionRedo();
    check(nearVec2(camera->getViewport(), Vec2{1280, 720}), "redo Camera viewport should restore new value");
    history::incActionClearHistory();
    check(constructSourceCommand<CameraViewportCommandBase>(Vec2{1279, 721})->run(cameraCtx)->succeeded,
        "Camera viewport inspector command should accept odd input");
    check(nearVec2(camera->getViewport(), Vec2{1280, 722}), "Camera viewport should normalize odd input to even dimensions");
    history::incActionUndo();
    check(nearVec2(camera->getViewport(), Vec2{1280, 720}),
        "undo odd Camera viewport normalization should restore previous even viewport");
    history::incActionRedo();
    check(nearVec2(camera->getViewport(), Vec2{1280, 722}),
        "redo odd Camera viewport normalization should restore normalized even viewport");
    history::incActionClearHistory();
    auto child = Node::createSourceDeferred([&] { return camera; });
    child->setName(text("camera-child-transform-target").toNullableString());
    child->localTransform = Transform(Vec3{10, -20, 0}, Vec3{0, 0, 0.1f}, Vec2{1.5f, 0.75f});
    child->localTransform.update();
    camera->transformChanged();
    auto childWorldBefore = child->transform();
    auto oldChildTransform = child->localTransform;
    auto oldTransform = camera->localTransform;
    auto oldViewport = camera->getViewport();
    camera->localTransform.scale = Vec2{-0.501f, 2};
    camera->localTransform.update();
    auto sourceChildren = camera->children;
    CameraResizeAction::Children cameraChildren(sourceChildren.begin(), sourceChildren.end());
    CameraResizeAction::ChildTransforms oldChildTransforms;
    for (auto cameraChild : cameraChildren) {
        auto uuid = cameraChild->getUUID();
        auto transform = cameraChild->localTransform;
        oldChildTransforms[uuid] = transform;
    }
    camera->foldScaleIntoViewport();
    CameraResizeAction::ChildTransforms newChildTransforms;
    for (auto cameraChild : cameraChildren) {
        auto uuid = cameraChild->getUUID();
        auto transform = cameraChild->localTransform;
        newChildTransforms[uuid] = transform;
    }
    auto resizeAction = CameraResizeAction::createSource([&] {
        auto receiver = camera;
        auto beforeTransform = oldTransform;
        auto beforeViewport = oldViewport;
        auto afterTransform = camera->localTransform;
        auto afterViewport = camera->getViewport();
        auto children = cameraChildren;
        auto beforeChildren = oldChildTransforms;
        auto afterChildren = newChildTransforms;
        return std::tuple{receiver, beforeTransform, beforeViewport, afterTransform, afterViewport,
            children, beforeChildren, afterChildren};
    });
    history::incActionPush(resizeAction);
    check(nearVec2(camera->getViewport(), Vec2{642, 1444}),
        "Camera resize should fold scale into viewport and normalize to even dimensions");
    check(nearVec2(camera->localTransform.scale, Vec2{-1, 1}), "Camera resize should preserve scale sign after folding into viewport");
    check(nearVec3(child->transform().translation, childWorldBefore.translation), "Camera resize should preserve child world translation");
    check(nearVec3(child->transform().rotation, childWorldBefore.rotation), "Camera resize should preserve child world rotation");
    check(nearVec2(child->transform().scale, childWorldBefore.scale), "Camera resize should preserve child world scale");
    history::incActionUndo();
    check(nearVec2(camera->getViewport(), Vec2{1280, 722}), "undo Camera resize should restore viewport");
    check(nearVec2(camera->localTransform.scale, oldTransform.scale), "undo Camera resize should restore transform scale");
    check(nearVec3(child->localTransform.translation, oldChildTransform.translation), "undo Camera resize should restore child local translation");
    check(nearVec3(child->localTransform.rotation, oldChildTransform.rotation), "undo Camera resize should restore child local rotation");
    check(nearVec2(child->localTransform.scale, oldChildTransform.scale), "undo Camera resize should restore child local scale");
    history::incActionRedo();
    check(nearVec2(camera->getViewport(), Vec2{642, 1444}), "redo Camera resize should restore folded even viewport");
    check(nearVec2(camera->localTransform.scale, Vec2{-1, 1}), "redo Camera resize should restore folded scale sign");
    check(nearVec3(child->transform().translation, childWorldBefore.translation), "redo Camera resize should preserve child world translation");
    check(nearVec3(child->transform().rotation, childWorldBefore.rotation), "redo Camera resize should preserve child world rotation");
    check(nearVec2(child->transform().scale, childWorldBefore.scale), "redo Camera resize should preserve child world scale");
    camera->setViewport(Vec2{320, 200});
    camera->localTransform.scale = Vec2{2, 0.5f};
    camera->localTransform.update();
    camera->transformChanged();
    childWorldBefore = child->transform();
    auto adapter = nijigenerate::viewport::common::transformhandle::ngViewportTransformHandleAdapter(camera);
    adapter->beginScale(camera);
    adapter->applyScale(camera, Vec2{2, 0.5f});
    check(nearVec2(camera->getViewport(), Vec2{320, 200}), "Camera handle resize should not change viewport without effective drag");
    check(nearVec2(camera->localTransform.scale, Vec2{2, 0.5f}), "Camera handle resize should preserve existing scale magnitude");
    adapter->applyScale(camera, Vec2{4, 1});
    check(nearVec2(camera->getViewport(), Vec2{640, 400}), "Camera handle resize should apply drag ratio to viewport");
    check(nearVec2(camera->localTransform.scale, Vec2{2, 0.5f}), "Camera handle resize should keep existing scale after viewport resize");
    check(nearVec3(child->transform().translation, childWorldBefore.translation), "Camera handle resize should preserve child world translation");
    check(nearVec3(child->transform().rotation, childWorldBefore.rotation), "Camera handle resize should preserve child world rotation");
    check(nearVec2(child->transform().scale, childWorldBefore.scale), "Camera handle resize should preserve child world scale");
}
