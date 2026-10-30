// Existing regression.d5513-5549,13650-13746; include inside regression::detail after helpers.
void testAnimationKeyframesUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::param::Parameter;
    using nijigenerate::ext::ExParameter;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    nicxlive::core::Animation animation;
    animation.length = 60;
    require(ngAnimationCreateOrUpdate(RetainedString::fromStaticLiteral("keys"), animation),
        RetainedString::fromStaticLiteral("animation setup should create an animation"));
    incSetEditMode(EditMode::AnimEdit, false);
    incAnimationChange(RetainedString::fromStaticLiteral("keys"));
    auto param = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("Anim1D"); }, [] { return false; });
    param->min = Vec2{0, 0};
    param->max = Vec2{1, 0};
    param->value = Vec2{0.25f, 0};
    ngRequireObject(incActivePuppet())->parameters.push_back(param);
    auto playback = incAnimationGet();
    ngRequireObject(playback)->seek(3);
    std::shared_ptr<Parameter> addParam = param;
    incActionClearHistory();
    incAnimationKeyframeAdd(addParam, 0, 0.25f);
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 1,
        RetainedString::fromStaticLiteral("1D keyframe add should create one lane"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 1,
        RetainedString::fromStaticLiteral("1D keyframe add should create one keyframe"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames[0].frame == 3,
        RetainedString::fromStaticLiteral("1D keyframe add should use current frame"));
    incActionUndo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 0,
        RetainedString::fromStaticLiteral("undo 1D keyframe add should remove the lane"));
    incActionRedo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 1 &&
        ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 1,
        RetainedString::fromStaticLiteral("redo 1D keyframe add should restore the lane"));
    incActionClearHistory();
    incAnimationKeyframeAdd(addParam, 0, 0.75f);
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 1,
        RetainedString::fromStaticLiteral("editing same frame should not duplicate keyframes"));
    require(near(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames[0].value, 0.75f),
        RetainedString::fromStaticLiteral("editing same frame should update value"));
    incActionUndo();
    require(near(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames[0].value, 0.25f),
        RetainedString::fromStaticLiteral("undo keyframe edit should restore old value"));
    incActionRedo();
    require(near(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames[0].value, 0.75f),
        RetainedString::fromStaticLiteral("redo keyframe edit should restore new value"));
    incActionClearHistory();
    require(incAnimationKeyframeRemove(addParam, 0),
        RetainedString::fromStaticLiteral("1D keyframe remove should succeed"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 0,
        RetainedString::fromStaticLiteral("1D keyframe remove should remove current frame"));
    incActionUndo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 1,
        RetainedString::fromStaticLiteral("undo 1D keyframe remove should restore keyframe"));
    incActionRedo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 0,
        RetainedString::fromStaticLiteral("redo 1D keyframe remove should remove keyframe again"));
}

void testParameter2DAnimationKeyframeGroupUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::param::Parameter;
    using nijigenerate::ext::ExParameter;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::parameter::animedit::AddAnimationKeyFrameCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    nicxlive::core::Animation animation;
    ngRequireObject(incActivePuppet())->getAnimations()[RetainedString::fromStaticLiteral("regression")] = animation;
    incSetEditMode(EditMode::AnimEdit, false);
    incAnimationChange(RetainedString::fromStaticLiteral("regression"));
    auto param = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("Anim2D"); }, [] { return true; });
    param->min = Vec2{-1, -1};
    param->max = Vec2{1, 1};
    param->value = Vec2{0.25f, -0.5f};
    ngRequireObject(incActivePuppet())->parameters.push_back(param);
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->parameters(Context::Parameters{param});
    require(ngRequireObject(constructSourceCommand<AddAnimationKeyFrameCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("AddAnimationKeyFrameCommand should succeed for 2D parameter"));
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("2D animation keyframe add should be one grouped undo entry"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 2,
        RetainedString::fromStaticLiteral("2D animation keyframe add should create X and Y lanes"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 1,
        RetainedString::fromStaticLiteral("X lane should get one keyframe"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[1].frames.size() == 1,
        RetainedString::fromStaticLiteral("Y lane should get one keyframe"));
    incActionUndo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 0,
        RetainedString::fromStaticLiteral("undo 2D animation keyframe add should remove both lanes at once"));
    incActionRedo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 2,
        RetainedString::fromStaticLiteral("redo 2D animation keyframe add should restore both lanes at once"));
    incActionClearHistory();
    std::shared_ptr<Parameter> removeParam = param;
    incActionPushGroup();
    incAnimationKeyframeRemove(removeParam, 0);
    incAnimationKeyframeRemove(removeParam, 1);
    incActionPopGroup();
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("2D animation keyframe remove should be one grouped undo entry"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 2,
        RetainedString::fromStaticLiteral("2D animation keyframe remove should preserve lanes"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 0,
        RetainedString::fromStaticLiteral("X lane keyframe should be removed"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[1].frames.size() == 0,
        RetainedString::fromStaticLiteral("Y lane keyframe should be removed"));
    incActionUndo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 1,
        RetainedString::fromStaticLiteral("undo 2D animation keyframe remove should restore X keyframe"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[1].frames.size() == 1,
        RetainedString::fromStaticLiteral("undo 2D animation keyframe remove should restore Y keyframe"));
    incActionRedo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].frames.size() == 0,
        RetainedString::fromStaticLiteral("redo 2D animation keyframe remove should remove X keyframe again"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[1].frames.size() == 0,
        RetainedString::fromStaticLiteral("redo 2D animation keyframe remove should remove Y keyframe again"));
}

void testAnimationCompositeKeyframeHistory() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::parameter::animedit::AddAnimationKeyFrameCommand;
    using nijigenerate::commands::parameter::prop::SetParameterNameCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    nicxlive::core::Animation animation;
    animation.length = 48;
    require(ngAnimationCreateOrUpdate(RetainedString::fromStaticLiteral("composite"), animation),
        RetainedString::fromStaticLiteral("composite animation should be created"));
    incSetEditMode(EditMode::AnimEdit, false);
    incAnimationChange(RetainedString::fromStaticLiteral("composite"));
    auto param = new2DParameter(RetainedString::fromStaticLiteral("AnimCompositeParam"));
    param->value = Vec2{0.25f, -0.5f};
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(incActivePuppet());
    ctx->parameters(Context::Parameters{param});
    incActionClearHistory();
    require(ngRequireObject(constructSourceCommand<AddAnimationKeyFrameCommand>()->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("composite animation should add grouped 2D keyframes"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 2,
        RetainedString::fromStaticLiteral("composite animation should create X/Y lanes"));
    require(incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("composite 2D keyframe should be one undo entry"));
    require(ngRequireObject(constructSourceCommand<SetParameterNameCommand>(
        RetainedString::fromStaticLiteral("AnimCompositeRenamed"))->run(ctx))->succeeded,
        RetainedString::fromStaticLiteral("composite animation should rename keyframed parameter"));
    require(ngRequireObject(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].paramRef)
        ->targetParam == param,
        RetainedString::fromStaticLiteral("animation lane should keep parameter reference after rename"));
    require(ngRequireObject(ngRequireObject(ngRequireObject(ngRequireObject(incAnimationGet())->animation())
        ->lanes[0].paramRef)->targetParam)->getName() == NullableString::fromStaticLiteral("AnimCompositeRenamed"),
        RetainedString::fromStaticLiteral("animation lane should expose renamed parameter"));
    incActionUndo();
    require(param->getName() == NullableString::fromStaticLiteral("AnimCompositeParam"),
        RetainedString::fromStaticLiteral("undo rename should restore parameter name without removing keyframes"));
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 2,
        RetainedString::fromStaticLiteral("undo rename should keep grouped keyframes"));
    incActionUndo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 0,
        RetainedString::fromStaticLiteral("undo keyframe group should remove both 2D lanes"));
    incActionRedo();
    require(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 2,
        RetainedString::fromStaticLiteral("redo keyframe group should restore both 2D lanes"));
    incActionRedo();
    require(param->getName() == NullableString::fromStaticLiteral("AnimCompositeRenamed"),
        RetainedString::fromStaticLiteral("redo rename should restore renamed parameter after keyframes are redone"));
    require(ngRequireObject(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].paramRef)
        ->targetParam == param,
        RetainedString::fromStaticLiteral("redo chain should preserve lane target identity"));
}
