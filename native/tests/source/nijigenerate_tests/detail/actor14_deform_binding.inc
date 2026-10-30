// Existing regression.d13789-13951; include inside regression::detail after helpers.
void testParameterDeformBindingModelCommandUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::model::set_deform_binding::SetDeformBindingCommand;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto param = new2DParameter(RetainedString::fromStaticLiteral("DeformBinding"));
    auto part = newMeshPart(RetainedString::fromStaticLiteral("deform-binding-target"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    ctx->parameters(Context::Parameters{param});
    ctx->nodes(Context::Nodes{part});
    ctx->keyPoint({2, 2});
    ctx->hasExplicitKeyPoint(true);
    struct OffsetsArgument {
        operator Slice<float>() && { return Slice<float>{1, 2, 3, 4, 5, 6}; }
    };
    auto result = std::allocate_shared<SetDeformBindingCommand>(SourceAllocator<SetDeformBindingCommand>{},
        RetainedStringSlice(RetainedString::fromStaticLiteral("deform")), OffsetsArgument{})->run(ctx);
    require(ngRequireObject(result)->succeeded,
        RetainedString::fromStaticLiteral("SetDeformBindingCommand should succeed"));
    auto binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
        param->getBinding(part, NullableString::fromStaticLiteral("deform")));
    require(binding != nullptr,
        RetainedString::fromStaticLiteral("SetDeformBindingCommand should create deformation binding"));
    require(ngRequireObject(binding)->isSet({2, 2}),
        RetainedString::fromStaticLiteral("SetDeformBindingCommand should set requested keypoint"));
    require(ngRequireObject(binding)->getValue({2, 2}).vertexOffsets.size() == part->getVertices().size(),
        RetainedString::fromStaticLiteral("deformation binding offsets should match target vertices"));
    const auto offset = ngRequireObject(binding)->getValue({2, 2}).vertexOffsets[0];
    require(offset.x != 0 || offset.y != 0,
        RetainedString::fromStaticLiteral("deformation binding should store non-zero offsets"));
    incActionUndo();
    require(param->getBinding(part, NullableString::fromStaticLiteral("deform")) == nullptr,
        RetainedString::fromStaticLiteral("undo SetDeformBindingCommand should remove newly-created deformation binding"));
    incActionRedo();
    binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
        param->getBinding(part, NullableString::fromStaticLiteral("deform")));
    require(binding != nullptr,
        RetainedString::fromStaticLiteral("redo SetDeformBindingCommand should restore deformation binding"));
    require(ngRequireObject(binding)->getValue({2, 2}).vertexOffsets.size() == part->getVertices().size(),
        RetainedString::fromStaticLiteral("redo SetDeformBindingCommand should restore offset length"));
}

void testParameterRotationXBindingPartUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::param::ValueParameterBinding;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::model::set_deform_binding::SetRotationXBindingCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto param = new2DParameter(RetainedString::fromStaticLiteral("RotationXBinding"));
    auto part = newMeshPart(RetainedString::fromStaticLiteral("rotation-x-binding-target"));
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    ctx->parameters(Context::Parameters{param});
    ctx->nodes(Context::Nodes{part});
    ctx->parameterValue(param->getKeypointValue({2, 2}));
    require(ngRequireObject(constructSourceCommand<SetRotationXBindingCommand>(0.4f)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("X rotation should create a Part binding using parameter-axis values"));
    auto rx = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(part, NullableString::fromStaticLiteral("transform.r.x")));
    require(rx != nullptr && near(ngRequireObject(rx)->getValue({2, 2}), 0.4f),
        RetainedString::fromStaticLiteral("X rotation should write radians to the requested key"));
    require(param->getBinding(part, NullableString::fromStaticLiteral("transform.r.y")) == nullptr &&
        param->getBinding(part, NullableString::fromStaticLiteral("transform.r.z")) == nullptr,
        RetainedString::fromStaticLiteral("X rotation should not create bindings for other axes"));
    incActionUndo();
    require(param->getBinding(part, NullableString::fromStaticLiteral("transform.r.x")) == nullptr,
        RetainedString::fromStaticLiteral("Undo should remove the newly created X rotation binding"));
    incActionRedo();
    rx = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(part, NullableString::fromStaticLiteral("transform.r.x")));
    require(rx != nullptr && near(ngRequireObject(rx)->getValue({2, 2}), 0.4f),
        RetainedString::fromStaticLiteral("Redo should restore the X rotation binding and value"));
    require(ngRequireObject(constructSourceCommand<SetRotationXBindingCommand>(0)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("X rotation should accept an explicit zero on an existing binding"));
    require(param->getBinding(part, NullableString::fromStaticLiteral("transform.r.x")) == rx &&
        near(ngRequireObject(rx)->getValue({2, 2}), 0),
        RetainedString::fromStaticLiteral("Setting zero should update the existing binding"));
    incActionUndo();
    require(near(ngRequireObject(rx)->getValue({2, 2}), 0.4f),
        RetainedString::fromStaticLiteral("Undo should restore the previous X rotation"));
    incActionRedo();
    require(near(ngRequireObject(rx)->getValue({2, 2}), 0),
        RetainedString::fromStaticLiteral("Redo should restore zero X rotation"));
}

void testParameterTRSBindingModelCommandUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::ValueParameterBinding;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::model::set_deform_binding::SetTRSBindingCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto param = new2DParameter(RetainedString::fromStaticLiteral("TRSBinding"));
    auto node = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    node->setName(NullableString::fromStaticLiteral("trs-target"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    ctx->parameters(Context::Parameters{param});
    ctx->nodes(Context::Nodes{node});
    ctx->keyPoint({2, 2});
    ctx->hasExplicitKeyPoint(true);
    struct LiteralFrame {
        Slice<float> translation;
        Slice<float> scale;
        void prepare(bool includeScale) {
            if (translation.isNull()) translation = Slice<float>{10, 20};
            if (includeScale && scale.isNull()) scale = Slice<float>{2, 3};
        }
    } literals;
    struct TranslationArgument {
        LiteralFrame& frame;
        operator Slice<float>() && { frame.prepare(false); return frame.translation; }
    };
    struct ScaleArgument {
        LiteralFrame& frame;
        operator Slice<float>() && { frame.prepare(true); return frame.scale; }
    };
    auto result = std::allocate_shared<SetTRSBindingCommand>(SourceAllocator<SetTRSBindingCommand>{},
        TranslationArgument{literals}, ScaleArgument{literals}, 45.0f, true)->run(ctx);
    require(ngRequireObject(result)->succeeded,
        RetainedString::fromStaticLiteral("SetTRSBindingCommand should succeed"));
    auto tx = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(node, NullableString::fromStaticLiteral("transform.t.x")));
    auto ty = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(node, NullableString::fromStaticLiteral("transform.t.y")));
    auto sx = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(node, NullableString::fromStaticLiteral("transform.s.x")));
    auto sy = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(node, NullableString::fromStaticLiteral("transform.s.y")));
    auto rz = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(node, NullableString::fromStaticLiteral("transform.r.z")));
    require(tx != nullptr && ty != nullptr && sx != nullptr && sy != nullptr && rz != nullptr,
        RetainedString::fromStaticLiteral("SetTRSBindingCommand should create TRS bindings"));
    require(near(ngRequireObject(tx)->getValue({2, 2}), 10.0f),
        RetainedString::fromStaticLiteral("SetTRSBindingCommand should set translation X"));
    require(near(ngRequireObject(ty)->getValue({2, 2}), 20.0f),
        RetainedString::fromStaticLiteral("SetTRSBindingCommand should set translation Y"));
    require(near(ngRequireObject(sx)->getValue({2, 2}), 2.0f),
        RetainedString::fromStaticLiteral("SetTRSBindingCommand should set scale X"));
    require(near(ngRequireObject(sy)->getValue({2, 2}), 3.0f),
        RetainedString::fromStaticLiteral("SetTRSBindingCommand should set scale Y"));
    require(near(ngRequireObject(rz)->getValue({2, 2}), static_cast<float>(45.0 * 3.14159265358979323846 / 180.0)),
        RetainedString::fromStaticLiteral("SetTRSBindingCommand should set rotation in radians"));
    incActionUndo();
    require(param->getBinding(node, NullableString::fromStaticLiteral("transform.t.x")) == nullptr,
        RetainedString::fromStaticLiteral("undo SetTRSBindingCommand should remove newly-created TRS bindings"));
    incActionRedo();
    tx = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(node, NullableString::fromStaticLiteral("transform.t.x")));
    rz = std::dynamic_pointer_cast<ValueParameterBinding>(
        param->getBinding(node, NullableString::fromStaticLiteral("transform.r.z")));
    require(tx != nullptr && near(ngRequireObject(tx)->getValue({2, 2}), 10.0f),
        RetainedString::fromStaticLiteral("redo SetTRSBindingCommand should restore translation binding"));
    require(rz != nullptr && near(ngRequireObject(rz)->getValue({2, 2}), static_cast<float>(45.0 * 3.14159265358979323846 / 180.0)),
        RetainedString::fromStaticLiteral("redo SetTRSBindingCommand should restore rotation binding"));
}

