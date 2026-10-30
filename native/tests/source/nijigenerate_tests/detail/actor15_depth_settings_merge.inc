// Complete existing regression.d17755-17814 only.
void testDepthBoneDeleteCleanupUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::node::node::DeleteNodeCommand;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::ext::nodes;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    root->setName(text("depth-root").toNullableString());
    auto targetParent = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    targetParent->setName(text("target-parent").toNullableString());
    auto target = GridDeformer::createSourceDeferred([&] { return targetParent; });
    target->setName(text("target-grid").toNullableString());
    auto boneA = ExDepthBone::createSourceDeferred([&] { return root; });
    boneA->setName(text("bone-a").toNullableString());
    boneA->boneId = text("Bone.A");
    auto boneB = ExDepthBone::createSourceDeferred([&] { return root; });
    boneB->setName(text("bone-b").toNullableString());
    boneB->boneId = text("Bone.B");
    nijigenerate::project::incActivePuppet()->rescanNodes();
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    check(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, boneA)->succeeded, "adding first source should succeed");
    check(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, boneB)->succeeded, "adding second source should succeed");
    check(root->bindings.size() == 1 && root->bindings[0].sourceBoneUuids.size() == 2,
        "delete cleanup fixture should have two sources");
    root->bindings[0].influenceRule.multipliersByBoneUuid[boneA->uuid] = 0.25f;
    root->bindings[0].influenceRule.multipliersByBoneUuid[boneB->uuid] = 0.75f;
    const auto multiplier = [&](std::uint64_t uuid, std::size_t sourceLine) {
        const auto entry = root->bindings[0].influenceRule.multipliersByBoneUuid.find(uuid);
        if (!entry) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            text("source/nijigenerate_tests/regression.d"), sourceLine);
        return *entry;
    };
    const auto remainingSourceIsB = [&] {
        const auto actual = root->bindings[0].sourceBoneUuids;
        const nijigenerate::compat::Slice<std::uint64_t> expected{static_cast<std::uint64_t>(boneB->uuid)};
        if (actual.size() != expected.size()) return false;
        for (std::size_t i = 0; i < actual.size(); ++i) if (actual[i] != expected[i]) return false;
        return true;
    };
    ctx->nodes(Context::Nodes{boneA});
    check(constructSourceCommand<DeleteNodeCommand>()->run(ctx)->succeeded, "DeleteNodeCommand should delete depth bone");
    check(!isChildOf(root, boneA), "DeleteNodeCommand should detach deleted depth bone");
    check(root->bindings.size() == 1, "DeleteNodeCommand should keep binding when another source remains");
    check(remainingSourceIsB(), "DeleteNodeCommand should remove only deleted bone source");
    check(!root->bindings[0].influenceRule.multipliersByBoneUuid.find(boneA->uuid) && near(multiplier(boneB->uuid, 17686), 0.75f),
        "DeleteNodeCommand should remove only the deleted source's influence multiplier");
    actions::incActionUndo();
    check(isChildOf(root, boneA), "undo DeleteNodeCommand should restore deleted depth bone");
    check(root->bindings.size() == 1 && root->bindings[0].sourceBoneUuids.size() == 2,
        "undo DeleteNodeCommand should restore removed depth bone source");
    check(near(multiplier(boneA->uuid, 17692), 0.25f), "undo DeleteNodeCommand should restore the deleted source's influence multiplier");
    actions::incActionRedo();
    check(!isChildOf(root, boneA), "redo DeleteNodeCommand should detach depth bone again");
    check(root->bindings.size() == 1 && remainingSourceIsB(), "redo DeleteNodeCommand should clean stale source again");
    check(!root->bindings[0].influenceRule.multipliersByBoneUuid.find(boneA->uuid),
        "redo DeleteNodeCommand should remove the deleted source's influence multiplier again");
    actions::incActionUndo();
    check(isChildOf(root, boneA) && root->bindings[0].sourceBoneUuids.size() == 2,
        "undoing source cleanup must restore the complete binding before target deletion");
    auto emptyTarget = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    ExDepthRigBinding emptyBinding;
    emptyBinding.targetUuid = emptyTarget->uuid;
    emptyBinding.influenceRule.maxInfluences = 2;
    root->bindings.append(emptyBinding);
    ctx->nodes(Context::Nodes{targetParent});
    check(constructSourceCommand<DeleteNodeCommand>()->run(ctx)->succeeded,
        "DeleteNodeCommand should delete a subtree containing a DepthRig target");
    check(!isChildOf(nijigenerate::project::incActivePuppet()->root, targetParent) && root->bindings.size() == 1 &&
        root->bindings[0].targetUuid == emptyTarget->uuid && root->bindings[0].sourceBoneUuids.empty() &&
        root->bindings[0].influenceRule.maxInfluences == 2,
        "deleting a target subtree must remove only matching DepthRig bindings and preserve unrelated empty bindings");
    actions::incActionUndo();
    check(isChildOf(nijigenerate::project::incActivePuppet()->root, targetParent) && root->bindings.size() == 2 &&
        root->findBindingIndex(target->uuid) >= 0 && root->findBindingIndex(emptyTarget->uuid) >= 0,
        "undoing target deletion must restore both the subtree and its complete DepthRig binding");
    actions::incActionRedo();
    check(!isChildOf(nijigenerate::project::incActivePuppet()->root, targetParent) && root->bindings.size() == 1 &&
        root->bindings[0].targetUuid == emptyTarget->uuid,
        "redoing target deletion must remove its binding again without dropping an unrelated empty binding");
}

void testDepthBoneInfluenceRuleCommandUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ExCommandResult;
    using nijigenerate::commands::ngCmd;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::ext::nodes;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto collect = [](auto&& expression) -> std::exception_ptr {
        try { expression(); }
        catch (const nijigenerate::compat::ThrowableException&) { return std::current_exception(); }
        return {};
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    root->setName(NullableString::fromStaticLiteral("influence-root"));
    auto target = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    target->setName(NullableString::fromStaticLiteral("influence-target"));
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto defaultResult = std::dynamic_pointer_cast<ExCommandResult<Json>>(
        ngCmd<DepthBoneCommand::GetDepthBoneInfluenceRule>(ctx, root, target));
    check(defaultResult != nullptr && defaultResult->succeeded &&
        nijigenerate::compat::ngJsonStringify(defaultResult->result.sourceIndexValue("maxInfluences")).view() == "4" &&
        root->bindings.empty(), "GetDepthBoneInfluenceRule must return defaults without creating a target binding");
    auto result = ngCmd<DepthBoneCommand::SetDepthBoneInfluenceRule>(ctx, root, target,
        text("{\"maxInfluences\":2,\"radiusScale\":1,\"minimumRadius\":12,\"falloff\":\"linear\",\"multipliersByBoneUuid\":{\"123\":1}}"));
    check(result->succeeded, "SetDepthBoneInfluenceRule command should succeed");
    check(root->bindings.size() == 1, "SetDepthBoneInfluenceRule should create binding for target");
    check(root->bindings[0].influenceRule.maxInfluences == 2, "SetDepthBoneInfluenceRule should apply maxInfluences");
    check(near(root->bindings[0].influenceRule.radiusScale, 1.0f),
        "SetDepthBoneInfluenceRule should accept an integer JSON radiusScale");
    check(near(root->bindings[0].influenceRule.minimumRadius, 12.0f),
        "SetDepthBoneInfluenceRule should accept an integer JSON minimumRadius");
    check(root->bindings[0].influenceRule.falloff.view() == "linear", "SetDepthBoneInfluenceRule should apply falloff");
    const auto multiplier = [&](std::size_t sourceLine) {
        const auto entry = root->bindings[0].influenceRule.multipliersByBoneUuid.find(123);
        if (!entry) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            text("source/nijigenerate_tests/regression.d"), sourceLine);
        return *entry;
    };
    check(near(multiplier(17346), 1.0f), "SetDepthBoneInfluenceRule should accept integer JSON per-bone multipliers");
    auto validRuleHistoryLength = actions::incActionHistory().size();
    auto invalidRuleError = collect([&] {
        ngCmd<DepthBoneCommand::SetDepthBoneInfluenceRule>(ctx, root, target, text("{\"radiusScale\":[]}"));
    });
    check(invalidRuleError != nullptr && root->bindings.size() == 1 &&
        near(root->bindings[0].influenceRule.radiusScale, 1.0f) && actions::incActionHistory().size() == validRuleHistoryLength,
        "nonnumeric influence radius must be rejected without mutation or history changes");
    invalidRuleError = collect([&] {
        ngCmd<DepthBoneCommand::SetDepthBoneInfluenceRule>(ctx, root, target, text("{\"multipliersByBoneUuid\":{\"123\":true}}"));
    });
    check(invalidRuleError != nullptr && near(multiplier(17359), 1.0f) &&
        actions::incActionHistory().size() == validRuleHistoryLength,
        "nonnumeric influence multiplier must be rejected without mutation or history changes");
    invalidRuleError = collect([&] {
        ngCmd<DepthBoneCommand::SetDepthBoneInfluenceRule>(ctx, root, target,
            text("{\"maxInfluences\":3,\"multipliersByBoneUuid\":{\"invalid\":0.5}}"));
    });
    check(invalidRuleError != nullptr && root->bindings.size() == 1 && root->bindings[0].influenceRule.maxInfluences == 2 &&
        near(multiplier(17366), 1.0f) && actions::incActionHistory().size() == validRuleHistoryLength,
        "invalid DepthBone influence JSON must not partially mutate an existing binding or action history");
    auto unboundTarget = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    unboundTarget->setName(NullableString::fromStaticLiteral("unbound-influence-target"));
    invalidRuleError = collect([&] {
        ngCmd<DepthBoneCommand::SetDepthBoneInfluenceRule>(ctx, root, unboundTarget,
            text("{\"multipliersByBoneUuid\":{\"invalid\":0.5}}"));
    });
    check(invalidRuleError != nullptr && root->bindings.size() == 1 && actions::incActionHistory().size() == validRuleHistoryLength,
        "invalid DepthBone influence JSON must not create a target binding before validation completes");
    auto getResult = std::dynamic_pointer_cast<ExCommandResult<Json>>(
        ngCmd<DepthBoneCommand::GetDepthBoneInfluenceRule>(ctx, root, target));
    check(getResult != nullptr, "GetDepthBoneInfluenceRule should return JSON payload");
    check(getResult->succeeded, "GetDepthBoneInfluenceRule command should succeed");
    check(nijigenerate::compat::ngJsonStringify(getResult->result.sourceIndexValue("maxInfluences")).view() == "2",
        "GetDepthBoneInfluenceRule should return maxInfluences");
    auto replaceResult = ngCmd<DepthBoneCommand::SetDepthBoneInfluenceRule>(ctx, root, target,
        text("{\"multipliersByBoneUuid\":{\"123\":0.75}}"));
    check(replaceResult->succeeded && near(multiplier(17385), 0.75f),
        "replacing a DepthBone influence map should update the current rule");
    actions::incActionUndo();
    check(near(multiplier(17388), 1.0f), "undo SetDepthBoneInfluenceRule should restore the independent previous influence map");
    actions::incActionRedo();
    check(root->bindings.size() == 1 && root->bindings[0].influenceRule.maxInfluences == 2 && near(multiplier(17393), 0.75f),
        "redo SetDepthBoneInfluenceRule should restore the replacement rule");
    actions::incActionUndo();
    actions::incActionUndo();
    check(root->bindings.empty(), "undoing both rule changes should restore the prior binding list");
    actions::incActionRedo();
    actions::incActionRedo();
}

