// Existing regression.d19612-19791,19974-20062; include inside regression::detail after helpers.
void testGridDeformerToolVirtualMeshApplyUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Deformable;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::project::EditMode;
    using nijigenerate::viewport::common::mesheditor::tools::grid::GridTool;
    using Operation = nijigenerate::viewport::vertex::mesheditor::deformable::
        IncMeshEditorOneFor<Deformable, EditMode::VertexEdit>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto grid = GridDeformer::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    grid->setName(NullableString::fromStaticLiteral("grid-tool-target"));
    auto original = grid->getVertices().dup();
    require(original.size() == 4,
        RetainedString::fromStaticLiteral("GridDeformer tool fixture should start from the default 2x2 grid"));
    auto op = std::allocate_shared<Operation>(SourceAllocator<Operation>{});
    op->setTarget(grid);
    auto tool = std::allocate_shared<GridTool>(SourceAllocator<GridTool>{});
    tool->numCut = 3;
    tool->resetVirtualMeshAsEmpty(op);
    require(!op->vertexMapDirty,
        RetainedString::fromStaticLiteral("empty virtual grid reset should not mark target dirty before editing"));
    require(tool->onDragStart(Vec2{0, 0}, op),
        RetainedString::fromStaticLiteral("GridTool should start creating an empty virtual grid by drag"));
    require(tool->onDragEnd(Vec2{30, 20}, op),
        RetainedString::fromStaticLiteral("GridTool should finish virtual grid creation on drag end"));
    require(op->vertexMapDirty,
        RetainedString::fromStaticLiteral("virtual grid creation should mark the operation dirty"));
    require(tool->applyVirtualMeshToTarget(op),
        RetainedString::fromStaticLiteral("GridTool should apply the virtual mesh to the GridDeformer target"));
    require(grid->getVertices().size() == 9,
        RetainedString::fromStaticLiteral("GridTool apply should create a 3x3 GridDeformer"));
    require(grid->deformation.size() == 9,
        RetainedString::fromStaticLiteral("GridTool apply should keep deformation length aligned with vertices"));
    incActionUndo();
    require(nearVec2Array(grid->getVertices(), original),
        RetainedString::fromStaticLiteral("undo GridTool apply should restore the original GridDeformer vertices"));
    incActionRedo();
    require(grid->getVertices().size() == 9,
        RetainedString::fromStaticLiteral("redo GridTool apply should restore the created grid"));
}

void testPathDeformerToolApplyUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::nodes::PathDeformer;
    using nicxlive::core::nodes::Deformable;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::project::EditMode;
    using nijigenerate::viewport::common::mesheditor::tools::bezierdeform::BezierDeformTool;
    using nijigenerate::viewport::common::mesheditor::tools::select::NodeSelect;
    using Operation = nijigenerate::viewport::vertex::mesheditor::deformable::
        IncMeshEditorOneFor<Deformable, EditMode::VertexEdit>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto path = PathDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    path->setName(NullableString::fromStaticLiteral("path-tool-target"));
    path->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-100, 0}, Vec2{100, 0}}));
    auto original = path->getVertices().dup();
    auto op = std::allocate_shared<Operation>(SourceAllocator<Operation>{});
    op->setTarget(path);
    auto tool = std::allocate_shared<BezierDeformTool>(SourceAllocator<BezierDeformTool>{});
    bool changed = false;
    op->mousePos = Vec2{0, 0};
    tool->updateVertexEdit(nullptr, op, static_cast<std::int32_t>(BezierDeformTool::BezierDeformActionID::AddPoint), changed);
    require(op->vertices.size() == 3,
        RetainedString::fromStaticLiteral("BezierDeformTool should add a control point to the operation"));
    op->applyToTarget();
    require(path->getVertices().size() == 3 && containsVec2(path->getVertices(), Vec2{0, 0}),
        RetainedString::fromStaticLiteral("PathDeformer apply should persist the inserted control point"));
    incActionUndo();
    require(nearVec2Array(path->getVertices(), original),
        RetainedString::fromStaticLiteral("undo PathDeformer apply should restore original control points"));
    incActionRedo();
    require(path->getVertices().size() == 3 && containsVec2(path->getVertices(), Vec2{0, 0}),
        RetainedString::fromStaticLiteral("redo PathDeformer apply should restore inserted control point"));
    op->setTarget(path);
    auto middle = op->getVertexFromPoint(Vec2{0, 0});
    require(middle != std::numeric_limits<std::uint64_t>::max(),
        RetainedString::fromStaticLiteral("PathDeformer tool fixture should find the inserted point"));
    op->selectOne(middle);
    op->lastMousePos = Vec2{0, 0};
    op->mousePos = Vec2{0, 20};
    tool->updateVertexEdit(nullptr, op, static_cast<std::int32_t>(NodeSelect::SelectActionID::StartDrag), changed);
    require(changed,
        RetainedString::fromStaticLiteral("a single motion must move the selected point on the drag-start update"));
    require(tool->onDragEnd(op->mousePos, op),
        RetainedString::fromStaticLiteral("BezierDeformTool should finish dragging a selected point"));
    bool operationContainsMovedPoint = false;
    for (auto v : op->vertices) {
        if (nearVec2(ngRequireObject(v)->position, Vec2{0, 20})) {
            operationContainsMovedPoint = true;
            break;
        }
    }
    require(operationContainsMovedPoint,
        RetainedString::fromStaticLiteral("BezierDeformTool drag should move the selected operation point"));
    op->applyToTarget();
    require(containsVec2(path->getVertices(), Vec2{0, 20}),
        RetainedString::fromStaticLiteral("PathDeformer apply should persist the moved control point"));
    incActionUndo();
    require(containsVec2(path->getVertices(), Vec2{0, 0}),
        RetainedString::fromStaticLiteral("undo moved PathDeformer apply should restore the previous point location"));
    incActionRedo();
    require(containsVec2(path->getVertices(), Vec2{0, 20}),
        RetainedString::fromStaticLiteral("redo moved PathDeformer apply should restore the moved point location"));
}

void testMeshEditorMultiObjectApplyUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::core::math::mesh::MeshVertex;
    using nijigenerate::viewport::vertex::mesheditor::editor::VertexMeshEditor;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto gridA = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    gridA->setName(NullableString::fromStaticLiteral("multi-grid-a"));
    auto gridB = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    gridB->setName(NullableString::fromStaticLiteral("multi-grid-b"));
    auto originalA = gridA->getVertices().dup();
    auto originalB = gridB->getVertices().dup();
    require(originalA.size() > 0 && originalB.size() > 0,
        RetainedString::fromStaticLiteral("multi-object fixture should have editable grids"));
    auto editor = VertexMeshEditor::createSource();
    nijigenerate::compat::ngRunWithScopeExit([&] {
        editor->setTargets(Slice<std::shared_ptr<Node>>{gridA, gridB});
        auto opA = editor->getEditorFor(gridA);
        auto opB = editor->getEditorFor(gridB);
        require(opA != nullptr && opB != nullptr,
            RetainedString::fromStaticLiteral("multi-object editor should create an operation for every target"));
        auto columnA = ngRequireObject(ngRequireObject(opA)->getVerticesByIndex(Slice<std::uint64_t>{0}, true)[0])->position.x;
        auto columnB = ngRequireObject(ngRequireObject(opB)->getVerticesByIndex(Slice<std::uint64_t>{0}, true)[0])->position.x;
        auto movedA = ngRequireObject(opA->getVerticesByIndex(Slice<std::uint64_t>{0}, true)[0])->position + Vec2{7, 0};
        auto movedB = ngRequireObject(opB->getVerticesByIndex(Slice<std::uint64_t>{0}, true)[0])->position + Vec2{-5, 0};
        using Filter = std::function<bool(MeshVertex*)>;
        auto filterA = std::allocate_shared<Filter>(SourceAllocator<Filter>{}, [columnA](MeshVertex* v) {
            return near(ngRequireObject(v)->position.x, columnA);
        });
        for (auto idx : opA->filterVertices(filterA)) {
            auto v = opA->getVerticesByIndex(Slice<std::uint64_t>{idx}, true)[0];
            opA->moveMeshVertex(v, ngRequireObject(v)->position + Vec2{7, 0});
        }
        auto filterB = std::allocate_shared<Filter>(SourceAllocator<Filter>{}, [columnB](MeshVertex* v) {
            return near(ngRequireObject(v)->position.x, columnB);
        });
        for (auto idx : opB->filterVertices(filterB)) {
            auto v = opB->getVerticesByIndex(Slice<std::uint64_t>{idx}, true)[0];
            opB->moveMeshVertex(v, ngRequireObject(v)->position + Vec2{-5, 0});
        }
        editor->applyToTarget();
        require(containsVec2(gridA->getVertices(), movedA),
            RetainedString::fromStaticLiteral("multi-object apply should update the first GridDeformer"));
        require(containsVec2(gridB->getVertices(), movedB),
            RetainedString::fromStaticLiteral("multi-object apply should update the second GridDeformer"));
        incActionUndo();
        require(nearVec2Array(gridA->getVertices(), originalA),
            RetainedString::fromStaticLiteral("one undo should restore the first multi-object target"));
        require(nearVec2Array(gridB->getVertices(), originalB),
            RetainedString::fromStaticLiteral("one undo should restore the second multi-object target"));
        incActionRedo();
        require(containsVec2(gridA->getVertices(), movedA),
            RetainedString::fromStaticLiteral("one redo should restore the first multi-object target"));
        require(containsVec2(gridB->getVertices(), movedB),
            RetainedString::fromStaticLiteral("one redo should restore the second multi-object target"));
    }, [&] { editor->closeScope(); });
}

