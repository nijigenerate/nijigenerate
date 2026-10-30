// Existing regression.d17134-17314; include inside regression::detail after helpers.
void testDepthBoneCreationNotifiesResourceViews() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::CreateResult;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::depth::bone::DepthBoneCommand;
    using nijigenerate::ext::nodes::ExDepthRigRoot;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::NodeNotifyListener;
    using nicxlive::core::nodes::NotifyReason;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto puppet = nijigenerate::project::incActivePuppet();
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(puppet);
    auto notifiedCell = std::allocate_shared<bool>(SourceAllocator<bool>{}, false);
    bool& structureNotified = *notifiedCell;
    ngRequireObject(ngRequireObject(puppet)->root)->addNotifyListener(NodeNotifyListener::fromContext(
        notifiedCell, [](void* context, const std::shared_ptr<Node>&, NotifyReason reason) {
            if (reason == NotifyReason::StructureChanged) *static_cast<bool*>(context) = true;
        }));
    auto rootResult = std::dynamic_pointer_cast<CreateResult<std::shared_ptr<Node>>>(
        ngCmd<DepthBoneCommand::CreateDepthRigRoot>(ctx, ngRequireObject(puppet)->root,
            RetainedString::fromStaticLiteral("resource-notify-depth-root")));
    require(rootResult != nullptr && rootResult->succeeded && rootResult->created.size() == 1,
        RetainedString::fromStaticLiteral("CreateDepthRigRoot should create one node for resource notification coverage"));
    auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(ngRequireObject(rootResult)->created[0]);
    require(root != nullptr && structureNotified,
        RetainedString::fromStaticLiteral("CreateDepthRigRoot should notify resource views after insertion"));
    structureNotified = false;
    const auto boneId = RetainedString::fromStaticLiteral("ResourceNotifyBone");
    auto restHead = Slice<float>{0, 0, 0};
    auto restTail = Slice<float>{0, 100, 0};
    auto boneResult = std::dynamic_pointer_cast<CreateResult<std::shared_ptr<Node>>>(
        ngCmd<DepthBoneCommand::AddDepthBone>(ctx, root, boneId, restHead, restTail, 0.0f));
    require(boneResult != nullptr && boneResult->succeeded && boneResult->created.size() == 1,
        RetainedString::fromStaticLiteral("AddDepthBone should create one node for resource notification coverage"));
    require(structureNotified,
        RetainedString::fromStaticLiteral("AddDepthBone should notify resource views after insertion"));
    structureNotified = false;
    incActionClearHistory();
    auto standard = ngCmd<DepthBoneCommand::AddStandardDepthSkeleton>(ctx, root, 1000.0f);
    require(ngRequireObject(standard)->succeeded && structureNotified,
        RetainedString::fromStaticLiteral("AddStandardDepthSkeleton should notify resource views after insertion"));
    require(ngRequireObject(root)->depthBones().size() == 20 && incActionHistory().size() == 1,
        RetainedString::fromStaticLiteral("AddStandardDepthSkeleton should create its hierarchy as one undoable action"));
    incActionUndo();
    require(root->depthBones().size() == 1,
        RetainedString::fromStaticLiteral("undo AddStandardDepthSkeleton should remove only the generated hierarchy"));
    incActionRedo();
    require(root->depthBones().size() == 20,
        RetainedString::fromStaticLiteral("redo AddStandardDepthSkeleton should restore the generated hierarchy"));
}

