// Existing regression.d3824-3899; include inside regression::detail after world/binding helpers.
void testProjectINPExportPrunesDepthRigNodes() {
    using nicxlive::core::nodes::Node;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec3;
    using nijigenerate::ext::nodes::ExDepthRigRoot;
    using nijigenerate::ext::nodes::ExDepthBone;
    using nijigenerate::ext::ExParameter;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::project::incActivePuppet;
    using namespace nijigenerate::io::inpexport;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto normal = newMeshPart(RetainedString::fromStaticLiteral("normal"));
    normal->setName(NullableString::fromStaticLiteral("normal"));
    auto root = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    root->setName(NullableString::fromStaticLiteral("depth-root"));
    auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
    bone->setName(NullableString::fromStaticLiteral("depth-bone"));
    bone->boneId = RetainedString::fromStaticLiteral("Bone");
    bone->localTransform.translation = Vec3{20, -10, 0};
    bone->localTransform.update();
    bone->transformChanged();
    auto preserved = Node::createSourceDeferred([&] { return bone; });
    preserved->setName(NullableString::fromStaticLiteral("preserved-under-depth-rig"));
    preserved->localTransform.translation = Vec3{5, 3, 0};
    preserved->localTransform.update();
    preserved->transformChanged();
    auto preservedWorld = nodeRenderedWorldTranslation(preserved);
    auto secondRoot = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    secondRoot->setName(NullableString::fromStaticLiteral("second-depth-root"));
    auto secondBone = ExDepthBone::createSourceDeferred([&] { return secondRoot; });
    secondBone->setName(NullableString::fromStaticLiteral("second-depth-bone"));
    auto secondPreserved = Node::createSourceDeferred([&] { return secondBone; });
    secondPreserved->setName(NullableString::fromStaticLiteral("second-preserved-under-depth-rig"));
    secondPreserved->localTransform.translation = Vec3{-7, 4, 0};
    secondPreserved->localTransform.update();
    secondPreserved->transformChanged();
    auto secondPreservedWorld = nodeRenderedWorldTranslation(secondPreserved);
    auto trailing = Node::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    trailing->setName(NullableString::fromStaticLiteral("trailing-normal-node"));
    auto param = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("ExportParam"); }, [] { return false; });
    ngRequireObject(incActivePuppet())->parameters.push_back(param);
    auto normalBinding = newValueBinding(param, normal, RetainedString::fromStaticLiteral("transform.t.x"));
    auto depthBinding = newValueBinding(param, bone, RetainedString::fromStaticLiteral("transform.t.x"));
    check(param->bindings.size() == 2, "export fixture should contain normal and depth-bone bindings");
    IncINPExportSettings exportSettings;
    auto exported = incINPExportGenPuppet(incActivePuppet(), exportSettings);
    auto exportedPreserved = findNodeRecursive(ngRequireObject(exported)->root, preserved->name.view());
    auto exportedSecondPreserved = findNodeRecursive(ngRequireObject(exported)->root, secondPreserved->name.view());
    auto exportedTrailing = findNodeRecursive(ngRequireObject(exported)->root, trailing->name.view());
    check(!treeContainsType<ExDepthRigRoot>(ngRequireObject(exported)->root) &&
        !treeContainsType<ExDepthBone>(ngRequireObject(exported)->root) &&
        exportedPreserved && exportedPreserved->parentRef() == ngRequireObject(exported)->root &&
        exportedSecondPreserved && exportedSecondPreserved->parentRef() == ngRequireObject(exported)->root &&
        exportedTrailing && exportedTrailing->parentRef() == ngRequireObject(exported)->root &&
        nearVec3(nodeRenderedWorldTranslation(exportedPreserved), preservedWorld) &&
        nearVec3(nodeRenderedWorldTranslation(exportedSecondPreserved), secondPreservedWorld) &&
        exportedPreserved->getIndexInParent() < exportedSecondPreserved->getIndexInParent() &&
        exportedSecondPreserved->getIndexInParent() < exportedTrailing->getIndexInParent(),
        "serialized INP export should omit only rig nodes and preserve their ordinary descendants");
    check(preserved->parentRef() == bone && secondPreserved->parentRef() == secondBone &&
        root->getIndexInParent() < secondRoot->getIndexInParent() &&
        secondRoot->getIndexInParent() < trailing->getIndexInParent() &&
        nearVec3(nodeRenderedWorldTranslation(preserved), preservedWorld) &&
        nearVec3(nodeRenderedWorldTranslation(secondPreserved), secondPreservedWorld),
        "temporary INP export pruning must restore the editable source hierarchy and transforms");
    ngINPExportPruneEditorOnlyNodes(incActivePuppet());
    check(!treeContainsType<ExDepthRigRoot>(ngRequireObject(incActivePuppet())->root),
        "INP export pruning should remove DepthRigRoot");
    check(!treeContainsType<ExDepthBone>(ngRequireObject(incActivePuppet())->root),
        "INP export pruning should remove DepthBone");
    check(ngRequireObject(incActivePuppet())->find<Node>(normal->getUUID()) != nullptr,
        "INP export pruning should preserve normal nodes");
    check(ngRequireObject(incActivePuppet())->find<Node>(preserved->getUUID()) == preserved &&
        preserved->parentRef() == ngRequireObject(incActivePuppet())->root &&
        nearVec3(nodeRenderedWorldTranslation(preserved), preservedWorld),
        "INP export pruning should promote ordinary descendants of editor-only rig nodes without moving them");
    check(secondPreserved->parentRef() == ngRequireObject(incActivePuppet())->root &&
        nearVec3(nodeRenderedWorldTranslation(secondPreserved), secondPreservedWorld) &&
        preserved->getIndexInParent() < secondPreserved->getIndexInParent() &&
        secondPreserved->getIndexInParent() < trailing->getIndexInParent(),
        "INP export pruning should preserve descendant order across consecutive excluded rig roots");
    check(param->bindings.size() == 1, "INP export pruning should drop bindings targeting excluded depth nodes");
    auto exportedBindingTarget = std::dynamic_pointer_cast<Node>(
        ngRequireObject(param->bindings[0])->getTarget().target.resource());
    check(exportedBindingTarget && exportedBindingTarget->getUUID() == normal->getUUID(),
        "INP export pruning should preserve bindings targeting exported nodes");
}
