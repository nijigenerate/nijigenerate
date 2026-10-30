// Existing regression.d13496-13572; include inside regression::detail after original helpers.
void testAnimationLifecycleUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::Animation;
    using nicxlive::core::AnimationLane;
    using nicxlive::core::AnimationParameterRef;
    using nicxlive::core::Keyframe;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto contains = [&](RetainedString key) {
        auto& map = ngRequireObject(incActivePuppet())->getAnimations();
        return map.find(key) != map.end();
    };
    const auto read = [&](RetainedString key, std::size_t sourceLine) {
        auto& map = ngRequireObject(incActivePuppet())->getAnimations();
        auto found = map.find(key);
        if (found == map.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), sourceLine);
        return found->second;
    };
    resetCase();
    auto param = nijigenerate::ext::ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("AnimLife"); }, [] { return false; });
    param->min = {0, 0};
    param->max = {1, 0};
    ngRequireObject(incActivePuppet())->parameters.push_back(param);
    Animation animation;
    animation.length = 24;
    animation.leadIn = 2;
    animation.leadOut = 20;
    animation.timestep = static_cast<float>(1.0 / 30.0);
    animation.animationWeight = 0.75f;
    const auto referenceUuid = param->getUUID();
    auto reference = std::allocate_shared<AnimationParameterRef>(SourceAllocator<AnimationParameterRef>{}, param, 0);
    nicxlive::core::common::SharedArray<Keyframe> frames{Keyframe{0, 0.1f, 0.5f}, Keyframe{12, 0.9f, 0.5f}};
    animation.lanes.push_back(AnimationLane(referenceUuid, reference, frames, nicxlive::core::common::InterpolateMode::Linear));
    check(ngAnimationCreateOrUpdate(RetainedStringSlice(text("walk")), animation), "animation create should succeed");
    check(contains(text("walk")), "animation create should add the animation");
    check(incAnimationGet() != nullptr && ngRequireObject(incAnimationGet())->name() == text("walk"), "animation create should select the new animation");
    check(read(text("walk"), 13520).lanes.size() == 1, "animation create should preserve lanes");
    check(near(read(text("walk"), 13521).timestep, static_cast<float>(1.0 / 30.0)), "animation create should preserve timestep");
    check(near(read(text("walk"), 13522).animationWeight, 0.75f), "animation create should preserve animation weight");
    incActionUndo();
    check(!contains(text("walk")), "undo animation create should remove the animation");
    check(incAnimationGet() == nullptr, "undo animation create should clear current animation");
    incActionRedo();
    check(contains(text("walk")), "redo animation create should restore the animation");
    check(incAnimationGet() != nullptr && ngRequireObject(incAnimationGet())->name() == text("walk"), "redo animation create should select the animation");
    Animation renamed = read(text("walk"), 13532);
    renamed.length = 48;
    renamed.leadIn = 4;
    renamed.leadOut = 44;
    renamed.timestep = static_cast<float>(1.0 / 60.0);
    renamed.animationWeight = 0.5f;
    renamed.additive = true;
    incActionClearHistory();
    check(ngAnimationCreateOrUpdate(RetainedStringSlice(text("run")), renamed, RetainedStringSlice(text("walk"))), "animation rename should succeed");
    check(!contains(text("walk")) && contains(text("run")), "animation rename should replace the key");
    check(read(text("run"), 13542).length == 48, "animation rename should apply edited data");
    check(read(text("run"), 13543).leadIn == 4 && read(text("run"), 13543).leadOut == 44, "animation rename should apply lead-in/out");
    check(near(read(text("run"), 13544).timestep, static_cast<float>(1.0 / 60.0)), "animation rename should apply timestep");
    check(near(read(text("run"), 13545).animationWeight, 0.5f), "animation rename should apply weight");
    check(read(text("run"), 13546).additive, "animation rename should apply additive flag");
    check(read(text("run"), 13547).lanes.size() == 1, "animation rename should preserve tracks");
    incActionUndo();
    check(contains(text("walk")) && !contains(text("run")), "undo animation rename should restore old key");
    check(read(text("walk"), 13551).length == 24, "undo animation rename should restore old data");
    check(read(text("walk"), 13552).leadIn == 2 && read(text("walk"), 13552).leadOut == 20, "undo animation rename should restore old lead-in/out");
    check(near(read(text("walk"), 13553).timestep, static_cast<float>(1.0 / 30.0)), "undo animation rename should restore old timestep");
    check(near(read(text("walk"), 13554).animationWeight, 0.75f), "undo animation rename should restore old weight");
    incActionRedo();
    check(contains(text("run")) && !contains(text("walk")), "redo animation rename should restore new key");
    check(read(text("run"), 13558).length == 48 && read(text("run"), 13558).leadOut == 44, "redo animation rename should restore edited properties");
    incActionClearHistory();
    check(ngAnimationDelete(RetainedStringSlice(text("run"))), "animation delete should succeed");
    check(!contains(text("run")), "animation delete should remove the animation");
    check(incAnimationGet() == nullptr, "animation delete should clear current animation when deleting the active one");
    incActionUndo();
    check(contains(text("run")), "undo animation delete should restore animation");
    check(read(text("run"), 13567).lanes.size() == 1, "undo animation delete should restore tracks");
    check(incAnimationGet() != nullptr && ngRequireObject(incAnimationGet())->name() == text("run"), "undo animation delete should restore active animation");
    incActionRedo();
    check(!contains(text("run")), "redo animation delete should remove animation again");
}

