// Existing regression.d4376-4610. Include once inside detail after original PSD helpers.
void testDepthMapCommandsUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using Floats = nijigenerate::compat::Slice<float>;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using Object = nijigenerate::core::settings::SettingsJsonObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::ExCommandResult;
    using nijigenerate::commands::depth::map::DepthMapCommand;
    using nijigenerate::commands::puppet::edit::EditCommand;
    using nijigenerate::ext::nodes::ExGridDeformer;
    using nijigenerate::ext::nodes::ExPathDeformer;
    using nijigenerate::ext::nodes::ExDepthOpType;
    using nijigenerate::io::depthmap_psd::PsdDepthImportResult;
    using nijigenerate::io::depthmap_psd::PsdDepthGridResult;
    using nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor;
    using nijigenerate::viewport::depth::tools::operation::DepthAttachedPointOperation;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::ngVecaFromVectorsSource;
    using nicxlive::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    namespace history = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto equalDepths = [](Floats actual, auto&& expectedProducer) {
        auto expected = expectedProducer();
        if (actual.size() != expected.size()) return false;
        for (std::size_t i = 0; i < actual.size(); ++i) if (!(actual[i] == expected[i])) return false;
        return true;
    };
    resetCase();
    ensureRegressionNodeTypesRegistered();
    auto grid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(text("depth-command-grid").toNullableString());
    grid->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}, Vec2{10, 10}}));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    check(ngCmd<DepthMapCommand::SetDepths>(ctx, grid, Floats{0, 0.25f, -0.5f, 1})->succeeded,
        "SetDepths command should succeed");
    check(equalDepths(grid->copyDepths(), [] { return Floats{0, 0.25f, -0.5f, 1}; }), "SetDepths should apply depths");
    history::incActionUndo();
    check(grid->copyDepths().isNull(), "undo SetDepths should restore null depths");
    history::incActionRedo();
    check(equalDepths(grid->copyDepths(), [] { return Floats{0, 0.25f, -0.5f, 1}; }),
        "redo SetDepths should restore depths");
    check(ngCmd<DepthMapCommand::SetDepths>(ctx, grid, Floats{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.5f})->succeeded,
        "SetDepths should accept and normalize non-finite input");
    check(equalDepths(grid->copyDepths(), [] { return Floats{0, 0, 0, 0.5f}; }),
        "SetDepths must not retain NaN or infinities");
    history::incActionUndo();
    check(equalDepths(grid->copyDepths(), [] { return Floats{0, 0.25f, -0.5f, 1}; }),
        "undo normalized SetDepths should restore previous finite depths");
    PsdDepthImportResult psdImport;
    PsdDepthGridResult psdGridResult;
    psdGridResult.grid = grid;
    psdGridResult.depths = Floats{1, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
    psdImport.grids.append(psdGridResult);
    check(applyPsdDepthImportForRegression(psdImport)->succeeded, "PSD depth import apply should succeed");
    check(equalDepths(grid->copyDepths(), [] { return Floats{1, 0, 0, 0}; }),
        "PSD depth import should replace NaN and infinities with zero before applying");
    check(equalDepths(psdImport.grids[0].depths, [] { return Floats{1, 0, 0, 0}; }),
        "PSD depth import result should not retain non-finite depths");
    history::incActionUndo();
    check(equalDepths(grid->copyDepths(), [] { return Floats{0, 0.25f, -0.5f, 1}; }),
        "undo PSD depth import should restore previous depths");
    history::incActionRedo();
    check(equalDepths(grid->copyDepths(), [] { return Floats{1, 0, 0, 0}; }),
        "redo PSD depth import should restore normalized imported depths");
    auto actionBeforeNoopImport = history::incActionTop();
    check(applyPsdDepthImportForRegression(psdImport)->succeeded,
        "reapplying identical PSD depths should succeed without touching the target");
    check(history::incActionTop() == actionBeforeNoopImport,
        "identical PSD depths must not create an action or refresh an unchanged target");
    auto depthScope = history::ngOpenActionStackScope(history::ActionStackScopeUnit::DepthEdit);
    psdImport.grids[0].depths = Floats{-1, -0.25f, 0.25f, 1};
    check(applyPsdDepthImportForRegression(psdImport)->succeeded,
        "PSD depth import apply should close edit action scopes before pushing undo history");
    check(!depthScope->isActive(), "PSD depth import apply should close the active DepthEdit action scope");
    check(equalDepths(grid->copyDepths(), [] { return Floats{-1, -0.25f, 0.25f, 1}; }),
        "PSD depth import from DepthEdit scope should apply depths");
    check(history::incActionCanUndo(), "PSD depth import from DepthEdit scope should be undoable from the main action stack");
    history::incActionUndo();
    check(equalDepths(grid->copyDepths(), [] { return Floats{1, 0, 0, 0}; }),
        "undo PSD depth import from DepthEdit scope should restore previous imported depths");
    auto secondGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    secondGrid->setName(text("depth-command-second-grid").toNullableString());
    secondGrid->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}, Vec2{8, 0}, Vec2{0, 8}, Vec2{8, 8}}));
    secondGrid->replaceDepths(Floats{0.1f, 0.2f, 0.3f, 0.4f});
    psdImport.grids.resize(0);
    PsdDepthGridResult firstDialogGridResult;
    firstDialogGridResult.grid = grid;
    firstDialogGridResult.depths = Floats{0, 0.1f, 0.2f, 0.3f};
    psdImport.grids.append(firstDialogGridResult);
    PsdDepthGridResult secondDialogGridResult;
    secondDialogGridResult.grid = secondGrid;
    secondDialogGridResult.depths = Floats{1, 1.1f, 1.2f, 1.3f};
    psdImport.grids.append(secondDialogGridResult);
    check(applyPsdDepthImportForRegression(psdImport)->succeeded,
        "PSD depth import dialog apply should update multiple GridDeformers");
    auto groupedImport = std::dynamic_pointer_cast<nijigenerate::actions::GroupAction>(history::incActionTop());
    check(groupedImport && groupedImport->actions.size() == 2,
        "PSD depth import dialog apply should push one GroupAction for all GridDeformer updates");
    check(equalDepths(grid->copyDepths(), [] { return Floats{0, 0.1f, 0.2f, 0.3f}; }),
        "PSD depth import group should apply first grid depths");
    check(equalDepths(secondGrid->copyDepths(), [] { return Floats{1, 1.1f, 1.2f, 1.3f}; }),
        "PSD depth import group should apply second grid depths");
    history::incActionUndo();
    check(equalDepths(grid->copyDepths(), [] { return Floats{1, 0, 0, 0}; }),
        "one undo should restore first grid from grouped PSD depth import");
    check(equalDepths(secondGrid->copyDepths(), [] { return Floats{0.1f, 0.2f, 0.3f, 0.4f}; }),
        "one undo should restore second grid from grouped PSD depth import");
    history::incActionRedo();
    check(equalDepths(grid->copyDepths(), [] { return Floats{0, 0.1f, 0.2f, 0.3f}; }),
        "one redo should reapply first grid from grouped PSD depth import");
    check(equalDepths(secondGrid->copyDepths(), [] { return Floats{1, 1.1f, 1.2f, 1.3f}; }),
        "one redo should reapply second grid from grouped PSD depth import");
    auto listedDepths = std::dynamic_pointer_cast<ExCommandResult<Json>>(ngCmd<DepthMapCommand::ListDepths>(ctx, grid));
    check(listedDepths && listedDepths->succeeded, "ListDepths command should return JSON");
    check(listedDepths->result.sourceIndexValue("count").as_int64() == 4, "ListDepths should report depth count");
    auto path = ExPathDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    path->setName(text("depth-command-path").toNullableString());
    path->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}, Vec2{5, 5}, Vec2{10, 0}}));
    check(ngCmd<DepthMapCommand::SetDepths>(ctx, path, Floats{-0.25f, 0, 0.75f})->succeeded,
        "SetDepths should accept every Deformable DepthMapped target");
    check(equalDepths(path->copyDepths(), [] { return Floats{-0.25f, 0, 0.75f}; }),
        "SetDepths should apply per-vertex depths to PathDeformer targets");
    history::incActionUndo();
    check(path->copyDepths().isNull(), "undo SetDepths should clear newly assigned PathDeformer depths");
    history::incActionRedo();
    check(equalDepths(path->copyDepths(), [] { return Floats{-0.25f, 0, 0.75f}; }),
        "redo SetDepths should restore PathDeformer depths");
    Object attachedFields;
    attachedFields["index"] = Json(1);
    attachedFields["amount"] = Json(0.4);
    check(ngCmd<DepthMapCommand::AddDepthOp>(ctx, grid, depthCommandOp(text("attached-point"), attachedFields), -1)->succeeded,
        "AddDepthOp should add an attached-point op");
    check(grid->copyDepthOps().size() == 1 && grid->copyDepthOps()[0].type == ExDepthOpType::AttachedPoint,
        "AddDepthOp should store attached-point op");
    Object ringFields;
    ringFields["p0"] = jsonVec2Value(0, 0);
    ringFields["p1"] = jsonVec2Value(10, 0);
    ringFields["amount"] = Json(0.75);
    ringFields["width"] = Json(8.0);
    ringFields["hardness"] = Json(0.5);
    ringFields["p0Angle"] = Json(45.0);
    ringFields["p1Angle"] = Json(135.0);
    check(ngCmd<DepthMapCommand::AddDepthOp>(ctx, grid, depthCommandOp(text("ring"), ringFields), -1)->succeeded,
        "AddDepthOp should add a ring op");
    check(grid->copyDepthOps().size() == 2 && grid->copyDepthOps()[1].type == ExDepthOpType::Ring,
        "AddDepthOp should append ring op");
    ringFields["amount"] = Json(-0.25);
    check(ngCmd<DepthMapCommand::UpdateDepthOp>(ctx, grid, 1, depthCommandOp(text("ring"), ringFields))->succeeded,
        "UpdateDepthOp should replace one op");
    check(near(grid->copyDepthOps()[1].amount, -0.25f), "UpdateDepthOp should update ring amount");
    history::incActionUndo();
    check(near(grid->copyDepthOps()[1].amount, 0.75f), "undo UpdateDepthOp should restore previous op");
    history::incActionRedo();
    check(near(grid->copyDepthOps()[1].amount, -0.25f), "redo UpdateDepthOp should restore updated op");
    check(ngCmd<DepthMapCommand::MoveDepthOp>(ctx, grid, 1, 0)->succeeded, "MoveDepthOp should succeed");
    check(grid->copyDepthOps()[0].type == ExDepthOpType::Ring, "MoveDepthOp should reorder ops");
    check(ngCmd<DepthMapCommand::RemoveDepthOp>(ctx, grid, 1)->succeeded, "RemoveDepthOp should succeed");
    check(grid->copyDepthOps().size() == 1, "RemoveDepthOp should remove one op");
    history::incActionUndo();
    check(grid->copyDepthOps().size() == 2, "undo RemoveDepthOp should restore removed op");
    check(ngCmd<DepthMapCommand::ApplyDepthOps>(ctx, grid)->succeeded, "ApplyDepthOps should succeed");
    auto appliedDepths = grid->copyDepths();
    check(!appliedDepths.isNull() && appliedDepths.size() == grid->vertices.size(),
        "ApplyDepthOps should bake depths to vertex count");
    auto listedOps = std::dynamic_pointer_cast<ExCommandResult<Json>>(ngCmd<DepthMapCommand::ListDepthOps>(ctx, grid));
    check(listedOps && listedOps->succeeded, "ListDepthOps command should return JSON");
    check(listedOps->result.sourceIndexValue("count").as_int64() == 2, "ListDepthOps should report operation count");
    check(ngCmd<DepthMapCommand::ClearDepthOps>(ctx, grid)->succeeded, "ClearDepthOps should succeed");
    check(grid->copyDepthOps().size() == 0, "ClearDepthOps should remove all ops");
    history::incActionUndo();
    check(grid->copyDepthOps().size() == 2, "undo ClearDepthOps should restore ops");
    check(ngCmd<DepthMapCommand::ClearDepths>(ctx, grid)->succeeded, "ClearDepths should succeed");
    check(grid->copyDepths().isNull(), "ClearDepths should clear depth array");
    grid->replaceDepths(Floats{0.9f, 0.4f, -0.2f, -0.8f});
    auto editor = DepthMeshEditor::createSource([] { return std::tuple{false}; });
    nijigenerate::compat::ngRunWithScopeExit([&] {
        editor->setTargets(Context::Nodes{grid});
        auto editorOne = editor->getEditorFor(grid);
        auto untouchedOperationDepths = grid->copyDepths();
        editor->applyToTargets();
        check(equalDepths(grid->copyDepths(), [&] { return untouchedOperationDepths; }),
            "DepthEdit Apply without edits must not regenerate depths from unchanged stored operations");
        check(editor->commitOperationAdd(editorOne, DepthAttachedPointOperation::createSource(
            [] { return std::tuple{std::size_t{2}, 0.3f}; })),
            "DepthMeshEditor should commit added depth op through command");
        check(editor->copyOperations(editorOne).size() == 3, "editor should show command-added operation");
        check(grid->copyDepthOps().size() == 2, "editor operation edits should stay local until Apply");
        check(ngCmd<EditCommand::Undo>(ctx)->succeeded, "Undo command should run for editor-added depth op");
        editor->update(nullptr, nullptr);
        check(editor->copyOperations(editorOne).size() == 2, "editor should resync operation list after undo");
        check(ngCmd<EditCommand::Redo>(ctx)->succeeded, "Redo command should run for editor-added depth op");
        editor->update(nullptr, nullptr);
        check(editor->copyOperations(editorOne).size() == 3, "editor should resync operation list after redo");
        editor->closeStack();
        editor->applyToTargets();
        check(grid->copyDepthOps().size() == 3, "editor Apply should save local operation edits through depth-op command");
        auto basedGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        basedGrid->setName(text("depth-op-existing-base").toNullableString());
        basedGrid->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}, Vec2{10, 10}}));
        Floats originalBaseDepths{0.9f, 0.4f, -0.2f, -0.8f};
        basedGrid->replaceDepths(originalBaseDepths);
        auto basedEditor = DepthMeshEditor::createSource([] { return std::tuple{false}; });
        nijigenerate::compat::ngRunWithScopeExit([&] {
            basedEditor->setTargets(Context::Nodes{basedGrid});
            auto basedEditorOne = basedEditor->getEditorFor(basedGrid);
            check(basedEditor->commitOperationAdd(basedEditorOne, DepthAttachedPointOperation::createSource(
                [] { return std::tuple{std::size_t{2}, 0.3f}; })),
                "DepthMeshEditor should add the first operation on top of existing depths");
            auto expectedBasedPreview = basedEditorOne->copyEditorDepths();
            check(expectedBasedPreview.size() == originalBaseDepths.size() &&
                near(expectedBasedPreview[0], originalBaseDepths[0]) &&
                near(expectedBasedPreview[1], originalBaseDepths[1]) &&
                near(expectedBasedPreview[2], originalBaseDepths[2] + 0.3f) &&
                near(expectedBasedPreview[3], originalBaseDepths[3]),
                "the first depth operation preview should preserve the imported/manual base depths");
            basedEditor->closeStack();
            history::incActionClearHistory();
            basedEditor->applyToTargets();
            check(equalDepths(basedGrid->copyDepths(), [&] { return expectedBasedPreview; }) &&
                equalDepths(basedGrid->copyDepthOpBaseDepths(), [&] { return originalBaseDepths; }),
                "DepthMeshEditor Apply must persist the same base-plus-operation depths shown in preview");
            check(ngCmd<EditCommand::Undo>(ctx)->succeeded &&
                equalDepths(basedGrid->copyDepths(), [&] { return originalBaseDepths; }) &&
                basedGrid->copyDepthOps().size() == 0 && basedGrid->copyDepthOpBaseDepths().size() == 0,
                "Undo must restore both depths and the absence of operation base state");
            check(ngCmd<EditCommand::Redo>(ctx)->succeeded &&
                equalDepths(basedGrid->copyDepths(), [&] { return expectedBasedPreview; }) &&
                basedGrid->copyDepthOps().size() == 1 &&
                equalDepths(basedGrid->copyDepthOpBaseDepths(), [&] { return originalBaseDepths; }),
                "Redo must restore the saved operation, its base depths, and the preview-equivalent result");
            auto directlyEditedDepths = expectedBasedPreview.duplicate();
            directlyEditedDepths[0] += 0.2f;
            basedEditor->replaceDirectDepths(basedEditorOne, directlyEditedDepths);
            history::incActionClearHistory();
            basedEditor->applyToTargets();
            check(equalDepths(basedGrid->copyDepths(), [&] { return directlyEditedDepths; }) &&
                basedGrid->copyDepthOps().size() == 0 && basedGrid->copyDepthOpBaseDepths().size() == 0,
                "direct vertex edits should bake the preview and clear the operation recipe");
            check(ngCmd<EditCommand::Undo>(ctx)->succeeded &&
                equalDepths(basedGrid->copyDepths(), [&] { return expectedBasedPreview; }) &&
                basedGrid->copyDepthOps().size() == 1 &&
                equalDepths(basedGrid->copyDepthOpBaseDepths(), [&] { return originalBaseDepths; }),
                "Undo after a direct edit must restore the operation, its base, and its applied depths");
            check(ngCmd<EditCommand::Redo>(ctx)->succeeded &&
                equalDepths(basedGrid->copyDepths(), [&] { return directlyEditedDepths; }) &&
                basedGrid->copyDepthOps().size() == 0 && basedGrid->copyDepthOpBaseDepths().size() == 0,
                "Redo after a direct edit must restore the baked depths without reapplying an operation recipe");
        }, [&] { basedEditor->dispose(); });
    }, [&] { editor->dispose(); });
}
