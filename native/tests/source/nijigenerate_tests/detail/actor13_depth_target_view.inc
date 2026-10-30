// Existing regression.d5889-6128 only; include inside the original detail namespace.
void testDepthTargetViewContracts() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::vertex::define_mesh::DefineGridCommand;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Deformable;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::ext::nodes::ExGridDeformer;
    using nijigenerate::ext::nodes::ExPart;
    using namespace nijigenerate::viewport::depth;
    using namespace nijigenerate::viewport::depth::common::targetview;
    using nijigenerate::viewport::depth::common::session::DepthViewSession;
    using nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto same = [](const auto& left, const auto& right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) if (!(left[i] == right[i])) return false;
        return true;
    };
    const auto sameProduced = [&](auto&& leftExpression, auto&& rightExpression) {
        auto left = leftExpression();
        auto right = rightExpression();
        return same(left, right);
    };
    const auto nearProduced = [&](auto&& leftExpression, auto&& rightExpression) {
        auto left = leftExpression();
        auto right = rightExpression();
        return near(left, right);
    };
    const auto nearVecProduced = [&](auto&& leftExpression, auto&& rightExpression) {
        auto left = leftExpression();
        auto right = rightExpression();
        return nearVec2(left, right);
    };
    resetCase();
    auto grid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(NullableString::fromStaticLiteral("depth-target-view-grid"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{grid});
    struct Axes {
        Slice<float> x, y;
        void prepare(bool second) {
            if (x.isNull()) x = Slice<float>{-20, 0, 20};
            if (second && y.isNull()) y = Slice<float>{-10, 10};
        }
    } axes;
    struct X { Axes& value; operator Slice<float>() && { value.prepare(false); return value.x; } };
    struct Y { Axes& value; operator Slice<float>() && { value.prepare(true); return value.y; } };
    check(std::allocate_shared<DefineGridCommand>(SourceAllocator<DefineGridCommand>{}, X{axes}, Y{axes})
        ->run(ctx)->succeeded, "DepthTargetView fixture should define a rectangular GridDeformer");
    grid->replaceDepths(Slice<float>{0.1f, 0.2f});
    auto view = std::allocate_shared<DepthTargetView>(SourceAllocator<DepthTargetView>{}, grid);
    check(view->getTarget() == grid, "DepthTargetView should retain its target GridDeformer");
    check(view->getVertices().size() == 6, "DepthTargetView should expose target vertices");
    check(view->copyWorkingDepths().size() == grid->vertices.size(),
        "DepthTargetView should normalize short stored depths to vertex count");
    check(near(view->getDepth(0), 0.1f) && near(view->getDepth(1), 0.2f) && near(view->getDepth(5), 0.0f),
        "DepthTargetView should preserve existing stored depths and zero-fill new entries");
    check(view->getIndices().size() == 12, "DepthTargetView should build rectangular grid triangle indices");
    check(view->boundsMin() == Vec2{-20, -10} && view->boundsMax() == Vec2{20, 10},
        "DepthTargetView should compute target bounds from vertices");
    auto childA = ExPart::create(grid);
    childA->setName(NullableString::fromStaticLiteral("equal-depth-child-a"));
    auto childB = ExPart::create(grid);
    childB->setName(NullableString::fromStaticLiteral("equal-depth-child-b"));
    grid->zSort = 1.0f;
    childA->zSort = -1.0f;
    childB->zSort = -1.0f;
    auto drawables = DepthTargetOffscreenTextureRenderer::drawableChildren(grid);
    check(drawables.size() == 2 && drawables[0] == childA && drawables[1] == childB,
        "depth offscreen drawables with equal zSort must preserve hierarchy traversal order");
    auto peer = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    peer->setName(NullableString::fromStaticLiteral("depth-target-view-scale-peer"));
    peer->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{1000, 0}, Vec2{0, 1000}, Vec2{1000, 1000}}));
    peer->replaceDepths(Slice<float>{0, 0, 0, 0});
    nijigenerate::project::incActivePuppet()->rescanNodes();
    auto expectedScale = ngDepthDisplayScaleForTargetsInNodeSpace(nijigenerate::project::incActivePuppet()->root,
        Slice<std::shared_ptr<Deformable>>{grid, peer});
    auto editor = DepthMeshEditor::createSource([] { return std::tuple{false}; });
    nijigenerate::compat::ngRunWithScopeExit([&] {
        editor->setTargets(Context::Nodes{grid});
        check(editor->depthViewSession() != nullptr && editor->depthViewSession()->targetByGrid(grid->uuid) != nullptr,
            "DepthEdit editor should own a shared DepthViewSession for selected targets");
        auto one = editor->getEditorFor(grid);
        check(one != nullptr && editor->targetViewFor(one) == editor->depthViewSession()->targetByGrid(grid->uuid),
            "DepthEdit editor wrapper should resolve its common DepthTargetView");
        check(near(one->depthDisplayScale(), expectedScale),
            "DepthEdit must use one root-space display scale shared by all model depth targets");
        grid->replaceDepths(Slice<float>{0.75f, 0.5f, 0.25f, 0, -0.25f, -0.5f});
        editor->resetFromTargets();
        auto editorView = editor->depthViewSession()->targetByGrid(grid->uuid);
        check(near(editorView->getDepth(0), 0.75f),
            "DepthEdit shared DepthViewSession should reset working depths from the target");
        auto untouched = grid->copyDepths();
        editor->applyToTargets();
        check(same(grid->copyDepths(), untouched), "DepthEdit Apply without edits must preserve imported target depths exactly");
        DepthCamera3D camera;
        camera.yaw = 0.3f;
        camera.pitch = -0.2f;
        camera.zoom = 1.25f;
        camera.pan = Vec2{3, -4};
        check(nearProduced([&] { return one->depthDisplayScale(); }, [&] { return editorView->depthDisplayScale(); }) &&
            nearVecProduced([&] { return one->modelToDepthView(Vec2{10, -5}, 0.4f, camera); },
                [&] { return editorView->modelToDepthView(Vec2{10, -5}, 0.4f, camera); }) &&
            nearVecProduced([&] { return one->depthViewToModel(Vec2{12, -8}, camera, -0.2f); },
                [&] { return editorView->depthViewToModel(Vec2{12, -8}, camera, -0.2f); }),
            "DepthEdit wrapper projection helpers should remain equivalent to the shared DepthTargetView");
        check(nearVecProduced([&] { return one->localToWorld(Vec2{3, 4}); }, [&] { return editorView->localToWorld(Vec2{3, 4}); }) &&
            nearVecProduced([&] { return one->worldToLocal(Vec2{3, 4}); }, [&] { return editorView->worldToLocal(Vec2{3, 4}); }) &&
            nearVecProduced([&] { return one->displayWorldToLocal(Vec2{12, -8}, camera, -0.2f); },
                [&] { return editorView->displayWorldToLocal(Vec2{12, -8}, camera, -0.2f); }),
            "DepthEdit wrapper local/world helpers should remain equivalent to the shared DepthTargetView");
        const auto indexMatches = [&] {
            auto first = one->nearestLocalVertexIndex(Vec2{18, 8});
            auto second = editorView->nearestLocalVertexIndex(Vec2{18, 8});
            return first == second;
        }();
        check(indexMatches && nearProduced([&] { return one->depthAtLocalPoint(Vec2{-20, -10}); },
            [&] { return editorView->depthAtLocalPoint(Vec2{-20, -10}); }),
            "DepthEdit wrapper local vertex/depth helpers should remain equivalent to the shared DepthTargetView");
        one->setDepth(0, 5.0f);
        editorView->setDepth(0, 5.0f);
        one->addDepth(1, -5.0f);
        editorView->addDepth(1, -5.0f);
        check(nearProduced([&] { return one->getDepth(0); }, [&] { return editorView->getDepth(0); }) &&
            nearProduced([&] { return one->getDepth(1); }, [&] { return editorView->getDepth(1); }),
            "DepthEdit wrapper depth edits should clamp and round like the shared DepthTargetView");
        one->replaceEditorDepths(Slice<float>{0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f});
        check(sameProduced([&] { return editorView->copyWorkingDepths(); }, [&] { return one->copyEditorDepths(); }),
            "DepthEdit wrapper should write direct depth changes through the shared DepthTargetView");
        one->replaceBaseDepths(Slice<float>{0, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f});
        one->resetWorkingDepths();
        check(sameProduced([&] { return editorView->copyWorkingDepths(); }, [&] { return editorView->baseDepths; }) &&
            sameProduced([&] { return one->copyEditorDepths(); }, [&] { return editorView->baseDepths; }),
            "DepthEdit wrapper should reset working depths through the shared DepthTargetView base depths");
        view->setDepth(0, 5.0f);
        view->addDepth(1, -5.0f);
        check(near(view->getDepth(0), 5.0f) && near(view->getDepth(1), -4.8f),
            "DepthTargetView should preserve edited depths outside the legacy DepthEdit range");
        check(near(ngDepthTargetClampDepth(5.0f), 5.0f) && near(ngDepthTargetClampDepth(-5.0f), -5.0f) &&
            near(ngDepthTargetClampDepth(0.33349f), 0.333f),
            "DepthTargetView should expose shared finite-value normalization without clipping absolute depth");
        view->replaceWorkingDepths(Slice<float>{0.33349f, -0.33349f, 0, 0, 0, 0});
        view->setDepth(0, view->getDepth(0));
        view->setDepth(1, view->getDepth(1));
        check(near(view->getDepth(0), 0.333f) && near(view->getDepth(1), -0.333f),
            "DepthTargetView should keep DepthEdit depth rounding behavior");
        check(view->nearestLocalVertexIndex(Vec2{18, 8}) >= 0, "DepthTargetView should support nearest local vertex lookup");
        check(near(view->depthAtLocalPoint(Vec2{-20, -10}), 0.333f), "DepthTargetView should read depth at the nearest local vertex");
        DepthCamera3D renderCamera;
        auto renderer = std::allocate_shared<DepthTargetRenderer>(SourceAllocator<DepthTargetRenderer>{});
        std::int32_t width = 0, height = 0;
        Vec2 scale{std::numeric_limits<float>::quiet_NaN()};
        check(ngDepthTargetOffscreenTextureSize(Vec2{0, 0}, Vec2{20000, 20000}, 8192, width, height, scale) &&
            width <= 8192 && height <= 8192 && static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) <=
            DepthTargetOffscreenMaxPixels && scale.x < 1.0f && scale.y < 1.0f,
            "depth offscreen texture sizing must downscale oversized targets before GPU allocation");
        check(!ngDepthTargetOffscreenTextureSize(Vec2{0, 0}, Vec2{std::numeric_limits<float>::infinity(), 1}, 8192,
            width, height, scale), "depth offscreen texture sizing must reject non-finite bounds before GPU allocation");
        auto mesh = renderer->buildMesh(view, renderCamera);
        check(mesh.positions.size() == view->getVertices().size() && mesh.uvs.size() == view->getVertices().size() &&
            same(mesh.indices, view->getIndices()), "DepthTargetRenderer should build projected mesh data from a common DepthTargetView");
        check(view->projectedPoints.size() == mesh.positions.size() && mesh.uvs[0] == Vec2{0, 1} &&
            mesh.uvs[mesh.uvs.size() - 1] == Vec2{1, 0},
            "DepthTargetRenderer should update projected points and derive stable target UVs");
        check(view->projectedVertex(0) == view->projectedPoints[0] && view->nearestProjectedVertex(view->projectedPoints[0], 0.1f) == 0,
            "DepthTargetView should expose projected-point lookup helpers for DepthEdit wrappers");
        auto lines = renderer->buildGridLines(view, renderCamera);
        check(lines.size() == 7, "DepthTargetRenderer should build shared rectangular grid wire lines for DepthEdit and DepthDraw");
        auto line = renderer->buildLine(Vec2{-20, -10}, Vec2{20, -10}, 0.5f, view->depthDisplayScale(), renderCamera);
        check(nearVec2(line.p0, view->projectLocalPoint(Vec2{-20, -10}, 0.5f, renderCamera)) &&
            nearVec2(line.p1, view->projectLocalPoint(Vec2{20, -10}, 0.5f, renderCamera)),
            "DepthTargetRenderer should build reusable projected line overlay geometry");
        auto gap = renderer->buildDepthLine(Vec2{0, 0}, -0.25f, Vec2{0, 0}, 0.75f, view->depthDisplayScale(), renderCamera);
        check(nearVec2(gap.p0, view->projectLocalPoint(Vec2{0, 0}, -0.25f, renderCamera)) &&
            nearVec2(gap.p1, view->projectLocalPoint(Vec2{0, 0}, 0.75f, renderCamera)),
            "DepthTargetRenderer should build reusable depth-space gap marker geometry");
        auto ellipse = renderer->buildEllipseLines(Vec2{0, 0}, 10.0f, 5.0f, 30.0f, 0.25f, view->depthDisplayScale(), renderCamera, 12);
        check(ellipse.size() == 12 && nearVec2(ellipse[0].p0, ellipse[ellipse.size() - 1].p1),
            "DepthTargetRenderer should build closed reusable ellipse overlay geometry");
        auto point = renderer->buildPoint(Vec2{0, 0}, 0.25f, view->depthDisplayScale(), renderCamera, 9.0f);
        check(nearVec2(point.point, view->projectLocalPoint(Vec2{0, 0}, 0.25f, renderCamera)) && near(point.size, 9.0f),
            "DepthTargetRenderer should build reusable projected point overlay geometry");
        auto plane = renderer->buildPlaneLines(Vec2{-20, -10}, Vec2{20, 10}, 0.0f, view->depthDisplayScale(), renderCamera);
        check(plane.size() == 4 && nearVec2(plane[0].p0, plane[plane.size() - 1].p1),
            "DepthTargetRenderer should build closed reusable plane overlay geometry");
        auto range = renderer->buildRangeLines(Vec2{-20, -10}, Vec2{20, 10}, -0.5f, 0.5f, view->depthDisplayScale(), renderCamera);
        check(range.size() == 4 && nearVec2(range[0].p0, view->projectLocalPoint(Vec2{-20, -10}, -0.5f, renderCamera)) &&
            nearVec2(range[1].p0, view->projectLocalPoint(Vec2{-20, 10}, 0.5f, renderCamera)),
            "DepthTargetRenderer should build reusable depth-range overlay geometry");
        view->clearBaseDepths();
        check(view->copyWorkingDepths().size() == grid->vertices.size() && near(view->getDepth(0), 0.0f),
            "DepthTargetView should clear base and working depths to target vertex count");
        auto session = std::allocate_shared<DepthViewSession>(SourceAllocator<DepthViewSession>{});
        check(near(session->camera.zoom, 1.0f) && near(session->camera.yaw, 0.0f) && near(session->camera.pitch, 0.0f),
            "DepthViewSession should own an initialized shared DepthCamera3D");
        session->camera.zoom = 1.5f;
        check(session->ensureTarget(nullptr) == nullptr, "DepthViewSession should ignore null targets");
        auto sessionView = session->ensureTarget(grid);
        check(sessionView != nullptr && sessionView->getTarget() == grid, "DepthViewSession should create a DepthTargetView for a GridDeformer");
        check(session->ensureTarget(grid) == sessionView && session->targets.size() == 1,
            "DepthViewSession should reuse an existing target view for the same GridDeformer");
        check(session->selectedTarget() == sessionView && session->selectTarget(grid->uuid), "DepthViewSession should track the selected target");
        auto second = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        session->setTargets(Slice<std::shared_ptr<Deformable>>{grid, second, grid});
        check(session->targets.size() == 2 && session->targetByGrid(grid->uuid) == sessionView &&
            session->targetByGrid(second->uuid) != nullptr && session->selectedTarget() == sessionView,
            "DepthViewSession target synchronization should preserve existing views, add new targets, and ignore duplicates");
        check(near(session->camera.zoom, 1.5f), "DepthViewSession should preserve the shared camera while synchronizing targets");
        session->setTargets(Slice<std::shared_ptr<Deformable>>{second});
        check(session->targets.size() == 1 && session->targetByGrid(grid->uuid) == nullptr &&
            session->selectedTarget()->getTarget() == second,
            "DepthViewSession target synchronization should remove stale targets and repair stale selection");
        session->setTargets(Slice<std::shared_ptr<Deformable>>{grid});
        sessionView = session->targetByGrid(grid->uuid);
        check(sessionView != nullptr && session->selectedTarget() == sessionView,
            "DepthViewSession target synchronization should restore selected target state for shared apply tests");
        sessionView->replaceWorkingDepths(Slice<float>{1, 1});
        session->resetWorkingDepths();
        check(near(sessionView->getDepth(0), sessionView->baseDepths[0]), "DepthViewSession should reset all target working depths");
        auto before = grid->copyDepths();
        sessionView->replaceWorkingDepths(Slice<float>{0.5f, 0.25f, 0, 0, 0, 0});
        auto action = session->workingDepthsChangeAction(grid->uuid, RetainedStringSlice(text("DepthViewSession test apply")));
        check(action != nullptr && sameProduced([&] { return grid->copyDepths(); }, [&] { return sessionView->copyWorkingDepths(); }),
            "DepthViewSession should expose working-depth apply actions through the shared depth command path");
        action->rollback();
        check(same(grid->copyDepths(), before), "DepthViewSession working-depth apply action should rollback to previous target depths");
        action->redo();
        check(sameProduced([&] { return grid->copyDepths(); }, [&] { return sessionView->copyWorkingDepths(); }),
            "DepthViewSession working-depth apply action should redo the shared working depths");
        check(session->workingDepthsChangeAction(999999, RetainedStringSlice(text("missing target"))) == nullptr,
            "DepthViewSession should reject working-depth apply actions for missing targets");
        session->removeTarget(grid->uuid);
        check(session->targets.size() == 0 && session->selectedGridUuid == 0, "DepthViewSession should remove targets and clear stale selection");
    }, [&] { editor->dispose(); });
}
