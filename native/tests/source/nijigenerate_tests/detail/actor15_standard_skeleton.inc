// Existing regression.d16614-16620 and 17090-17132 only.
void testDepthBoneActionsUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::actions::depthbone;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    auto target = newPart(text("target"));
    auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
    bone->boneId = text("Bone");
    ExDepthRigBinding binding;
    binding.targetUuid = target->uuid;
    binding.targetKind = ExDepthTargetKind::Grid;
    binding.sourceBoneUuids = {static_cast<std::uint64_t>(bone->uuid)};
    ExDepthBoneSourceSettings setting;
    setting.boneUuid = bone->uuid;
    setting.weight = 0.75f;
    setting.depthOffset = 1.25f;
    setting.depthScale = 0.5f;
    setting.rotation = 0.375f;
    binding.sourceSettings = {setting};
    auto oldBindings = root->bindings.duplicate();
    root->bindings = {binding};
    actions::incActionPush(std::allocate_shared<DepthBoneSourceListChangeAction>(
        SourceAllocator<DepthBoneSourceListChangeAction>{}, text("DepthBone Source List"), root, oldBindings, root->bindings));
    check(root->bindings.size() == 1, "depth bone source list change should apply");
    check(root->bindings[0].sourceSettings[0].depthOffset == 1.25f, "depth bone source settings should apply");
    check(near(root->bindings[0].sourceSettings[0].rotation, 0.375f), "depth bone source rotation should apply");
    actions::incActionUndo();
    check(root->bindings.size() == 0, "depth bone source list undo should restore old list");
    actions::incActionRedo();
    check(root->bindings.size() == 1, "depth bone source list redo should restore new list");
    auto oldHead = bone->restHead;
    auto oldTail = bone->restTail;
    auto oldRoll = bone->restRoll;
    auto newHead = Vec3{1, 2, 3};
    auto newTail = Vec3{4, 5, 6};
    auto newRoll = 0.25f;
    bone->restHead = newHead;
    bone->restTail = newTail;
    bone->restRoll = newRoll;
    actions::incActionPush(std::allocate_shared<DepthBoneRestChangeAction>(SourceAllocator<DepthBoneRestChangeAction>{},
        bone, oldHead, oldTail, oldRoll, newHead, newTail, newRoll));
    check(bone->restHead == newHead && bone->restTail == newTail && near(bone->restRoll, newRoll),
        "depth bone rest change should apply");
    actions::incActionUndo();
    check(bone->restHead == oldHead && bone->restTail == oldTail && near(bone->restRoll, oldRoll),
        "depth bone rest undo should restore old values");
    actions::incActionRedo();
    check(bone->restHead == newHead && bone->restTail == newTail && near(bone->restRoll, newRoll),
        "depth bone rest redo should restore new values");
    auto constraintAction = ngCreateDepthBoneConstraintAction(bone);
    bone->constraintType = text("hinge");
    bone->hingeAxis = Vec3{0, 1, 0};
    bone->lockRotation = true;
    bone->lockTranslation = true;
    bone->allowParentToTargets = false;
    bone->rotationLimits = {-0.5f, 0.5f};
    bone->maxStepRadians = 0.1f;
    constraintAction->updateNewState();
    actions::incActionPush(constraintAction);
    check(bone->constraintType.view() == "hinge" && bone->lockRotation && bone->lockTranslation && !bone->allowParentToTargets,
        "depth bone constraint change should apply");
    actions::incActionUndo();
    check(bone->constraintType.view() == "" && !bone->lockRotation && !bone->lockTranslation && bone->allowParentToTargets,
        "depth bone constraint undo should restore old values");
    actions::incActionRedo();
    check(bone->constraintType.view() == "hinge" && bone->lockRotation && bone->lockTranslation && !bone->allowParentToTargets,
        "depth bone constraint redo should restore new values");
}

