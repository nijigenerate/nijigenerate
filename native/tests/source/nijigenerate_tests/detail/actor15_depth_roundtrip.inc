// Complete existing regression.d17402-17533 only.
void testDepthBoneSerializationRoundTrip() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::node::node::CopyNodeCommand;
    using nijigenerate::commands::puppet::file::SaveFileCommand;
    using namespace nijigenerate::commands::node::base;
    using namespace nijigenerate::ext::nodes;
    namespace paths = nijigenerate::compat::phobos;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto equalLimits = [](nijigenerate::compat::Slice<float> a, nijigenerate::compat::Slice<float> b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (!(a[i] == b[i])) return false;
        return true;
    };
    const auto equalUuids = [](nijigenerate::compat::Slice<std::uint64_t> a,
            nijigenerate::compat::Slice<std::uint64_t> b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) return false;
        return true;
    };
    resetCase();
    ensureRegressionNodeTypesRegistered();
    auto copyContainer = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    copyContainer->setName(text("serialized-depth-container").toNullableString());
    auto root = ExDepthRigRoot::createSourceDeferred([&] { return copyContainer; });
    root->setName(text("serialized-depth-root").toNullableString());
    auto target = GridDeformer::createSourceDeferred([&] { return copyContainer; });
    target->setName(text("serialized-depth-target").toNullableString());
    auto bone = ngCreateDepthBone(root, text("SerializedBone"), Vec3{1, 2, 3}, Vec3{4, 5, 6}, 0.25f);
    bone->setName(text("serialized-depth-bone").toNullableString());
    bone->constraintType = text("hinge");
    bone->hingeAxis = {0, 1, 0};
    bone->lockRotation = true;
    bone->lockTranslation = true;
    bone->allowParentToTargets = false;
    bone->rotationLimits = {-0.75f, 0.5f};
    bone->maxStepRadians = 0.125f;
    root->addBoneSource(target, ExDepthTargetKind::Grid, bone);
    root->bindings[0].influenceRule.maxInfluences = 2;
    root->bindings[0].influenceRule.radiusScale = 1.5f;
    root->bindings[0].influenceRule.minimumRadius = 12.0f;
    root->bindings[0].influenceRule.falloff = text("linear");
    root->bindings[0].influenceRule.multipliersByBoneUuid[bone->uuid] = 0.4f;
    ExDepthBoneSourceSettings setting;
    setting.boneUuid = bone->uuid;
    setting.weight = 0.25f;
    setting.depthOffset = 1.5f;
    setting.depthScale = 2.0f;
    setting.rotation = -0.625f;
    root->bindings[0].setSourceSetting(setting);
    auto copiedRoot = std::dynamic_pointer_cast<ExDepthRigRoot>(root->dup());
    check(copiedRoot != nullptr && copiedRoot->uuid != root->uuid && copiedRoot->children.size() == 1,
        "duplicating a DepthRigRoot must create a distinct root and child hierarchy");
    auto copiedBone = std::dynamic_pointer_cast<ExDepthBone>(copiedRoot->children[0]);
    check(copiedBone != nullptr && copiedBone->uuid != bone->uuid && copiedBone->boneId == bone->boneId &&
        copiedBone->restHead == bone->restHead && copiedBone->restTail == bone->restTail && near(copiedBone->restRoll, bone->restRoll) &&
        copiedBone->constraintType == bone->constraintType && copiedBone->hingeAxis == bone->hingeAxis &&
        copiedBone->lockRotation == bone->lockRotation && copiedBone->lockTranslation == bone->lockTranslation &&
        copiedBone->allowParentToTargets == bone->allowParentToTargets && equalLimits(copiedBone->rotationLimits, bone->rotationLimits) &&
        near(copiedBone->maxStepRadians, bone->maxStepRadians), "duplicating a DepthBone must preserve all extension state");
    check(copiedRoot->bindings.size() == 1 && copiedRoot->bindings[0].targetUuid == target->uuid && [&] {
        auto actual = copiedRoot->bindings[0].sourceBoneUuids;
        nijigenerate::compat::Slice<std::uint64_t> expected{static_cast<std::uint64_t>(copiedBone->uuid)};
        return equalUuids(actual, expected);
    }() && copiedRoot->bindings[0].sourceSettings[0].boneUuid == copiedBone->uuid &&
        static_cast<bool>(copiedRoot->bindings[0].influenceRule.multipliersByBoneUuid.find(copiedBone->uuid)),
        "duplicating a DepthRigRoot must preserve external targets and remap copied bone references");
    copiedBone->rotationLimits[0] = -9.0f;
    copiedRoot->bindings[0].sourceSettings[0].weight = 0.9f;
    copiedRoot->bindings[0].influenceRule.multipliersByBoneUuid[copiedBone->uuid] = 0.9f;
    const auto originalMultiplier = [&] {
        const auto entry = root->bindings[0].influenceRule.multipliersByBoneUuid.find(bone->uuid);
        if (!entry) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            text("source/nijigenerate_tests/regression.d"), 17459);
        return *entry;
    };
    check(near(bone->rotationLimits[0], -0.75f) && near(root->bindings[0].sourceSettings[0].weight, 0.25f) &&
        near(originalMultiplier(), 0.4f), "duplicated DepthRig state must not alias nested source arrays or influence maps");
    auto copyContext = std::allocate_shared<Context>(SourceAllocator<Context>{});
    copyContext->nodes(Context::Nodes{copyContainer});
    clipboardNodes.resize(0);
    check(constructSourceCommand<CopyNodeCommand>()->run(copyContext)->succeeded && clipboardNodes.size() == 1,
        "copying a common DepthRig ancestor must create one clipboard subtree");
    auto copiedContainer = clipboardNodes[0];
    auto ancestorCopiedRoot = std::dynamic_pointer_cast<ExDepthRigRoot>(findNodeRecursive(copiedContainer, "serialized-depth-root'"));
    auto ancestorCopiedTarget = std::dynamic_pointer_cast<GridDeformer>(findNodeRecursive(copiedContainer, "serialized-depth-target'"));
    check(ancestorCopiedRoot != nullptr && ancestorCopiedTarget != nullptr && ancestorCopiedRoot->bindings.size() == 1 &&
        ancestorCopiedRoot->bindings[0].targetUuid == ancestorCopiedTarget->uuid,
        "copying an ancestor must remap a DepthRig binding to its copied sibling target");
    clipboardNodes.resize(0);
    copyToClipboard(Nodes{root, target});
    check(clipboardNodes.size() == 2, "copying separate selected DepthRig nodes must preserve both clipboard roots");
    auto separatelyCopiedRoot = std::dynamic_pointer_cast<ExDepthRigRoot>(clipboardNodes[0]);
    auto separatelyCopiedTarget = std::dynamic_pointer_cast<GridDeformer>(clipboardNodes[1]);
    check(separatelyCopiedRoot != nullptr && separatelyCopiedTarget != nullptr && separatelyCopiedRoot->bindings.size() == 1 &&
        separatelyCopiedRoot->bindings[0].targetUuid == separatelyCopiedTarget->uuid,
        "copying a DepthRigRoot and sibling target together must remap references across clipboard roots");
    clipboardNodes.resize(0);
    copyToClipboard(Nodes{copyContainer, root, target, bone});
    check(clipboardNodes.size() == 1, "copying overlapping ancestor and descendant selections must keep only the outermost root");
    auto overlapCopiedRoot = std::dynamic_pointer_cast<ExDepthRigRoot>(findNodeRecursive(clipboardNodes[0], "serialized-depth-root'"));
    auto overlapCopiedTarget = std::dynamic_pointer_cast<GridDeformer>(findNodeRecursive(clipboardNodes[0], "serialized-depth-target'"));
    check(overlapCopiedRoot != nullptr && overlapCopiedTarget != nullptr &&
        overlapCopiedRoot->bindings[0].targetUuid == overlapCopiedTarget->uuid,
        "overlapping clipboard selections must keep bindings within the copied ancestor subtree");
    clipboardNodes.resize(0);
    nijigenerate::project::incActivePuppet()->root->build();
    const std::array fixtureSegments{paths::ngTempDirectory(), text("nijigenerate-regression-depthbone")};
    auto fixtureDir = paths::ngBuildPath(fixtureSegments);
    paths::ngMkdirRecurse(fixtureDir.view());
    const std::array saveSegments{fixtureDir, text("depthbone-roundtrip")};
    auto saveBase = paths::ngBuildPath(saveSegments);
    auto savePath = concatenateSourceStrings({saveBase, text(".inx")});
    check(constructSourceCommand<SaveFileCommand>(saveBase)->run(std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
        "SaveFileCommand should save DepthBone fixture");
    check(paths::ngFileExists(savePath.view()) && paths::ngIsFile(savePath.view()), "DepthBone INX fixture should exist");
    ensureRegressionNodeTypesRegistered();
    auto loadedPuppet = nijigenerate::io::project_reader::ngLoadProjectPuppet<nijigenerate::ext::ExPuppet>(savePath);
    auto loadedRoot = std::dynamic_pointer_cast<ExDepthRigRoot>(findNodeRecursive(loadedPuppet->root, "serialized-depth-root"));
    auto loadedTarget = std::dynamic_pointer_cast<GridDeformer>(findNodeRecursive(loadedPuppet->root, "serialized-depth-target"));
    auto loadedBone = std::dynamic_pointer_cast<ExDepthBone>(findNodeRecursive(loadedPuppet->root, "serialized-depth-bone"));
    const bool rootPresent = loadedRoot != nullptr;
    auto rootMessage = concatenateSourceStrings({text("DepthRigRoot should load from INX; tree:\n"), nodeTreeSummary(loadedPuppet->root)});
    require(rootPresent, rootMessage);
    check(loadedTarget != nullptr, "DepthBone target should load from INX");
    check(loadedBone != nullptr, "DepthBone node should load from INX");
    check(loadedBone->boneId.view() == "SerializedBone", "DepthBone boneId should round-trip");
    check(loadedBone->restHead == Vec3{1, 2, 3} && loadedBone->restTail == Vec3{4, 5, 6}, "DepthBone rest segment should round-trip");
    check(near(loadedBone->restRoll, 0.25f), "DepthBone restRoll should round-trip");
    check(loadedBone->constraintType.view() == "hinge" && loadedBone->hingeAxis == Vec3{0, 1, 0}, "DepthBone constraint should round-trip");
    check(loadedBone->lockRotation && loadedBone->lockTranslation && !loadedBone->allowParentToTargets,
        "DepthBone constraint booleans should round-trip");
    check(loadedBone->rotationLimits.size() == 2 && near(loadedBone->rotationLimits[0], -0.75f) && near(loadedBone->rotationLimits[1], 0.5f),
        "DepthBone rotation limits should round-trip");
    check(near(loadedBone->maxStepRadians, 0.125f), "DepthBone maxStepRadians should round-trip");
    check(loadedRoot->bindings.size() == 1, "DepthRigRoot bindings should round-trip");
    auto loadedBinding = loadedRoot->bindings[0];
    check(loadedBinding.targetUuid == loadedTarget->uuid, "DepthRigBinding target uuid should round-trip");
    auto loadedUuids = loadedBinding.sourceBoneUuids;
    nijigenerate::compat::Slice<std::uint64_t> expectedUuids{static_cast<std::uint64_t>(loadedBone->uuid)};
    check(equalUuids(loadedUuids, expectedUuids), "DepthRigBinding source bone uuid should round-trip");
    auto loadedSetting = loadedBinding.sourceSetting(loadedBone->uuid);
    check(near(loadedSetting.weight, 0.25f) && near(loadedSetting.depthOffset, 1.5f) &&
        near(loadedSetting.depthScale, 2.0f) && near(loadedSetting.rotation, -0.625f), "DepthBone source settings should round-trip");
    check(loadedBinding.influenceRule.maxInfluences == 2, "DepthBone influence maxInfluences should round-trip");
    check(near(loadedBinding.influenceRule.radiusScale, 1.5f), "DepthBone influence radiusScale should round-trip");
    check(near(loadedBinding.influenceRule.minimumRadius, 12.0f), "DepthBone influence minimumRadius should round-trip");
    check(loadedBinding.influenceRule.falloff.view() == "linear", "DepthBone influence falloff should round-trip");
}