void testDepthBoneSourceSettingsActionMerge() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::ext::nodes;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    root->setName(NullableString::fromStaticLiteral("merge-depth-root"));
    auto target = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    target->setName(NullableString::fromStaticLiteral("merge-target-grid"));
    auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
    bone->setName(NullableString::fromStaticLiteral("merge-depth-bone"));
    bone->boneId = text("MergeBone");
    nijigenerate::project::incActivePuppet()->rescanNodes();
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    check(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, bone)->succeeded,
        "source settings merge fixture should add a source");
    actions::incActionClearHistory();
    auto unboundTarget = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    unboundTarget->setName(NullableString::fromStaticLiteral("invalid-settings-unbound-grid"));
    // std.exception.collectException default catches Exception, retaining the
    // same thrown object; Error and unrelated native exceptions propagate.
    std::exception_ptr invalidSettings;
    try { ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, unboundTarget, bone, text("{invalid")); }
    catch (const nijigenerate::compat::ThrowableException&) { invalidSettings = std::current_exception(); }
    check(invalidSettings != nullptr && root->findBindingIndex(unboundTarget->uuid) < 0 && actions::incActionHistory().empty(),
        "invalid source settings JSON must not create a binding or action before validation completes");
    for (auto malformedRotation : nijigenerate::compat::Slice<Text>{text("{\"rotation\":true}"), text("{\"rotation\":[]}")}) {
        std::exception_ptr malformedRotationError;
        try { ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, unboundTarget, bone, malformedRotation); }
        catch (const nijigenerate::compat::ThrowableException&) { malformedRotationError = std::current_exception(); }
        check(malformedRotationError != nullptr && root->findBindingIndex(unboundTarget->uuid) < 0 &&
            actions::incActionHistory().empty(),
            "nonnumeric source rotation must be rejected before creating a binding or action");
    }
    ngBeginDepthBoneSourceSettingsMerge(101, text("weight"));
    nijigenerate::compat::ngRunWithScopeExit([&] {
        check(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, target, bone, text("{\"weight\":0.25}"))->succeeded,
            "first source settings drag update should succeed");
        check(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, target, bone, text("{\"weight\":0.75}"))->succeeded,
            "second source settings drag update should succeed");
        ngEndDepthBoneSourceSettingsMerge();
        check(actions::incActionHistory().size() == 1,
            "updates from one source settings drag session should merge into one history entry");
        actions::incActionUndo();
        check(near(root->bindings[0].sourceSetting(bone->uuid).weight, 1.0f),
            "merged source settings undo should restore the value from before the drag");
        actions::incActionRedo();
        check(near(root->bindings[0].sourceSetting(bone->uuid).weight, 0.75f),
            "merged source settings redo should restore the final drag value");
        ngBeginDepthBoneSourceSettingsMerge(102, text("weight"));
        check(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, target, bone, text("{\"weight\":0.5}"))->succeeded,
            "a later source settings drag should succeed");
        ngEndDepthBoneSourceSettingsMerge();
        check(actions::incActionHistory().size() == 2,
            "separate source settings drag sessions must remain separate history entries");
    }, [] { ngEndDepthBoneSourceSettingsMerge(); });
}