void testDepthBoneOverlaySelectionGeometry() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec4;
    using nicxlive::core::nodes::Node;
    using nijigenerate::ext::nodes::ExDepthRigRoot;
    using nijigenerate::ext::nodes::ngCreateDepthBone;
    using nijigenerate::ext::nodes::excamera_detail::point;
    using nijigenerate::viewport::model::depthboneoverlay::buildDepthBoneOverlayGeometry;
    using nijigenerate::viewport::common::transformhandle::ngViewportTransformHandleAdapter;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto xy = [](Vec4 value) { return Vec2{value.x, value.y}; };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    auto parent = ngCreateDepthBone(root, text("Parent"), Vec3{10, 10, 0}, Vec3{20, 20, 0});
    auto selected = ngCreateDepthBone(parent, text("Selected"), Vec3{20, 20, 0}, Vec3{30, 20, 0});
    auto childA = ngCreateDepthBone(selected, text("ChildA"), Vec3{30, 20, 0}, Vec3{40, 20, 0});
    auto childB = ngCreateDepthBone(selected, text("ChildB"), Vec3{20, 30, 0}, Vec3{20, 40, 0});
    auto sibling = ngCreateDepthBone(parent, text("Sibling"), Vec3{5, 20, 0}, Vec3{0, 30, 0});
    auto rootToLocal = nijigenerate::core::math::source_matrix::inverse(root->transform().matrix());
    const auto nodePoint = [&](std::shared_ptr<Node> node) {
        const auto value = point(rootToLocal * node->transform().matrix(), Vec4{0, 0, 0, 1});
        return Vec3{value.x, value.y, value.z};
    };
    auto geometry = buildDepthBoneOverlayGeometry(root, selected);
    check(geometry.selectedPoints.size() == 1, "DepthBone overlay must mark exactly the selected node");
    check(nearVec3(geometry.selectedPoints[0], nodePoint(selected)),
        "DepthBone overlay marker must use the selected node position, not its parent position");
    check(geometry.points.size() == 4, "DepthBone overlay must leave parent, children, and sibling as ordinary points");
    check(geometry.parentLines.size() == 2, "DepthBone overlay must classify exactly one parent-to-selected link");
    check(nearVec3(geometry.parentLines[0], nodePoint(parent)) && nearVec3(geometry.parentLines[1], nodePoint(selected)),
        "incoming DepthBone overlay link must run from parent to selected node");
    check(geometry.childLines.size() == 4, "DepthBone overlay must classify every selected-to-child link");
    check(nearVec3(geometry.childLines[0], nodePoint(selected)) && nearVec3(geometry.childLines[1], nodePoint(childA)) &&
        nearVec3(geometry.childLines[2], nodePoint(selected)) && nearVec3(geometry.childLines[3], nodePoint(childB)),
        "outgoing DepthBone overlay links must run from selected node to each child");
    check(geometry.lines.size() == 2 && nearVec3(geometry.lines[0], nodePoint(parent)) &&
        nearVec3(geometry.lines[1], nodePoint(sibling)),
        "unrelated DepthBone links must remain in the ordinary line buffer");
    auto selectedWorld = xy(point(selected->transform().matrix(), Vec4{0, 0, 0, 1}));
    auto selectedHandleBounds = ngViewportTransformHandleAdapter(selected)->bounds(selected);
    check(nearVec2(xy(selectedHandleBounds), selectedWorld) &&
        nearVec2(Vec2{selectedHandleBounds.z, selectedHandleBounds.w}, selectedWorld),
        "DepthBone transform hover controls must be anchored to the selected node origin, not descendant bounds");
    auto rootWorld = xy(point(root->transform().matrix(), Vec4{0, 0, 0, 1}));
    auto rootHandleBounds = ngViewportTransformHandleAdapter(root)->bounds(root);
    check(nearVec2(xy(rootHandleBounds), rootWorld) && nearVec2(Vec2{rootHandleBounds.z, rootHandleBounds.w}, rootWorld),
        "DepthRigRoot transform hover controls must be anchored to the root node origin, not its bone hierarchy");
}