void testDepthBoneStandardParameterTemplate() {
    using Text = nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::depth::bone::AddStandardDepthParametersCommand;
    using nijigenerate::ext::nodes::ExDepthRigRoot;
    using nijigenerate::ext::nodes::ngAddStandardDepthSkeleton;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::param::ValueParameterBinding;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    root->setName(NullableString::fromStaticLiteral("template-depth-param-root"));
    ngAddStandardDepthSkeleton(root, 1000.0f);
    auto command = constructSourceCommand<AddStandardDepthParametersCommand>();
    command->root = root;
    check(ngRequireObject(command->run(std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded,
        "AddStandardDepthParametersCommand should create templates");
    auto faceYaw = findParameter(incActivePuppet(), "Face::Yaw-Pitch");
    auto faceRoll = findParameter(incActivePuppet(), "Face::Roll");
    auto bodyYaw = findParameter(incActivePuppet(), "Body::Yaw-Pitch");
    auto bodyRoll = findParameter(incActivePuppet(), "Body::Roll");
    check(faceYaw != nullptr && faceYaw->isVec2, "Face::Yaw-Pitch should be a 2D parameter");
    check(faceRoll != nullptr && !faceRoll->isVec2, "Face::Roll should be a 1D parameter");
    check(bodyYaw != nullptr && bodyYaw->isVec2, "Body::Yaw-Pitch should be a 2D parameter");
    check(bodyRoll != nullptr && !bodyRoll->isVec2, "Body::Roll should be a 1D parameter");
    check(ngRequireObject(faceYaw)->axisPoints[0].size() == 5 && faceYaw->axisPoints[1].size() == 5,
        "Face::Yaw-Pitch should use a 5x5 keypoint grid");
    check(ngRequireObject(bodyYaw)->axisPoints[0].size() == 5 && bodyYaw->axisPoints[1].size() == 5,
        "Body::Yaw-Pitch should use a 5x5 keypoint grid");
    check(ngRequireObject(faceRoll)->axisPoints[0].size() == 5 && faceRoll->axisPoints[1].size() == 1,
        "Face::Roll should use a 5-key 1D grid");
    check(ngRequireObject(bodyRoll)->axisPoints[0].size() == 5 && bodyRoll->axisPoints[1].size() == 1,
        "Body::Roll should use a 5-key 1D grid");
    auto head = findDepthBoneById(root, text("Head"));
    auto headYaw = std::dynamic_pointer_cast<ValueParameterBinding>(
        faceYaw->getBinding(head, NullableString::fromStaticLiteral("transform.r.y")));
    check(headYaw != nullptr, "Face::Yaw-Pitch should bind Head transform.r.y");
    check(near(ngRequireObject(headYaw)->getValue({0, 2}), -0.5235988f), "Head yaw left key should match template");
    check(near(headYaw->getValue({2, 2}), 0.0f), "Head yaw origin key should match template");
    check(near(headYaw->getValue({4, 2}), 0.5235988f), "Head yaw right key should match template");
    auto pelvis = findDepthBoneById(root, text("Pelvis"));
    auto pelvisX = std::dynamic_pointer_cast<ValueParameterBinding>(
        bodyRoll->getBinding(pelvis, NullableString::fromStaticLiteral("transform.t.x")));
    check(pelvisX != nullptr, "Body::Roll should bind Pelvis transform.t.x");
    check(near(ngRequireObject(pelvisX)->getValue({0, 0}), 30.0f), "Body::Roll left pelvis X should match template");
    check(near(pelvisX->getValue({2, 0}), 0.0f), "Body::Roll origin pelvis X should match template");
    check(near(pelvisX->getValue({4, 0}), -30.0f), "Body::Roll right pelvis X should match template");
    incActionUndo();
    check(findParameter(incActivePuppet(), "Face::Yaw-Pitch") == nullptr,
        "undo standard depth parameters should remove created parameters");
    incActionRedo();
    check(findParameter(incActivePuppet(), "Face::Yaw-Pitch") != nullptr,
        "redo standard depth parameters should restore created parameters");
    resetCase();
    auto conflictRoot = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    conflictRoot->setName(NullableString::fromStaticLiteral("template-depth-param-conflict-root"));
    ngAddStandardDepthSkeleton(conflictRoot, 1000.0f);
    auto existingFaceRoll = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("Face::Roll"); }, [] { return false; });
    existingFaceRoll->min = Vec2{-30, 0};
    existingFaceRoll->max = Vec2{30, 0};
    existingFaceRoll->insertAxisPoint(0, 0.5f);
    ngRequireObject(incActivePuppet())->parameters.push_back(existingFaceRoll);
    check(existingFaceRoll->axisPoints[0].size() == 3,
        "test fixture Face::Roll should start with a non-template but sufficient 3-key axis");
    auto conflictCommand = constructSourceCommand<AddStandardDepthParametersCommand>();
    conflictCommand->root = conflictRoot;
    auto conflictResult = conflictCommand->run(std::allocate_shared<Context>(SourceAllocator<Context>{}));
    check(ngRequireObject(conflictResult)->succeeded,
        "AddStandardDepthParametersCommand should reuse compatible existing standard parameters instead of throwing");
    check(findParameter(incActivePuppet(), "Face::Roll") == existingFaceRoll,
        "standard depth parameter setup should reuse the existing Face::Roll");
    check(existingFaceRoll->axisPoints[0].size() == 3 && existingFaceRoll->axisPoints[1].size() == 1,
        "standard depth parameter setup should not force Face::Roll to the template grid when required paramValue keys already exist");
    check(near(existingFaceRoll->min.x, -1.0f) && near(existingFaceRoll->max.x, 1.0f),
        "standard depth parameter setup should normalize Face::Roll range");
    auto neck = findDepthBoneById(conflictRoot, text("Neck"));
    auto neckRoll = std::dynamic_pointer_cast<ValueParameterBinding>(
        existingFaceRoll->getBinding(neck, NullableString::fromStaticLiteral("transform.r.z")));
    check(neckRoll != nullptr, "standard depth parameter setup should bind existing Face::Roll to Neck transform.r.z");
    incActionUndo();
    check(findParameter(incActivePuppet(), "Face::Roll") == existingFaceRoll,
        "undo standard depth parameter setup should keep pre-existing Face::Roll");
    check(existingFaceRoll->axisPoints[0].size() == 3 && near(existingFaceRoll->min.x, -30.0f) &&
        near(existingFaceRoll->max.x, 30.0f),
        "undo standard depth parameter setup should restore Face::Roll range without requiring template axis count");
    incActionRedo();
    check(existingFaceRoll->axisPoints[0].size() == 3 && near(existingFaceRoll->min.x, -1.0f) &&
        near(existingFaceRoll->max.x, 1.0f),
        "redo standard depth parameter setup should preserve sufficient Face::Roll keypoints");
    resetCase();
    auto missingKeyRoot = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    missingKeyRoot->setName(NullableString::fromStaticLiteral("template-depth-param-missing-key-root"));
    ngAddStandardDepthSkeleton(missingKeyRoot, 1000.0f);
    auto missingKeyFaceRoll = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("Face::Roll"); }, [] { return false; });
    missingKeyFaceRoll->min = Vec2{-1, 0};
    missingKeyFaceRoll->max = Vec2{1, 0};
    missingKeyFaceRoll->insertAxisPoint(0, 0.25f);
    ngRequireObject(incActivePuppet())->parameters.push_back(missingKeyFaceRoll);
    check(missingKeyFaceRoll->axisPoints[0].size() == 3,
        "test fixture Face::Roll should miss the 0 standard paramValue key");
    auto missingKeyCommand = constructSourceCommand<AddStandardDepthParametersCommand>();
    missingKeyCommand->root = missingKeyRoot;
    check(ngRequireObject(missingKeyCommand->run(std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded,
        "AddStandardDepthParametersCommand should add only missing paramValue keypoints for existing parameters");
    check(missingKeyFaceRoll->axisPoints[0].size() == 4,
        "standard depth parameter setup should add only the missing interior paramValue key");
    auto missingKeyNeck = findDepthBoneById(missingKeyRoot, text("Neck"));
    auto missingKeyNeckRoll = std::dynamic_pointer_cast<ValueParameterBinding>(
        missingKeyFaceRoll->getBinding(missingKeyNeck, NullableString::fromStaticLiteral("transform.r.z")));
    check(missingKeyNeckRoll != nullptr && near(missingKeyNeckRoll->getValue({3, 0}), 0.41887903f),
        "standard depth parameter setup should set binding values at required endpoint paramValue keys");
    incActionUndo();
    check(missingKeyFaceRoll->getBinding(missingKeyNeck, NullableString::fromStaticLiteral("transform.r.z")) == nullptr,
        "undo missing-key standard depth parameter setup should remove created binding");
    incActionRedo();
    check(missingKeyFaceRoll->getBinding(missingKeyNeck, NullableString::fromStaticLiteral("transform.r.z")) != nullptr,
        "redo missing-key standard depth parameter setup should restore created binding");
    resetCase();
    auto typeConflictRoot = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    typeConflictRoot->setName(NullableString::fromStaticLiteral("template-depth-param-type-conflict-root"));
    ngAddStandardDepthSkeleton(typeConflictRoot, 1000.0f);
    auto wrongTypeFaceRoll = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("Face::Roll"); }, [] { return true; });
    ngRequireObject(incActivePuppet())->parameters.push_back(wrongTypeFaceRoll);
    auto actionBeforeTypeConflict = incActionTop();
    auto typeConflictCommand = constructSourceCommand<AddStandardDepthParametersCommand>();
    typeConflictCommand->root = typeConflictRoot;
    auto typeConflictResult = typeConflictCommand->run(std::allocate_shared<Context>(SourceAllocator<Context>{}));
    check(!ngRequireObject(typeConflictResult)->succeeded &&
        typeConflictResult->message.view().find("Face::Roll") != std::string_view::npos &&
        typeConflictResult->message.view().find("2D") != std::string_view::npos &&
        typeConflictResult->message.view().find("1D") != std::string_view::npos,
        "AddStandardDepthParametersCommand should report a same-name dimensionality conflict");
    auto sameNameFaceRollCount = 0;
    for (auto param : ngRequireObject(incActivePuppet())->parameters) {
        if (ngRequireObject(param)->name == NullableString::fromStaticLiteral("Face::Roll")) ++sameNameFaceRollCount;
    }
    check(sameNameFaceRollCount == 1 && wrongTypeFaceRoll->isVec2,
        "same-name incompatible parameters must not be duplicated or mutated");
    check(findParameter(incActivePuppet(), "Face::Yaw-Pitch") == nullptr && incActionTop() == actionBeforeTypeConflict,
        "standard depth parameter type preflight must reject the command before any mutation or Undo entry");
}