void testParameterMeshBindingCompositeWorkflow() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::vertex::define_mesh::DefineMeshCommand;
    using nijigenerate::commands::model::set_deform_binding::SetDeformBindingCommand;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto part = newMeshPart(RetainedString::fromStaticLiteral("mesh-binding-composite-target"));
    auto meshCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    meshCtx->nodes(Context::Nodes{std::static_pointer_cast<Node>(part)});
    struct MeshLiteralFrame {
        Slice<float> vertices;
        Slice<std::uint16_t> indices;
        void prepare(bool includeIndices) {
            if (vertices.isNull()) vertices = Slice<float>{0, 0, 20, 0, 20, 20, 0, 20};
            if (includeIndices && indices.isNull()) indices = Slice<std::uint16_t>{0, 1, 2, 0, 2, 3};
        }
    } mesh;
    struct VerticesArgument {
        MeshLiteralFrame& frame;
        operator Slice<float>() && { frame.prepare(false); return frame.vertices; }
    };
    struct IndicesArgument {
        MeshLiteralFrame& frame;
        operator Slice<std::uint16_t>() && { frame.prepare(true); return frame.indices; }
    };
    require(ngRequireObject(std::allocate_shared<DefineMeshCommand>(SourceAllocator<DefineMeshCommand>{},
        VerticesArgument{mesh}, IndicesArgument{mesh})->run(meshCtx))->succeeded,
        RetainedString::fromStaticLiteral("composite mesh-binding should define a four-vertex mesh"));
    require(part->getMesh().vertices.size() == 4,
        RetainedString::fromStaticLiteral("composite mesh-binding mesh should have four vertices"));
    require(part->deformation.size() == 4,
        RetainedString::fromStaticLiteral("composite mesh-binding deformation should follow mesh topology"));
    auto param = new2DParameter(RetainedString::fromStaticLiteral("MeshBindingComposite"));
    auto bindCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    bindCtx->puppet(nijigenerate::project::incActivePuppet());
    bindCtx->parameters(Context::Parameters{param});
    bindCtx->nodes(Context::Nodes{std::static_pointer_cast<Node>(part)});
    bindCtx->keyPoint({2, 2});
    bindCtx->hasExplicitKeyPoint(true);
    struct OffsetsArgument {
        operator Slice<float>() && { return Slice<float>{1, 2, 3, 4, 5, 6, 7, 8}; }
    };
    require(ngRequireObject(std::allocate_shared<SetDeformBindingCommand>(SourceAllocator<SetDeformBindingCommand>{},
        RetainedStringSlice(RetainedString::fromStaticLiteral("deform")), OffsetsArgument{})->run(bindCtx))->succeeded,
        RetainedString::fromStaticLiteral("composite mesh-binding should create deformation binding after topology change"));
    auto binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
        param->getBinding(part, NullableString::fromStaticLiteral("deform")));
    require(binding != nullptr,
        RetainedString::fromStaticLiteral("composite mesh-binding should have deformation binding"));
    require(ngRequireObject(binding)->getValue({2, 2}).vertexOffsets.size() == part->getVertices().size(),
        RetainedString::fromStaticLiteral("composite mesh-binding offsets should match current topology"));
    incActionUndo();
    require(param->getBinding(part, NullableString::fromStaticLiteral("deform")) == nullptr,
        RetainedString::fromStaticLiteral("undo binding should not undo prior mesh topology"));
    require(part->getMesh().vertices.size() == 4,
        RetainedString::fromStaticLiteral("undo binding should keep four-vertex mesh"));
    incActionUndo();
    require(part->getMesh().vertices.size() == 3,
        RetainedString::fromStaticLiteral("undo mesh should restore original topology"));
    require(part->deformation.size() == 3,
        RetainedString::fromStaticLiteral("undo mesh should restore deformation length"));
    incActionRedo();
    require(part->getMesh().vertices.size() == 4,
        RetainedString::fromStaticLiteral("redo mesh should restore topology before binding redo"));
    require(part->deformation.size() == 4,
        RetainedString::fromStaticLiteral("redo mesh should restore deformation length"));
    incActionRedo();
    binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
        param->getBinding(part, NullableString::fromStaticLiteral("deform")));
    require(binding != nullptr,
        RetainedString::fromStaticLiteral("redo binding should restore deformation binding"));
    require(ngRequireObject(binding)->getValue({2, 2}).vertexOffsets.size() == part->getMesh().vertices.size(),
        RetainedString::fromStaticLiteral("redo binding should restore offsets matching redone topology"));
}