// Existing regression.d13574-13648; filesystem operations are the authored body only.
void testAnimationTrackBindingCleanup() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::Animation;
    using nicxlive::core::AnimationLane;
    using nicxlive::core::AnimationParameterRef;
    using nicxlive::core::Keyframe;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::parameter::prop::SetParameterNameCommand;
    using nijigenerate::commands::parameter::paramedit::DeleteParameterCommand;
    using namespace nijigenerate::commands::puppet::file;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    using namespace nijigenerate::compat::phobos;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    const std::array segments{ngTempDirectory(), text("nijigenerate-regression-animation-track-cleanup")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto keepParam = nijigenerate::ext::ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("AnimTrackKeep"); }, [] { return false; });
        keepParam->min = {0, 0};
        keepParam->max = {1, 0};
        auto deleteParam = nijigenerate::ext::ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("AnimTrackDelete"); }, [] { return false; });
        deleteParam->min = {0, 0};
        deleteParam->max = {1, 0};
        ngRequireObject(incActivePuppet())->parameters.push_back(keepParam);
        ngRequireObject(incActivePuppet())->parameters.push_back(deleteParam);
        Animation animation;
        animation.length = 30;
        const auto keepUuid = keepParam->getUUID();
        auto keepRef = std::allocate_shared<AnimationParameterRef>(SourceAllocator<AnimationParameterRef>{}, keepParam, 0);
        nicxlive::core::common::SharedArray<Keyframe> keepFrames{Keyframe{0, 0.0f, 0.5f}, Keyframe{10, 1.0f, 0.5f}};
        animation.lanes.push_back(AnimationLane(keepUuid, keepRef, keepFrames, nicxlive::core::common::InterpolateMode::Linear));
        const auto deleteUuid = deleteParam->getUUID();
        auto deleteRef = std::allocate_shared<AnimationParameterRef>(SourceAllocator<AnimationParameterRef>{}, deleteParam, 0);
        nicxlive::core::common::SharedArray<Keyframe> deleteFrames{Keyframe{0, 0.25f, 0.5f}, Keyframe{10, 0.75f, 0.5f}};
        animation.lanes.push_back(AnimationLane(deleteUuid, deleteRef, deleteFrames, nicxlive::core::common::InterpolateMode::Linear));
        check(ngAnimationCreateOrUpdate(RetainedStringSlice(text("track-cleanup")), animation), "animation track cleanup fixture should create animation");
        check(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes.size() == 2, "animation fixture should contain both lanes");
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->parameters(Context::Parameters{keepParam});
        check(ngRequireObject(constructSourceCommand<SetParameterNameCommand>(text("AnimTrackRenamed"))->run(ctx))->succeeded,
            "renaming animation target parameter should succeed");
        check(ngRequireObject(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].paramRef)->targetParam == keepParam,
            "animation lane should keep parameter reference across rename");
        check(ngRequireObject(ngRequireObject(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].paramRef)->targetParam)->getName().view() == "AnimTrackRenamed",
            "animation lane should expose renamed parameter");
        incActionUndo();
        check(ngRequireObject(ngRequireObject(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].paramRef)->targetParam)->getName().view() == "AnimTrackKeep",
            "undo rename should be reflected through animation lane reference");
        incActionRedo();
        check(ngRequireObject(ngRequireObject(ngRequireObject(ngRequireObject(incAnimationGet())->animation())->lanes[0].paramRef)->targetParam)->getName().view() == "AnimTrackRenamed",
            "redo rename should be reflected through animation lane reference");
        incActionClearHistory();
        ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(incActivePuppet());
        ctx->parameters(Context::Parameters{deleteParam});
        check(ngRequireObject(constructSourceCommand<DeleteParameterCommand>()->run(ctx))->succeeded,
            "deleting animation target parameter should succeed");
        check(findParameter(incActivePuppet(), "AnimTrackDelete") == nullptr,
            "deleted animation target parameter should leave the puppet parameter list");
        const std::array saveSegments{fixtureDir, text("track-cleanup")};
        auto saveBase = ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, text(".inx")});
        check(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded, "animation cleanup fixture should save");
        check(ngRequireObject(constructSourceCommand<OpenFileCommand>(savePath)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded, "animation cleanup fixture should load");
        auto& animations = ngRequireObject(incActivePuppet())->getAnimations();
        auto found = animations.find(text("track-cleanup"));
        auto loadedAnimation = found == animations.end() ? std::shared_ptr<Animation>{} : animations.retain(found);
        check(loadedAnimation != nullptr, "animation cleanup fixture should reload animation");
        bool sawRenamedLane = false;
        bool sawDeletedTarget = false;
        for (auto lane : ngRequireObject(loadedAnimation)->lanes) {
            if (lane.paramRef != nullptr && lane.paramRef->targetParam != nullptr) {
                if (lane.paramRef->targetParam->getName().view() == "AnimTrackRenamed") sawRenamedLane = true;
                if (lane.paramRef->targetParam->getName().view() == "AnimTrackDelete") sawDeletedTarget = true;
            }
        }
        check(sawRenamedLane, "native reload should reconnect lane to renamed surviving parameter");
        check(!sawDeletedTarget, "native reload should not reconnect lanes to deleted parameters");
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
