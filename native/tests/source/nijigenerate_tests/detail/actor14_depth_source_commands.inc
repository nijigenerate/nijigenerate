// Existing regression.d14248-14400; include inside regression::detail after helpers.
void testDepthBoneSourceCommandsUndoRedo() {
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::ExCommandResult;
    using nijigenerate::core::settings::SettingsJsonValue;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ThrowableException;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    namespace paths = nijigenerate::compat::phobos;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto equalUuids = [](auto left, auto right) {
        return left.size() == right.size() && (!left.size() ||
            std::memcmp(left.data(), right.data(), left.size() * sizeof(std::uint64_t)) == 0);
    };
    const auto readMultiplier = [&](const DepthUuidFloatMap& values, std::uint64_t uuid, int line) {
        auto entry = values.find(uuid);
        if (!entry) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            text("source/nijigenerate_tests/regression.d"), line);
        return *entry;
    };
    resetCase();
    const std::array nodeSegments{regressionRepoRoot(), text("source"), text("nijigenerate"), text("panels"),
        text("inspector"), text("node.d")};
    auto nodeInspectorSource = paths::ngReadUtf8Text(paths::ngBuildPath(nodeSegments));
    check(nodeInspectorSource.view().find("rotationDegrees = degrees(rotationX.value)") != std::string_view::npos &&
        nodeInspectorSource.view().find("rotationX.value = radians(rotationDegrees)") != std::string_view::npos,
        "Node rotation Inspector should use radians for data and degrees for UI");
    auto inspectorNames = Slice<RetainedString>{text("griddeform.d"), text("pathdeform.d")};
    for (auto inspectorName : inspectorNames) {
        const std::array segments{regressionRepoRoot(), text("source"), text("nijigenerate"), text("panels"),
            text("inspector"), inspectorName};
        auto inspectorPath = paths::ngBuildPath(segments);
        auto inspectorSource = paths::ngReadUtf8Text(inspectorPath);
        const bool editorsPresent =
            inspectorSource.view().find("rotationDegrees = degrees(setting.rotation)") != std::string_view::npos &&
            inspectorSource.view().find("normalizeDepthBoneSourceRotation(radians(rotationDegrees))") != std::string_view::npos &&
            inspectorSource.view().find("\"%.2f°\", ImGuiSliderFlags.NoRoundToFormat") != std::string_view::npos &&
            inspectorSource.view().find("igDragFloat(\"###rotation\"") != std::string_view::npos &&
            inspectorSource.view().find("\"r %.1f°\", ImGuiSliderFlags.NoRoundToFormat") != std::string_view::npos;
        require(editorsPresent, concatenateSourceStrings({inspectorPath, text(
            " should expose Node-compatible degree editors in both the popup and source row")}));
    }
    check(near(ngNormalizeDepthBoneSourceRotation(6.2831855f), 0.0f),
        "BoneSource rotation normalization should wrap one full turn to zero");
    check(near(ngNormalizeDepthBoneSourceRotation(std::numeric_limits<float>::quiet_NaN()), 0.0f),
        "BoneSource rotation normalization should reject non-finite values");
    auto root = ExDepthRigRoot::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    root->setName(NullableString::fromStaticLiteral("depth-root"));
    auto target = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    target->setName(NullableString::fromStaticLiteral("target-grid"));
    auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
    bone->setName(NullableString::fromStaticLiteral("depth-bone"));
    bone->boneId = text("Bone");
    ngRequireObject(nijigenerate::project::incActivePuppet())->rescanNodes();
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto foreignRoot = ExDepthRigRoot::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto foreignBone = ExDepthBone::createSourceDeferred([&] { return foreignRoot; });
    auto historyBeforeForeignSource = incActionHistory().size();
    std::exception_ptr foreignSourceError;
    try { ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, foreignBone); }
    catch (const ThrowableException&) { foreignSourceError = std::current_exception(); }
    check(foreignSourceError && root->bindings.size() == 0 && incActionHistory().size() == historyBeforeForeignSource,
        "AddDepthBoneSource must reject a source from another rig before mutation or history changes");
    auto addResult = ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, bone);
    check(ngRequireObject(addResult)->succeeded, "AddDepthBoneSource command should succeed");
    check(root->bindings.size() == 1, "AddDepthBoneSource should create one binding");
    check(root->bindings[0].targetUuid == target->getUUID(), "AddDepthBoneSource should bind target uuid");
    auto addedSources = root->bindings[0].sourceBoneUuids;
    auto expectedAddedSources = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
    check(equalUuids(addedSources, expectedAddedSources), "AddDepthBoneSource should store source bone uuid");
    incActionUndo();
    check(root->bindings.size() == 0, "undo AddDepthBoneSource should remove binding");
    incActionRedo();
    check(root->bindings.size() == 1 && [&] {
        auto actual = root->bindings[0].sourceBoneUuids;
        auto expected = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
        return equalUuids(actual, expected);
    }(), "redo AddDepthBoneSource should restore binding");
    auto settingsResult = ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, target, bone,
        text(R"({"weight":0.25,"depthOffset":1.5,"depthScale":2.0,"rotation":0.5})"));
    check(ngRequireObject(settingsResult)->succeeded, "SetDepthBoneSourceSettings command should succeed");
    auto settingsHistoryLabel = ngRequireObject(incActionTop())->describeUndo();
    check(settingsHistoryLabel.view().find(target->name.view()) != std::string_view::npos &&
        settingsHistoryLabel.view().find(bone->name.view()) != std::string_view::npos,
        "SetDepthBoneSourceSettings history label should identify its target and source bone");
    auto setting = root->bindings[0].sourceSetting(bone->getUUID());
    check(near(setting.weight, 0.25f), "SetDepthBoneSourceSettings should apply weight");
    check(near(setting.depthOffset, 1.5f), "SetDepthBoneSourceSettings should apply depth offset");
    check(near(setting.depthScale, 2.0f), "SetDepthBoneSourceSettings should apply depth scale");
    check(near(setting.rotation, 0.5f), "SetDepthBoneSourceSettings should apply rotation");
    auto listedSources = std::dynamic_pointer_cast<ExCommandResult<SettingsJsonValue>>(
        ngCmd<DepthBoneCommand::ListDepthBoneSources>(ctx, root, target));
    check(listedSources && listedSources->succeeded, "ListDepthBoneSources should return source settings JSON");
    check(near(static_cast<float>(listedSources->result.sourceIndexValue("sources").sourceArrayValue()[0]
        .sourceIndexValue("rotation").as_double()), 0.5f), "ListDepthBoneSources should expose source rotation");
    incActionUndo();
    setting = root->bindings[0].sourceSetting(bone->getUUID());
    check(near(setting.weight, 1.0f), "undo SetDepthBoneSourceSettings should restore default weight");
    check(near(setting.depthOffset, 0.0f), "undo SetDepthBoneSourceSettings should restore default depth offset");
    check(near(setting.depthScale, 1.0f), "undo SetDepthBoneSourceSettings should restore default depth scale");
    check(near(setting.rotation, 0.0f), "undo SetDepthBoneSourceSettings should restore default rotation");
    incActionRedo();
    setting = root->bindings[0].sourceSetting(bone->getUUID());
    check(near(setting.weight, 0.25f) && near(setting.depthOffset, 1.5f) && near(setting.depthScale, 2.0f) &&
        near(setting.rotation, 0.5f), "redo SetDepthBoneSourceSettings should restore edited settings");
    check(ngRequireObject(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, target, bone,
        text(R"({"weight":0.4})")))->succeeded,
        "SetDepthBoneSourceSettings should accept a legacy payload without rotation");
    setting = root->bindings[0].sourceSetting(bone->getUUID());
    check(near(setting.weight, 0.4f) && near(setting.rotation, 0.5f),
        "a settings payload without rotation should preserve the existing source rotation");
    incActionUndo();
    auto secondTarget = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    secondTarget->setName(NullableString::fromStaticLiteral("second-target-grid"));
    ngRequireObject(nijigenerate::project::incActivePuppet())->rescanNodes();
    check(ngRequireObject(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, secondTarget, bone))->succeeded,
        "the same DepthBone should be addable to a second target");
    check(ngRequireObject(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, secondTarget, bone,
        text(R"({"rotation":-0.25})")))->succeeded,
        "the second target should accept an independent source rotation");
    auto firstSetting = root->bindings[static_cast<std::size_t>(root->findBindingIndex(target->getUUID()))]
        .sourceSetting(bone->getUUID());
    auto secondSetting = root->bindings[static_cast<std::size_t>(root->findBindingIndex(secondTarget->getUUID()))]
        .sourceSetting(bone->getUUID());
    check(near(firstSetting.rotation, 0.5f) && near(secondSetting.rotation, -0.25f),
        "each target should keep an independent rotation for the same DepthBone");
    auto retainedBone = ExDepthBone::createSourceDeferred([&] { return root; });
    retainedBone->setName(NullableString::fromStaticLiteral("retained-depth-bone"));
    retainedBone->boneId = text("RetainedBone");
    check(ngRequireObject(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, retainedBone))->succeeded,
        "the removal fixture should retain another source on the target");
    auto targetBindingIndex = static_cast<std::size_t>(root->findBindingIndex(target->getUUID()));
    root->bindings[targetBindingIndex].influenceRule.multipliersByBoneUuid[bone->getUUID()] = 0.4f;
    root->bindings[targetBindingIndex].influenceRule.multipliersByBoneUuid[retainedBone->getUUID()] = 0.8f;
    auto removeResult = ngCmd<DepthBoneCommand::RemoveDepthBoneSource>(ctx, root, target, bone);
    check(ngRequireObject(removeResult)->succeeded, "RemoveDepthBoneSource command should succeed");
    targetBindingIndex = static_cast<std::size_t>(root->findBindingIndex(target->getUUID()));
    check(root->bindings.size() == 2 && [&] {
        auto actual = root->bindings[targetBindingIndex].sourceBoneUuids;
        auto expected = Slice<std::uint64_t>{static_cast<std::uint64_t>(retainedBone->getUUID())};
        return equalUuids(actual, expected);
    }() && !root->bindings[targetBindingIndex].influenceRule.multipliersByBoneUuid.find(bone->getUUID()) &&
        near(readMultiplier(root->bindings[targetBindingIndex].influenceRule.multipliersByBoneUuid,
            retainedBone->getUUID(), 14384), 0.8f),
        "RemoveDepthBoneSource should remove the source and only its influence multiplier");
    incActionUndo();
    auto restoredIndex = root->findBindingIndex(target->getUUID());
    check(root->bindings.size() == 2 && restoredIndex >= 0 && [&] {
        auto actual = root->bindings[static_cast<std::size_t>(restoredIndex)].sourceBoneUuids;
        auto expected = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID()),
            static_cast<std::uint64_t>(retainedBone->getUUID())};
        return equalUuids(actual, expected);
    }() && near(readMultiplier(root->bindings[static_cast<std::size_t>(restoredIndex)].influenceRule.multipliersByBoneUuid,
        bone->getUUID(), 14392), 0.4f), "undo RemoveDepthBoneSource should restore the source and its influence multiplier");
    incActionRedo();
    targetBindingIndex = static_cast<std::size_t>(root->findBindingIndex(target->getUUID()));
    check(root->bindings.size() == 2 && targetBindingIndex < root->bindings.size() && [&] {
        auto actual = root->bindings[targetBindingIndex].sourceBoneUuids;
        auto expected = Slice<std::uint64_t>{static_cast<std::uint64_t>(retainedBone->getUUID())};
        return equalUuids(actual, expected);
    }() && !root->bindings[targetBindingIndex].influenceRule.multipliersByBoneUuid.find(bone->getUUID()),
        "redo RemoveDepthBoneSource should remove the source multiplier again");
}
