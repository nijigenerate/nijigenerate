// Existing regression.d14098-14246; include inside regression::detail after helpers.
void testDepthBoneInspectorCommandsUndoRedo() {
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::depth::bone::DepthBoneCommand;
    using nijigenerate::panels::inspector::ngDepthRigRootForTarget;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ThrowableException;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec3;
    using nijigenerate::project::incActivePuppet;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto equalVec3 = [](Vec3 left, Vec3 right) {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    root->setName(NullableString::fromStaticLiteral("depth-root"));
    auto secondRoot = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    secondRoot->setName(NullableString::fromStaticLiteral("depth-root-2"));
    auto gridTarget = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    gridTarget->setName(NullableString::fromStaticLiteral("depth-root-grid-target"));
    auto pathTarget = ExPathDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    pathTarget->setName(NullableString::fromStaticLiteral("depth-root-path-target"));
    auto unboundTarget = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    unboundTarget->setName(NullableString::fromStaticLiteral("depth-root-unbound-target"));
    ExDepthRigBinding gridBinding;
    gridBinding.targetUuid = gridTarget->getUUID();
    gridBinding.targetKind = ExDepthTargetKind::Grid;
    secondRoot->bindings = Slice<ExDepthRigBinding>{gridBinding};
    ExDepthRigBinding pathBinding;
    pathBinding.targetUuid = pathTarget->getUUID();
    pathBinding.targetKind = ExDepthTargetKind::Path;
    root->bindings = Slice<ExDepthRigBinding>{pathBinding};
    auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
    bone->setName(NullableString::fromStaticLiteral("depth-bone"));
    bone->boneId = RetainedString::fromStaticLiteral("Bone");
    ngRequireObject(incActivePuppet())->rescanNodes();
    auto firstBindingCount = root->bindings.size();
    auto secondBindingCount = secondRoot->bindings.size();
    check(ngDepthRigRootForTarget(incActivePuppet(), gridTarget) == secondRoot,
        "GridDeformer inspector must resolve the DepthRigRoot that binds its target");
    check(ngDepthRigRootForTarget(incActivePuppet(), pathTarget) == root,
        "PathDeformer inspector must resolve the DepthRigRoot that binds its target");
    check(ngDepthRigRootForTarget(incActivePuppet(), unboundTarget) == root,
        "an unbound target must retain the first-root fallback used to create its initial binding");
    check(root->bindings.size() == firstBindingCount && secondRoot->bindings.size() == secondBindingCount,
        "resolving an inspector DepthRigRoot must not mutate bindings");
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(incActivePuppet());
    ctx->nodes(Context::Nodes{bone});
    auto actionBeforeInvalidRest = incActionTop();
    bool invalidRestRejected = false;
    try {
        auto head = Slice<float>{7, 8, 9};
        auto tail = Slice<float>{1, 2};
        ngCmd<DepthBoneCommand::SetDepthBoneRest>(ctx, bone, head, tail, 0.5f);
    } catch (const ThrowableException&) { invalidRestRejected = true; }
    check(invalidRestRejected, "SetDepthBoneRest should reject an invalid late rest vector");
    check(equalVec3(bone->restHead, Vec3{0, 0, 0}) && equalVec3(bone->restTail, Vec3{0, 100, 0}) &&
        near(bone->restRoll, 0.0f), "rejected SetDepthBoneRest input must not leave partial mutations");
    check(incActionTop() == actionBeforeInvalidRest, "rejected SetDepthBoneRest input must not create an Undo entry");
    auto head = Slice<float>{1, 2, 3};
    auto tail = Slice<float>{4, 5, 6};
    auto restResult = ngCmd<DepthBoneCommand::SetDepthBoneRest>(ctx, bone, head, tail, 0.25f);
    check(ngRequireObject(restResult)->succeeded, "SetDepthBoneRest command should succeed");
    check(equalVec3(bone->restHead, Vec3{1, 2, 3}), "SetDepthBoneRest should apply rest head");
    check(equalVec3(bone->restTail, Vec3{4, 5, 6}), "SetDepthBoneRest should apply rest tail");
    check(near(bone->restRoll, 0.25f), "SetDepthBoneRest should apply rest roll");
    incActionUndo();
    check(equalVec3(bone->restHead, Vec3{0, 0, 0}), "undo SetDepthBoneRest should restore rest head");
    check(equalVec3(bone->restTail, Vec3{0, 100, 0}), "undo SetDepthBoneRest should restore rest tail");
    check(near(bone->restRoll, 0.0f), "undo SetDepthBoneRest should restore rest roll");
    incActionRedo();
    check(equalVec3(bone->restHead, Vec3{1, 2, 3}), "redo SetDepthBoneRest should restore rest head");
    check(equalVec3(bone->restTail, Vec3{4, 5, 6}), "redo SetDepthBoneRest should restore rest tail");
    check(near(bone->restRoll, 0.25f), "redo SetDepthBoneRest should restore rest roll");
    incActionClearHistory();
    auto constraintResult = ngCmd<DepthBoneCommand::SetDepthBoneConstraint>(ctx, bone,
        RetainedString::fromStaticLiteral(
            R"({"constraintType":"hinge","hingeAxis":[0,1,0],"lockRotation":true,"lockTranslation":true,"allowParentToTargets":false,"rotationLimits":[-0.5,0.5],"maxStepRadians":0.1})"));
    check(ngRequireObject(constraintResult)->succeeded, "SetDepthBoneConstraint command should succeed");
    check(bone->constraintType == "hinge", "SetDepthBoneConstraint should apply constraint type");
    check(equalVec3(bone->hingeAxis, Vec3{0, 1, 0}), "SetDepthBoneConstraint should apply hinge axis");
    check(bone->lockRotation && bone->lockTranslation && !bone->allowParentToTargets,
        "SetDepthBoneConstraint should apply boolean flags");
    check(bone->rotationLimits.size() == 2 && near(bone->rotationLimits[0], -0.5f) &&
        near(bone->rotationLimits[1], 0.5f), "SetDepthBoneConstraint should apply rotation limits");
    check(near(bone->maxStepRadians, 0.1f), "SetDepthBoneConstraint should apply max step");
    incActionUndo();
    check(bone->constraintType == "", "undo SetDepthBoneConstraint should restore constraint type");
    check(equalVec3(bone->hingeAxis, Vec3{0, 0, 1}), "undo SetDepthBoneConstraint should restore hinge axis");
    check(!bone->lockRotation && !bone->lockTranslation && bone->allowParentToTargets,
        "undo SetDepthBoneConstraint should restore boolean flags");
    check(bone->rotationLimits.size() == 0, "undo SetDepthBoneConstraint should restore rotation limits");
    check(near(bone->maxStepRadians, 0.0f), "undo SetDepthBoneConstraint should restore max step");
    incActionRedo();
    check(bone->constraintType == "hinge", "redo SetDepthBoneConstraint should restore constraint type");
    check(equalVec3(bone->hingeAxis, Vec3{0, 1, 0}), "redo SetDepthBoneConstraint should restore hinge axis");
    check(bone->lockRotation && bone->lockTranslation && !bone->allowParentToTargets,
        "redo SetDepthBoneConstraint should restore boolean flags");
    check(bone->rotationLimits.size() == 2 && near(bone->rotationLimits[0], -0.5f) &&
        near(bone->rotationLimits[1], 0.5f), "redo SetDepthBoneConstraint should restore rotation limits");
    check(near(bone->maxStepRadians, 0.1f), "redo SetDepthBoneConstraint should restore max step");
    auto actionBeforeInvalidConstraint = incActionTop();
    bool invalidConstraintRejected = false;
    try {
        ngCmd<DepthBoneCommand::SetDepthBoneConstraint>(ctx, bone,
            RetainedString::fromStaticLiteral(R"({"lockRotation":false,"hingeAxis":[1,2]})"));
    } catch (const ThrowableException&) { invalidConstraintRejected = true; }
    check(invalidConstraintRejected, "SetDepthBoneConstraint should reject an invalid late constraint field");
    check(bone->constraintType == "hinge" && equalVec3(bone->hingeAxis, Vec3{0, 1, 0}) &&
        bone->lockRotation && bone->lockTranslation && !bone->allowParentToTargets &&
        bone->rotationLimits.size() == 2 && near(bone->maxStepRadians, 0.1f),
        "rejected SetDepthBoneConstraint input must not leave partial mutations");
    check(incActionTop() == actionBeforeInvalidConstraint,
        "rejected SetDepthBoneConstraint input must not create an Undo entry");
    auto invalidNumbers = Slice<RetainedString>{RetainedString::fromStaticLiteral(R"({"hingeAxis":[0,[],1]})"),
        RetainedString::fromStaticLiteral(R"({"rotationLimits":[true]})"),
        RetainedString::fromStaticLiteral(R"({"maxStepRadians":"0.1"})")};
    for (auto invalidNumericConstraint : invalidNumbers) {
        auto actionBeforeInvalidNumber = incActionTop();
        bool invalidNumberRejected = false;
        try {
            ngCmd<DepthBoneCommand::SetDepthBoneConstraint>(ctx, bone, invalidNumericConstraint);
        } catch (const ThrowableException&) { invalidNumberRejected = true; }
        require(invalidNumberRejected, concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "SetDepthBoneConstraint should reject non-numeric constraint values: "), invalidNumericConstraint}));
        const bool unchanged = bone->constraintType == "hinge" && equalVec3(bone->hingeAxis, Vec3{0, 1, 0}) &&
            bone->lockRotation && bone->lockTranslation && !bone->allowParentToTargets &&
            bone->rotationLimits.size() == 2 && near(bone->rotationLimits[0], -0.5f) &&
            near(bone->rotationLimits[1], 0.5f) && near(bone->maxStepRadians, 0.1f);
        require(unchanged, concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "rejected numeric constraint input must not leave partial mutations: "), invalidNumericConstraint}));
        const bool noUndo = incActionTop() == actionBeforeInvalidNumber;
        require(noUndo, concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "rejected numeric constraint input must not create an Undo entry: "), invalidNumericConstraint}));
    }
}