std::shared_ptr<nijigenerate::ext::nodes::ExDepthBone> findDepthBoneById(
        std::shared_ptr<nijigenerate::ext::nodes::ExDepthRigRoot> root,
        nijigenerate::compat::RetainedString boneId) {
    for (auto bone : nijigenerate::compat::ngRequireObject(root)->depthBones()) {
        if (nijigenerate::compat::ngRequireObject(bone)->boneId == boneId) return bone;
    }
    return {};
}

void testDepthBoneStandardSkeletonTemplate() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::nodes::Node;
    using nijigenerate::ext::nodes::ExDepthRigRoot;
    using nijigenerate::ext::nodes::ngAddStandardDepthSkeleton;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto unitScaleRoot = ExDepthRigRoot::createSourceDeferred([] { return std::shared_ptr<Node>{}; });
    auto doubleScaleRoot = ExDepthRigRoot::createSourceDeferred([] { return std::shared_ptr<Node>{}; });
    ngAddStandardDepthSkeleton(unitScaleRoot, 1.0f);
    ngAddStandardDepthSkeleton(doubleScaleRoot, 2.0f);
    auto unitScaleHand = findDepthBoneById(unitScaleRoot, text("Hand.R"));
    auto doubleScaleHand = findDepthBoneById(doubleScaleRoot, text("Hand.R"));
    auto unitScaleFoot = findDepthBoneById(unitScaleRoot, text("Foot.R"));
    auto doubleScaleFoot = findDepthBoneById(doubleScaleRoot, text("Foot.R"));
    check(unitScaleHand != nullptr && doubleScaleHand != nullptr &&
        near(doubleScaleHand->restTail.x, unitScaleHand->restTail.x * 2.0f),
        "fallback standard skeleton width should apply the requested scale exactly once");
    check(unitScaleFoot != nullptr && doubleScaleFoot != nullptr &&
        near(doubleScaleFoot->restTail.y - doubleScaleFoot->restHead.y,
            (unitScaleFoot->restTail.y - unitScaleFoot->restHead.y) * 2.0f),
        "fallback standard skeleton segment heights should apply the requested scale exactly once");
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    root->setName(nicxlive::core::common::NullableString::fromStaticLiteral("template-depth-root"));
    ngAddStandardDepthSkeleton(root, 1000.0f);
    auto bones = root->depthBones();
    check(bones.size() == 19, "standard DepthBone skeleton should create the expected bone count");
    for (auto boneId : std::array{
            text("Pelvis"), text("Spine"), text("Chest"), text("Neck"), text("Head"),
            text("Clavicle.L"), text("UpperArm.L"), text("Forearm.L"), text("Hand.L"),
            text("Clavicle.R"), text("UpperArm.R"), text("Forearm.R"), text("Hand.R"),
            text("Thigh.L"), text("Shin.L"), text("Foot.L"), text("Thigh.R"), text("Shin.R"), text("Foot.R")}) {
        auto bone = findDepthBoneById(root, boneId);
        const bool present = bone != nullptr;
        auto presentMessage = concatenateSourceStrings({text("standard DepthBone skeleton should include "), boneId});
        require(present, presentMessage);
        const bool nonzero = bone->restHead != bone->restTail;
        auto nonzeroMessage = concatenateSourceStrings({text("standard DepthBone should have non-zero rest segment: "), boneId});
        require(nonzero, nonzeroMessage);
    }
    auto head = findDepthBoneById(root, text("Head"));
    check(head != nullptr && !head->allowParentToTargets,
        "Head should disable allowParentToTargets in the standard skeleton");
    auto footL = findDepthBoneById(root, text("Foot.L"));
    auto footR = findDepthBoneById(root, text("Foot.R"));
    check(footL != nullptr && footR != nullptr, "standard DepthBone skeleton should include both feet");
    check(footL->lockToRootValue() && footR->lockToRootValue(), "standard feet should be lockToRoot by default");
}
