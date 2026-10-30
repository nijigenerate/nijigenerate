// Existing regression.d13012-13079,13092-13151,13953-14025; include inside detail after helpers.
void testParameterCommandRenameAndGroupUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::parameter::param::Add1DParameterCommand;
    using nijigenerate::commands::parameter::prop::SetParameterNameCommand;
    using namespace nijigenerate::commands::parameter::group;
    using nicxlive::core::common::NullableString;
    using nijigenerate::ext::ExParameter;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto addResult = constructSourceCommand<Add1DParameterCommand>(-1, 1)->run(ctx);
    require(ngRequireObject(addResult)->succeeded,
        RetainedString::fromStaticLiteral("Add1DParameterCommand should succeed"));
    auto param = std::dynamic_pointer_cast<ExParameter>(
        ngRequireObject(nijigenerate::project::incActivePuppet())->parameters[0]);
    require(param != nullptr,
        RetainedString::fromStaticLiteral("parameter command should create ExParameter"));
    ctx->parameters(Context::Parameters{param});
    require(ngRequireObject(constructSourceCommand<SetParameterNameCommand>(
        RetainedString::fromStaticLiteral("Renamed Param"))->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("SetParameterNameCommand should succeed"));
    require(ngRequireObject(param)->name == NullableString::fromStaticLiteral("Renamed Param"),
        RetainedString::fromStaticLiteral("parameter rename command should apply"));
    incActionUndo();
    require(ngRequireObject(param)->name != NullableString::fromStaticLiteral("Renamed Param"),
        RetainedString::fromStaticLiteral("undo SetParameterNameCommand should restore old name"));
    incActionRedo();
    require(ngRequireObject(param)->name == NullableString::fromStaticLiteral("Renamed Param"),
        RetainedString::fromStaticLiteral("redo SetParameterNameCommand should restore new name"));
    auto groupResult = constructSourceCommand<CreateParamGroupCommand>()->run(ctx);
    require(ngRequireObject(groupResult)->succeeded,
        RetainedString::fromStaticLiteral("CreateParamGroupCommand should succeed"));
    auto group = ngRequireObject(groupResult)->created[0];
    require(hasGroup(group),
        RetainedString::fromStaticLiteral("CreateParamGroupCommand should add a parameter group"));
    incActionUndo();
    require(!hasGroup(group),
        RetainedString::fromStaticLiteral("undo CreateParamGroupCommand should remove the group"));
    incActionRedo();
    require(hasGroup(group),
        RetainedString::fromStaticLiteral("redo CreateParamGroupCommand should restore the group"));
    ctx->parameters(Context::Parameters{param});
    require(ngRequireObject(constructSourceCommand<MoveParameterCommand>(group, 0)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("MoveParameterCommand should succeed"));
    require(ngRequireObject(param)->getParent() == group,
        RetainedString::fromStaticLiteral("MoveParameterCommand should attach parameter to group"));
    incActionUndo();
    require(ngRequireObject(param)->getParent() == nullptr,
        RetainedString::fromStaticLiteral("undo MoveParameterCommand should detach parameter from group"));
    incActionRedo();
    require(ngRequireObject(param)->getParent() == group,
        RetainedString::fromStaticLiteral("redo MoveParameterCommand should attach parameter to group again"));
    ctx->parameters(Context::Parameters{group});
    require(ngRequireObject(constructSourceCommand<ChangeGroupColorCommand>(
        std::array<float, 3>{0.25f, 0.5f, 0.75f})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("ChangeGroupColorCommand should succeed"));
    require(ngRequireObject(group)->color.x == 0.25f && group->color.y == 0.5f && group->color.z == 0.75f,
        RetainedString::fromStaticLiteral("ChangeGroupColorCommand should apply"));
    incActionUndo();
    require(ngRequireObject(group)->color.x != 0.25f || group->color.y != 0.5f || group->color.z != 0.75f,
        RetainedString::fromStaticLiteral("undo ChangeGroupColorCommand should restore old color"));
    incActionRedo();
    require(ngRequireObject(group)->color.x == 0.25f && group->color.y == 0.5f && group->color.z == 0.75f,
        RetainedString::fromStaticLiteral("redo ChangeGroupColorCommand should restore new color"));
    require(ngRequireObject(constructSourceCommand<DeleteParamGroupCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("DeleteParamGroupCommand should succeed"));
    require(!hasGroup(group),
        RetainedString::fromStaticLiteral("DeleteParamGroupCommand should remove group"));
    require(ngRequireObject(param)->getParent() == nullptr,
        RetainedString::fromStaticLiteral("DeleteParamGroupCommand should detach children"));
    incActionUndo();
    require(hasGroup(group),
        RetainedString::fromStaticLiteral("undo DeleteParamGroupCommand should restore group"));
    require(ngRequireObject(param)->getParent() == group,
        RetainedString::fromStaticLiteral("undo DeleteParamGroupCommand should restore child membership"));
    incActionRedo();
    require(!hasGroup(group),
        RetainedString::fromStaticLiteral("redo DeleteParamGroupCommand should remove group again"));
    require(ngRequireObject(param)->getParent() == nullptr,
        RetainedString::fromStaticLiteral("redo DeleteParamGroupCommand should detach children again"));
}

void testParameterCopyPasteDuplicateAndLinkCommands() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::parameter::param::Add1DParameterCommand;
    using namespace nijigenerate::commands::parameter::paramedit;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::ValueParameterBinding;
    using nicxlive::core::param::ParameterParameterBinding;
    using namespace nijigenerate::core::actionstack;
    using namespace nijigenerate::project;
    resetCase();
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(incActivePuppet());
    require(ngRequireObject(constructSourceCommand<Add1DParameterCommand>(-1, 1)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("source parameter should be created"));
    require(ngRequireObject(constructSourceCommand<Add1DParameterCommand>(-1, 1)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("destination parameter should be created"));
    auto source = ngRequireObject(incActivePuppet())->parameters[0];
    auto dest = ngRequireObject(incActivePuppet())->parameters[1];
    auto node = Node::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    node->setName(NullableString::fromStaticLiteral("parameter-copy-node"));
    auto sourceBinding = newValueBinding(source, node, RetainedString::fromStaticLiteral("transform.t.x"), true);
    ngRequireObject(sourceBinding)->setValue({0, 0}, -2.0f);
    sourceBinding->setValue({2, 0}, 4.0f);
    sourceBinding->reInterpolate();
    ctx->parameters(Context::Parameters{source});
    require(ngRequireObject(constructSourceCommand<CopyParameterCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("CopyParameterCommand should succeed"));
    ctx->parameters(Context::Parameters{dest});
    require(ngRequireObject(constructSourceCommand<PasteParameterCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("PasteParameterCommand should succeed"));
    auto pasted = std::dynamic_pointer_cast<ValueParameterBinding>(
        ngRequireObject(dest)->getBinding(node, NullableString::fromStaticLiteral("transform.t.x")));
    require(pasted != nullptr,
        RetainedString::fromStaticLiteral("PasteParameterCommand should create destination binding"));
    require(ngRequireObject(pasted)->isSet({0, 0}) && near(pasted->getValue({0, 0}), -2.0f),
        RetainedString::fromStaticLiteral("paste should copy low key value"));
    require(pasted->isSet({2, 0}) && near(pasted->getValue({2, 0}), 4.0f),
        RetainedString::fromStaticLiteral("paste should copy high key value"));
    incActionUndo();
    require(dest->getBinding(node, NullableString::fromStaticLiteral("transform.t.x")) == nullptr,
        RetainedString::fromStaticLiteral("undo PasteParameterCommand should remove pasted binding"));
    incActionRedo();
    require(dest->getBinding(node, NullableString::fromStaticLiteral("transform.t.x")) != nullptr,
        RetainedString::fromStaticLiteral("redo PasteParameterCommand should restore pasted binding"));
    incActionClearHistory();
    ctx->parameters(Context::Parameters{source});
    auto duplicateResult = constructSourceCommand<DuplicateParameterCommand>()->run(ctx);
    require(ngRequireObject(duplicateResult)->succeeded,
        RetainedString::fromStaticLiteral("DuplicateParameterCommand should succeed"));
    auto duplicated = ngRequireObject(duplicateResult)->created[0];
    require(ngRequireObject(duplicated)->bindings.size() == ngRequireObject(source)->bindings.size(),
        RetainedString::fromStaticLiteral("DuplicateParameterCommand should copy bindings"));
    incActionUndo();
    require(ngRequireObject(incActivePuppet())->parameters.size() == 2,
        RetainedString::fromStaticLiteral("undo DuplicateParameterCommand should remove duplicate"));
    incActionRedo();
    require(ngRequireObject(incActivePuppet())->parameters.size() == 3,
        RetainedString::fromStaticLiteral("redo DuplicateParameterCommand should restore duplicate"));
    incActionClearHistory();
    ctx->parameters(Context::Parameters{source});
    auto duplicateFlipResult = constructSourceCommand<DuplicateParameterWithFlipCommand>()->run(ctx);
    require(ngRequireObject(duplicateFlipResult)->succeeded,
        RetainedString::fromStaticLiteral("DuplicateParameterWithFlipCommand should succeed"));
    require(ngRequireObject(ngRequireObject(duplicateFlipResult)->created[0])->bindings.size() == source->bindings.size(),
        RetainedString::fromStaticLiteral("DuplicateParameterWithFlipCommand should recreate bindings"));
    incActionUndo();
    require(ngRequireObject(incActivePuppet())->parameters.size() == 3,
        RetainedString::fromStaticLiteral("undo DuplicateParameterWithFlipCommand should remove flipped duplicate"));
    incActionRedo();
    require(ngRequireObject(incActivePuppet())->parameters.size() == 4,
        RetainedString::fromStaticLiteral("redo DuplicateParameterWithFlipCommand should restore flipped duplicate"));
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<CopyParameterCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("CopyParameterCommand should refill clipboard"));
    ctx->parameters(Context::Parameters{dest});
    require(ngRequireObject(constructSourceCommand<PasteParameterWithFlipCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("PasteParameterWithFlipCommand should succeed"));
    require(dest->getBinding(node, NullableString::fromStaticLiteral("transform.t.x")) != nullptr,
        RetainedString::fromStaticLiteral("PasteParameterWithFlipCommand should keep/create destination binding"));
    incActionClearHistory();
    ctx->parameters(Context::Parameters{dest});
    require(ngRequireObject(constructSourceCommand<LinkToCommand>(source, 0, 0)->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("LinkToCommand should succeed"));
    auto linkBinding = std::dynamic_pointer_cast<ParameterParameterBinding>(
        dest->getBinding(source, NullableString::fromStaticLiteral("X")));
    require(linkBinding != nullptr,
        RetainedString::fromStaticLiteral("LinkToCommand should create ParameterParameterBinding"));
    require(ngRequireObject(linkBinding)->isSet({0, 0}) && near(linkBinding->values[0][0], source->min.x),
        RetainedString::fromStaticLiteral("link should map destination min to source min"));
    require(linkBinding->isSet({2, 0}) && near(linkBinding->values[2][0], source->max.x),
        RetainedString::fromStaticLiteral("link should map destination max to source max"));
    incActionUndo();
    require(dest->getBinding(source, NullableString::fromStaticLiteral("X")) == nullptr,
        RetainedString::fromStaticLiteral("undo LinkToCommand should remove link binding"));
    incActionRedo();
    require(dest->getBinding(source, NullableString::fromStaticLiteral("X")) != nullptr,
        RetainedString::fromStaticLiteral("redo LinkToCommand should restore link binding"));
}

void testParameterAxesPropsCommandUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::parameter::prop::ApplyParameterPropsAxesCommand;
    using namespace nijigenerate::commands::binding;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::param::Parameter;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::core::actionstack;
    using namespace nijigenerate::project;
    resetCase();
    auto node = Node::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    auto param = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("Axes"); }, [] { return true; });
    param->min = Vec2{-1, -1};
    param->max = Vec2{1, 1};
    param->insertAxisPoint(0, 0.5f);
    param->insertAxisPoint(1, 0.5f);
    ngRequireObject(incActivePuppet())->parameters.push_back(param);
    auto oldAxisX = param->axisPoints[0].duplicate();
    auto oldAxisY = param->axisPoints[1].duplicate();
    auto binding = newValueBinding(param, node, RetainedString::fromStaticLiteral("transform.t.x"), true);
    ngRequireObject(binding)->setValue({1, 1}, 6.0f);
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->parameters(Context::Parameters{param});
    struct ExpandedAxesFrame {
        Slice<float> x;
        Slice<float> y;
        void prepare(bool includeY) {
            if (x.isNull()) x = Slice<float>{0, 0.25f, 0.5f, 0.75f, 1};
            if (includeY && y.isNull()) y = Slice<float>{0, 0.5f, 1};
        }
    } expanded;
    struct ExpandedX {
        ExpandedAxesFrame& frame;
        operator Slice<float>() && { frame.prepare(false); return frame.x; }
    };
    struct ExpandedY {
        ExpandedAxesFrame& frame;
        operator Slice<float>() && { frame.prepare(true); return frame.y; }
    };
    require(ngRequireObject(std::allocate_shared<ApplyParameterPropsAxesCommand>(
        SourceAllocator<ApplyParameterPropsAxesCommand>{}, std::array<float, 2>{-2, -3},
        std::array<float, 2>{2, 3}, ExpandedX{expanded}, ExpandedY{expanded})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("ApplyParameterPropsAxesCommand should succeed"));
    require(param->min.x == -2 && param->min.y == -3,
        RetainedString::fromStaticLiteral("ApplyParameterPropsAxesCommand should apply min"));
    require(param->max.x == 2 && param->max.y == 3,
        RetainedString::fromStaticLiteral("ApplyParameterPropsAxesCommand should apply max"));
    require(param->axisPoints[0].size() == 5 && param->axisPoints[1].size() == 3,
        RetainedString::fromStaticLiteral("ApplyParameterPropsAxesCommand should apply axis breakpoints"));
    require(binding->isSet({2, 1}) && near(binding->getValue({2, 1}), 6.0f),
        RetainedString::fromStaticLiteral("ApplyParameterPropsAxesCommand should remap bound center value"));
    incActionUndo();
    require(param->min.x == -1 && param->min.y == -1,
        RetainedString::fromStaticLiteral("undo ApplyParameterPropsAxesCommand should restore min"));
    require(param->max.x == 1 && param->max.y == 1,
        RetainedString::fromStaticLiteral("undo ApplyParameterPropsAxesCommand should restore max"));
    // Used scalar float[] equality, core.internal.array.equality.__equals/isEqual.
    const auto sameAxis = [](const auto& lhs, const auto& rhs) {
        if (lhs.size() != rhs.size()) return false;
        for (std::size_t i = 0; i < lhs.size(); ++i) if (lhs[i] != rhs[i]) return false;
        return true;
    };
    require(sameAxis(param->axisPoints[0], oldAxisX) && sameAxis(param->axisPoints[1], oldAxisY),
        RetainedString::fromStaticLiteral("undo ApplyParameterPropsAxesCommand should restore axis breakpoints"));
    require(binding->isSet({1, 1}) && near(binding->getValue({1, 1}), 6.0f),
        RetainedString::fromStaticLiteral("undo ApplyParameterPropsAxesCommand should restore original bound value"));
    incActionRedo();
    require(param->min.x == -2 && param->min.y == -3,
        RetainedString::fromStaticLiteral("redo ApplyParameterPropsAxesCommand should restore min"));
    require(param->max.x == 2 && param->max.y == 3,
        RetainedString::fromStaticLiteral("redo ApplyParameterPropsAxesCommand should restore max"));
    require(param->axisPoints[0].size() == 5 && param->axisPoints[1].size() == 3,
        RetainedString::fromStaticLiteral("redo ApplyParameterPropsAxesCommand should restore axis breakpoints"));
    require(binding->isSet({2, 1}) && near(binding->getValue({2, 1}), 6.0f),
        RetainedString::fromStaticLiteral("redo ApplyParameterPropsAxesCommand should restore remapped bound value"));
    param->value = param->max;
    paramPointChanged(param);
    std::shared_ptr<Parameter> armedParam = param;
    incArmParameter(0, armedParam);
    require(nijigenerate::panels::parameters::incParamPoint() == nijigenerate::compat::Vec2u{4, 2},
        RetainedString::fromStaticLiteral("armed keypoint should initially resolve against expanded axes"));
    struct ReducedAxesFrame {
        Slice<float> x;
        Slice<float> y;
        void prepare(bool includeY) {
            if (x.isNull()) x = Slice<float>{0, 1};
            if (includeY && y.isNull()) y = Slice<float>{0, 1};
        }
    } reduced;
    struct ReducedX {
        ReducedAxesFrame& frame;
        operator Slice<float>() && { frame.prepare(false); return frame.x; }
    };
    struct ReducedY {
        ReducedAxesFrame& frame;
        operator Slice<float>() && { frame.prepare(true); return frame.y; }
    };
    require(ngRequireObject(std::allocate_shared<ApplyParameterPropsAxesCommand>(
        SourceAllocator<ApplyParameterPropsAxesCommand>{}, std::array<float, 2>{-2, -3},
        std::array<float, 2>{2, 3}, ReducedX{reduced}, ReducedY{reduced})->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("ApplyParameterPropsAxesCommand should support reducing armed axes"));
    require(nijigenerate::panels::parameters::incParamPoint() == nijigenerate::compat::Vec2u{1, 1},
        RetainedString::fromStaticLiteral("armed keypoint should be refreshed after reducing axes"));
    require(ngBindingHasKeypoint(binding, nijigenerate::panels::parameters::incParamPoint()),
        RetainedString::fromStaticLiteral("refreshed armed keypoint should remain inside the binding"));
    cParamPoint = {9, 9};
    require(!ngBindingHasKeypoint(binding, cParamPoint),
        RetainedString::fromStaticLiteral("out-of-range keypoint should be detected"));
    require(!ngBindingIsSetAt(binding, cParamPoint),
        RetainedString::fromStaticLiteral("out-of-range binding query should not throw"));
    incActionUndo();
    require(nijigenerate::panels::parameters::incParamPoint() == nijigenerate::compat::Vec2u{4, 2},
        RetainedString::fromStaticLiteral("undo should resolve the armed keypoint against restored axes"));
    require(ngBindingHasKeypoint(binding, nijigenerate::panels::parameters::incParamPoint()),
        RetainedString::fromStaticLiteral("undo-resolved keypoint should remain inside the binding"));
    incActionRedo();
    require(nijigenerate::panels::parameters::incParamPoint() == nijigenerate::compat::Vec2u{1, 1},
        RetainedString::fromStaticLiteral("redo should resolve the armed keypoint against reduced axes"));
    require(ngBindingHasKeypoint(binding, nijigenerate::panels::parameters::incParamPoint()),
        RetainedString::fromStaticLiteral("redo-resolved keypoint should remain inside the binding"));
    binding->values.resize(1);
    require(!ngBindingHasKeypoint(binding, {1, 1}),
        RetainedString::fromStaticLiteral("mismatched binding value matrix should be detected"));
    require(!ngBindingIsSetAt(binding, {1, 1}),
        RetainedString::fromStaticLiteral("mismatched binding value query should not throw"));
}