void testMeshEditorMirrorSymmetryContracts() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::vertex::define_mesh::DefineGridCommand;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Deformable;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::core::math::mesh::MeshVertex;
    using nijigenerate::project::EditMode;
    using Operation = nijigenerate::viewport::vertex::mesheditor::deformable::
        IncMeshEditorOneFor<Deformable, EditMode::VertexEdit>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    const auto containsX = [](Vec2Array values, float x) {
        for (auto value : values) if (near(value.x, x)) return true;
        return false;
    };
    auto grid = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    grid->setName(NullableString::fromStaticLiteral("mirror-grid"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{grid});
    struct AxesFrame {
        Slice<float> x;
        Slice<float> y;
        void prepare(bool includeY) {
            if (x.isNull()) x = Slice<float>{-100, 0, 100};
            if (includeY && y.isNull()) y = Slice<float>{-5, 5};
        }
    } axes;
    struct XArgument {
        AxesFrame& frame;
        operator Slice<float>() && { frame.prepare(false); return frame.x; }
    };
    struct YArgument {
        AxesFrame& frame;
        operator Slice<float>() && { frame.prepare(true); return frame.y; }
    };
    require(ngRequireObject(std::allocate_shared<DefineGridCommand>(SourceAllocator<DefineGridCommand>{},
        XArgument{axes}, YArgument{axes})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("mirror fixture grid should be defined"));
    auto original = grid->getVertices().dup();
    auto op = std::allocate_shared<Operation>(SourceAllocator<Operation>{});
    op->setTarget(grid);
    op->mirrorHoriz = true;
    op->mirrorOrigin = Vec2{0, 0};
    auto left = op->getVertexFromPoint(Vec2{-100, -5});
    auto right = op->getVertexFromPoint(Vec2{100, -5});
    require(left != std::numeric_limits<std::uint64_t>::max() &&
        right != std::numeric_limits<std::uint64_t>::max() && left != right,
        RetainedString::fromStaticLiteral("mirror fixture should find paired vertices"));
    require(op->mirrorVertex(1, left) == right,
        RetainedString::fromStaticLiteral("horizontal mirror should find the opposite vertex"));
    const auto mirrored = op->mirrorDelta(1, Vec2{3, -2});
    require(mirrored.x == -3 && mirrored.y == -2,
        RetainedString::fromStaticLiteral("horizontal mirror should invert only the X delta"));
    op->select(left);
    require(std::find(op->mirrorSelected.begin(), op->mirrorSelected.end(), right) != op->mirrorSelected.end(),
        RetainedString::fromStaticLiteral("selecting one side should track the mirrored counterpart"));
    auto delta = Vec2{2, 0};
    auto leftMoved = ngRequireObject(op->getVerticesByIndex(Slice<std::uint64_t>{left}, true)[0])->position + delta;
    const auto rightPosition = ngRequireObject(op->getVerticesByIndex(Slice<std::uint64_t>{right}, true)[0])->position;
    auto rightMoved = rightPosition + op->mirrorDelta(1, delta);
    using Filter = std::function<bool(MeshVertex*)>;
    for (auto idx : op->filterVertices(std::allocate_shared<Filter>(SourceAllocator<Filter>{}, [](MeshVertex* v) {
        return near(ngRequireObject(v)->position.x, -100);
    }))) {
        auto v = op->getVerticesByIndex(Slice<std::uint64_t>{idx}, true)[0];
        op->moveMeshVertex(v, ngRequireObject(v)->position + delta);
    }
    for (auto idx : op->filterVertices(std::allocate_shared<Filter>(SourceAllocator<Filter>{}, [](MeshVertex* v) {
        return near(ngRequireObject(v)->position.x, 100);
    }))) {
        auto v = op->getVerticesByIndex(Slice<std::uint64_t>{idx}, true)[0];
        const auto position = ngRequireObject(v)->position;
        const auto mirroredDelta = op->mirrorDelta(1, delta);
        op->moveMeshVertex(v, position + mirroredDelta);
    }
    op->resetMesh();
    require(nearVec2Array(grid->getVertices(), original),
        RetainedString::fromStaticLiteral("reset before apply should leave the target unchanged"));
    require(nearVec2(ngRequireObject(op->getVerticesByIndex(Slice<std::uint64_t>{left}, true)[0])->position, original[left]),
        RetainedString::fromStaticLiteral("reset before apply should cancel the local mirrored edit"));
    for (auto idx : op->filterVertices(std::allocate_shared<Filter>(SourceAllocator<Filter>{}, [](MeshVertex* v) {
        return near(ngRequireObject(v)->position.x, -100);
    }))) {
        auto v = op->getVerticesByIndex(Slice<std::uint64_t>{idx}, true)[0];
        op->moveMeshVertex(v, ngRequireObject(v)->position + delta);
    }
    for (auto idx : op->filterVertices(std::allocate_shared<Filter>(SourceAllocator<Filter>{}, [](MeshVertex* v) {
        return near(ngRequireObject(v)->position.x, 100);
    }))) {
        auto v = op->getVerticesByIndex(Slice<std::uint64_t>{idx}, true)[0];
        const auto position = ngRequireObject(v)->position;
        const auto mirroredDelta = op->mirrorDelta(1, delta);
        op->moveMeshVertex(v, position + mirroredDelta);
    }
    op->applyToTarget();
    require(containsVec2(grid->getVertices(), leftMoved),
        RetainedString::fromStaticLiteral("mirrored apply should update the edited vertex"));
    require(containsX(grid->getVertices(), rightMoved.x),
        RetainedString::fromStaticLiteral("mirrored apply should update the counterpart column"));
    incActionUndo();
    require(nearVec2Array(grid->getVertices(), original),
        RetainedString::fromStaticLiteral("undo should restore mirrored GridDeformer vertices"));
    incActionRedo();
    require(containsVec2(grid->getVertices(), leftMoved),
        RetainedString::fromStaticLiteral("redo should restore mirrored edited vertex"));
    require(containsX(grid->getVertices(), rightMoved.x),
        RetainedString::fromStaticLiteral("redo should restore mirrored counterpart column"));
}

void testMeshEditorOperationTargets() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::core::math::mesh::MeshVertex;
    using nijigenerate::project::EditMode;
    using NodeOp = nijigenerate::viewport::common::mesheditor::operations::node::
        IncMeshEditorOneFor<Node, EditMode::ModelEdit>;
    using DeformOp = nijigenerate::viewport::vertex::mesheditor::deformable::
        IncMeshEditorOneFor<GridDeformer, EditMode::VertexEdit>;
    using DrawableOp = nijigenerate::viewport::vertex::mesheditor::drawable::
        IncMeshEditorOneFor<Part, EditMode::VertexEdit>;
    using Filter = std::function<bool(MeshVertex*)>;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto node = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    node->setName(NullableString::fromStaticLiteral("operation-node"));
    node->setValueSource(RetainedString::fromStaticLiteral("transform.t.x"), 10.0f);
    node->setValueSource(RetainedString::fromStaticLiteral("transform.t.y"), 20.0f);
    auto nodeOp = std::allocate_shared<NodeOp>(SourceAllocator<NodeOp>{}, false);
    nodeOp->setTarget(node);
    require(nodeOp->getTarget() == node,
        RetainedString::fromStaticLiteral("Node operation should keep its target"));
    require(nodeOp->getVertexFromPoint(Vec2{10, 20}) == 0,
        RetainedString::fromStaticLiteral("Node operation should hit-test the node translation"));
    require(nodeOp->getVertexFromPoint(Vec2{200, 200}) == std::numeric_limits<std::uint64_t>::max(),
        RetainedString::fromStaticLiteral("Node operation should reject distant hit tests"));
    const auto sameIndices = [](const auto& lhs, const auto& rhs) {
        return lhs.size() == rhs.size() &&
            (!lhs.size() || std::memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(std::uint64_t)) == 0);
    };
    const auto normalRectangle = nodeOp->getInRect(Vec2{0, 0}, Vec2{20, 30}, 0);
    require(sameIndices(normalRectangle, Slice<std::uint64_t>{0}),
        RetainedString::fromStaticLiteral("Node operation should select translation inside a rectangle"));
    const auto invertedRectangle = nodeOp->getInRect(Vec2{20, 30}, Vec2{0, 0}, 0);
    require(sameIndices(invertedRectangle, Slice<std::uint64_t>{0}),
        RetainedString::fromStaticLiteral("Node operation should normalize inverted rectangle bounds"));
    auto nodeVertices = nodeOp->getVerticesByIndex(Slice<std::uint64_t>{0, 1}, false);
    require(nodeVertices.size() == 2 && nodeVertices[0] != nullptr && nodeVertices[1] == nullptr,
        RetainedString::fromStaticLiteral("Node operation should expose only the synthetic translation vertex"));
    require(nearVec2(ngRequireObject(nodeVertices[0])->position, Vec2{10, 20}),
        RetainedString::fromStaticLiteral("Node operation synthetic vertex should match translation"));
    const auto filteredTranslation = nodeOp->filterVertices(std::allocate_shared<Filter>(SourceAllocator<Filter>{},
        [](MeshVertex* v) { return nearVec2(ngRequireObject(v)->position, Vec2{10, 20}); }));
    require(sameIndices(filteredTranslation, Slice<std::uint64_t>{0}),
        RetainedString::fromStaticLiteral("Node operation should filter its translation vertex"));
    auto grid = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    grid->setName(NullableString::fromStaticLiteral("operation-grid"));
    auto gridOriginal = grid->getVertices().dup();
    require(gridOriginal.size() > 0,
        RetainedString::fromStaticLiteral("GridDeformer fixture should create vertices"));
    auto deformOp = std::allocate_shared<DeformOp>(SourceAllocator<DeformOp>{});
    deformOp->setTarget(grid);
    require(deformOp->getTarget() == grid,
        RetainedString::fromStaticLiteral("Deformable operation should keep its target"));
    auto deformVertex = deformOp->getVerticesByIndex(Slice<std::uint64_t>{0}, true)[0];
    auto deformColumnX = ngRequireObject(deformVertex)->position.x;
    auto deformMoved = deformVertex->position + Vec2{3, 0};
    auto columnFilter = std::allocate_shared<Filter>(SourceAllocator<Filter>{}, [deformColumnX](MeshVertex* v) {
        return near(ngRequireObject(v)->position.x, deformColumnX);
    });
    for (auto candidate : deformOp->filterVertices(columnFilter)) {
        auto destination = deformOp->getVerticesByIndex(Slice<std::uint64_t>{candidate}, true)[0];
        const auto position = ngRequireObject(deformOp->getVerticesByIndex(
            Slice<std::uint64_t>{candidate}, true)[0])->position;
        deformOp->moveMeshVertex(destination, position + Vec2{3, 0});
    }
    deformOp->applyToTarget();
    require(containsVec2(grid->getVertices(), deformMoved),
        RetainedString::fromStaticLiteral("Deformable operation should apply moved vertices to GridDeformer"));
    require(grid->getVertices().size() == gridOriginal.size(),
        RetainedString::fromStaticLiteral("Deformable operation should preserve vertex count for a move"));
    incActionUndo();
    require(nearVec2Array(grid->getVertices(), gridOriginal),
        RetainedString::fromStaticLiteral("Deformable operation undo should restore GridDeformer vertices"));
    incActionRedo();
    require(containsVec2(grid->getVertices(), deformMoved),
        RetainedString::fromStaticLiteral("Deformable operation redo should restore moved GridDeformer vertex"));
    auto part = newMeshPart(RetainedString::fromStaticLiteral("operation-drawable"));
    auto drawableOriginal = part->getMesh().vertices.dup();
    require(drawableOriginal.size() > 0,
        RetainedString::fromStaticLiteral("Drawable fixture should create mesh vertices"));
    auto drawableOp = std::allocate_shared<DrawableOp>(SourceAllocator<DrawableOp>{});
    drawableOp->setTarget(part);
    require(drawableOp->getTarget() == part,
        RetainedString::fromStaticLiteral("Drawable operation should keep its target"));
    auto drawableVertex = drawableOp->getVerticesByIndex(Slice<std::uint64_t>{0}, true)[0];
    auto drawableMoved = ngRequireObject(drawableVertex)->position + Vec2{-2, 6};
    drawableOp->moveMeshVertex(drawableVertex, drawableMoved);
    drawableOp->applyToTarget();
    require(nearVec2(part->getMesh().vertices[0], drawableMoved),
        RetainedString::fromStaticLiteral("Drawable operation should apply moved vertices to Part mesh"));
    require(part->getMesh().vertices.size() == drawableOriginal.size(),
        RetainedString::fromStaticLiteral("Drawable operation should preserve vertex count for a move"));
    incActionUndo();
    require(nearVec2Array(part->getMesh().vertices, drawableOriginal),
        RetainedString::fromStaticLiteral("Drawable operation undo should restore mesh vertices"));
    incActionRedo();
    require(nearVec2(part->getMesh().vertices[0], drawableMoved),
        RetainedString::fromStaticLiteral("Drawable operation redo should restore moved mesh vertex"));
}
