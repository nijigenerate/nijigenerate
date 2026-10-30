// Complete existing regression.d14906-15175 only.
void testDepthBoneGpuOffsetPacketConstruction() {
    using Text = nijigenerate::compat::RetainedString;
    using Floats = nijigenerate::compat::Slice<float>;
    using nijigenerate::compat::Vec2u;
    using nicxlive::core::nodes::Deformable;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::Mat4;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::ext::ExParameter;
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::commands::depth::bone;
    using nijigenerate::viewport::depth::common::targetview::ngDepthDisplayScaleForTargetsInNodeSpace;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto sameFloats = [](Floats a, Floats b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (!(a[i] == b[i])) return false;
        return true;
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    root->setName(text("gpu-packet-depth-root").toNullableString());
    auto target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    target->setName(text("gpu-packet-grid").toNullableString());
    target->rebuffer(nicxlive::core::math::ngVecaFromVectorsSource(SharedArray<Vec2>{
        Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
    target->replaceDepths(Floats{0.5f, -0.25f, 0.0f, 1.0f});
    auto bone = ngCreateDepthBone(root, text("GpuPacketBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
    ExDepthRigBinding binding;
    binding.targetUuid = target->uuid;
    binding.targetKind = ExDepthTargetKind::Grid;
    binding.sourceBoneUuids = {static_cast<std::uint64_t>(bone->uuid)};
    binding.influenceRule.maxInfluences = 3;
    binding.influenceRule.radiusScale = 1.25f;
    binding.influenceRule.minimumRadius = 4.0f;
    binding.influenceRule.falloff = text("linear");
    binding.influenceRule.multipliersByBoneUuid[bone->uuid] = 0.5f;
    ExDepthBoneSourceSettings setting;
    setting.boneUuid = bone->uuid;
    setting.weight = 0.75f;
    setting.depthOffset = 1.25f;
    setting.depthScale = 0.5f;
    setting.rotation = 0.5235988f;
    binding.sourceSettings = {setting};
    root->bindings = {binding};
    auto param = ExParameter::createSourceDeferred(
        [] { return nicxlive::core::common::NullableString::fromStaticLiteral("DepthGpuPacketParam"); }, [] { return false; });
    param->min = {0, 0};
    param->max = {1, 0};
    param->value = {1, 0};
    nijigenerate::project::incActivePuppet()->parameters.push_back(param);
    auto tx = newValueBinding(param, bone, text("transform.t.x"));
    tx->setValue(Vec2u{1, 0}, 5.0f);
    DepthBoneGpuOffsetPacket packet;
    nijigenerate::compat::RetainedStringSlice error;
    // Retain the actual array cell for each original &root.bindings[index]
    // argument, even if a virtual getter replaces the root's descriptor.
    const auto buildPacket = [&](std::size_t index, std::shared_ptr<Deformable> targetValue,
            DepthBoneGpuOffsetPacket& output, bool preview = false, bool writeBinding = true) {
        auto rootValue = root;
        auto bindings = rootValue->bindings;
        auto* bindingCell = &bindings[index];
        return ngBuildDepthBoneGpuOffsetPacket(rootValue, bindingCell, targetValue, param,
            Vec2u{1, 0}, output, error, preview, writeBinding);
    };
    const auto checkError = [&]<std::size_t N>(bool condition, const char (&prefix)[N]) {
        auto message = concatenateSourceStrings({Text::fromStaticLiteral(prefix), error.retainedString()});
        require(condition, message);
    };
    checkError(buildPacket(0, target, packet, true, true), "GridDeformer GPU packet construction should succeed: ");
    check(packet.target == target, "GPU packet should preserve target identity");
    check(packet.parameter == param, "GPU packet should preserve parameter identity");
    check(packet.keypoint == Vec2u{1, 0}, "GPU packet should preserve keypoint");
    check(packet.writePreview && packet.writeBinding, "GPU packet should preserve writeback routing flags");
    check(packet.vertices.size() == target->vertices.size(), "GPU packet should include one vertex per target vertex");
    auto rawDepths = packet.rawDepths;
    auto targetDepths = target->copyDepths();
    check(sameFloats(rawDepths, targetDepths), "GPU packet should preserve unscaled target depths for stale checks");
    check(packet.depths.size() == target->vertices.size(), "GPU packet should include one depth per target vertex");
    check(packet.boneCount == 1, "GPU packet should include the runtime bone");
    check(packet.bones.size() == packet.boneCount * DepthBoneGpuBoneStride, "GPU packet bone stride should match");
    check(packet.sourceCount == 1, "GPU packet should include one source setting");
    check(packet.sources.size() == packet.sourceCount * DepthBoneGpuSourceStride, "GPU packet source stride should match");
    check(packet.maxInfluences == 3, "GPU packet should preserve maxInfluences");
    check(near(packet.radiusScale, 1.25f), "GPU packet should preserve radiusScale");
    auto dispatch = packet.dispatchPacket();
    check(dispatch.vertices.size() == packet.vertices.size(), "GPU dispatch packet should keep vertices");
    check(dispatch.depths.size() == packet.depths.size(), "GPU dispatch packet should keep depths");
    check(dispatch.bones.size() == packet.bones.size(), "GPU dispatch packet should keep bone data");
    check(dispatch.sources.size() == packet.sources.size(), "GPU dispatch packet should keep source data");
    check(near(packet.sources[4], 0.75f), "GPU packet should preserve source weight");
    check(near(packet.sources[5], 0.5f), "GPU packet should preserve source depthScale");
    check(near(packet.sources[7], 0.5f), "GPU packet should preserve source multiplier");
    check(near(packet.sources[DepthBoneGpuSourceSinRotationIndex], 0.5f), "GPU packet should store the source rotation sine");
    check(near(packet.sources[DepthBoneGpuSourceCosRotationIndex], 0.8660254f), "GPU packet should store the source rotation cosine");
    check(std::isfinite(packet.sources[DepthBoneGpuSourceRotationPivotXShiftIndex]) &&
        packet.sources[DepthBoneGpuSourceRotationPivotXShiftIndex] != 0.0f,
        "GPU packet should store one finite source-wide rotation pivot X shift");
    check(near(packet.sourceInputs[DepthBoneGpuSourceSinRotationIndex], 0.5f) &&
        near(packet.sourceInputs[DepthBoneGpuSourceCosRotationIndex], 0.8660254f),
        "GPU stale-check inputs should include source rotation");
    check(near(packet.sourceInputs[6], 1.25f), "GPU packet should preserve the unscaled source depthOffset for stale checks");
    root->bindings[0].influenceRule.maxInfluences = DepthBoneGpuMaxInfluences;
    DepthBoneGpuOffsetPacket maximumInfluencePacket;
    check(buildPacket(0, target, maximumInfluencePacket, true, true) && maximumInfluencePacket.maxInfluences == 16,
        "GPU packet construction must accept the inspector's maximum influence count");
    root->bindings[0].influenceRule.maxInfluences = 3;
    for (auto rotationCase : nijigenerate::compat::Slice<Floats>{Floats{0.0f, 0.0f, 1.0f},
            Floats{1.5707963f, 1.0f, 0.0f}, Floats{-1.5707963f, -1.0f, 0.0f}}) {
        auto angleSetting = root->bindings[0].sourceSetting(bone->uuid);
        angleSetting.rotation = rotationCase[0];
        root->bindings[0].setSourceSetting(angleSetting);
        DepthBoneGpuOffsetPacket anglePacket;
        checkError(buildPacket(0, target, anglePacket, true, true),
            "GPU packet construction should accept cardinal BoneSource rotation values: ");
        check(near(anglePacket.sources[DepthBoneGpuSourceSinRotationIndex], rotationCase[1]) &&
            near(anglePacket.sources[DepthBoneGpuSourceCosRotationIndex], rotationCase[2]),
            "GPU packet should encode the expected sine/cosine for cardinal BoneSource rotations");
        check(near(anglePacket.sources[6], packet.sources[6]),
            "GPU packet rotation must not change the world-scaled depthOffset component");
    }
    root->bindings[0].setSourceSetting(setting);
    auto modelDepthScale = ngDepthDisplayScaleForTargetsInNodeSpace(root,
        nijigenerate::compat::Slice<std::shared_ptr<Deformable>>{target});
    check(modelDepthScale > 1.0f, "GPU packet regression model should have a visible model-wide depth scale");
    check(near(packet.depths[0], 0.5f * modelDepthScale) && near(packet.depths[1], -0.25f * modelDepthScale) &&
        near(packet.depths[2], 0.0f) && near(packet.depths[3], 1.0f * modelDepthScale),
        "GPU packet must convert adjusted depths with one model-wide scale");
    check(near(packet.sources[6], 1.25f * modelDepthScale),
        "GPU packet must convert source depthOffset with the same model-wide scale");
    auto sourcePlaneDistance = (0.5f * setting.depthScale + setting.depthOffset) * modelDepthScale;
    auto expectedPivotXShift = sourcePlaneDistance * static_cast<float>(std::sin(setting.rotation)) /
        static_cast<float>(std::cos(setting.rotation));
    auto actualPivotXShift = packet.sources[DepthBoneGpuSourceRotationPivotXShiftIndex];
    const bool correctPivot = near(actualPivotXShift, expectedPivotXShift);
    const std::array<format::TypedFormatArgument, 2> pivotArguments{
        format::ngFormatArgument(actualPivotXShift), format::ngFormatArgument(expectedPivotXShift)};
    auto pivotRendered = format::ngFormatTyped(
        "BoneSource rotation pivot must keep X=d*tan(R); got %s expected %s", pivotArguments);
    require(correctPivot, nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(pivotRendered)); }));
    auto sourceLineLength = std::sqrt(actualPivotXShift * actualPivotXShift + sourcePlaneDistance * sourcePlaneDistance);
    auto expectedSourceLineLength = std::abs(sourcePlaneDistance / static_cast<float>(std::cos(setting.rotation)));
    const bool correctLine = near(sourceLineLength, expectedSourceLineLength);
    const std::array<format::TypedFormatArgument, 2> lineArguments{
        format::ngFormatArgument(sourceLineLength), format::ngFormatArgument(expectedSourceLineLength)};
    auto lineRendered = format::ngFormatTyped(
        "BoneSource rotation pivot must keep line length D=d/cos(R); got %s expected %s", lineArguments);
    require(correctLine, nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(lineRendered)); }));
    auto displayedPivots = ngDepthBoneSourceEffectivePivots(root, bone);
    check(displayedPivots.size() == 1, "ModelEdit should expose one effective pivot for the fixture BoneSource");
    check(displayedPivots[0].targetUuid == target->uuid && near(displayedPivots[0].rotationPivotXShift, actualPivotXShift) &&
        near(displayedPivots[0].effectivePoint.x, displayedPivots[0].bonePoint.x - actualPivotXShift) &&
        near(displayedPivots[0].effectivePoint.y, displayedPivots[0].bonePoint.y) &&
        near(displayedPivots[0].effectivePoint.z, displayedPivots[0].bonePoint.z),
        "ModelEdit effective pivot should match the GPU pivot center Ceff=C-d*tan(R)");
    ngSetDepthBoneEffectivePivotSelection(root, bone);
    check(ngHasPendingDepthBoneEffectivePivotRefresh(), "selecting a BoneSource pivot should queue one cache refresh");
    ngFlushDepthBoneEffectivePivotDirty();
    auto cachedPivots = ngCachedDepthBoneSourceEffectivePivots(root, bone);
    check(cachedPivots.size() == displayedPivots.size() &&
        near(cachedPivots[0].rotationPivotXShift, displayedPivots[0].rotationPivotXShift) &&
        nearVec3(cachedPivots[0].bonePoint, displayedPivots[0].bonePoint) &&
        nearVec3(cachedPivots[0].effectivePoint, displayedPivots[0].effectivePoint),
        "cached ModelEdit pivots must preserve the immediate calculation exactly");
    auto cachedGeneration = ngDepthBoneEffectivePivotCacheGeneration();
    for (int iteration = 0; iteration < 3; ++iteration) {
        ngSetDepthBoneEffectivePivotSelection(root, bone);
        check(ngCachedDepthBoneSourceEffectivePivots(root, bone).size() == displayedPivots.size(),
            "steady ModelEdit frames should reuse the completed pivot cache");
        ngFlushDepthBoneEffectivePivotDirty();
    }
    check(ngDepthBoneEffectivePivotCacheGeneration() == cachedGeneration,
        "steady ModelEdit frames must not rebuild BoneSource effective pivots");
    auto overlaySource = nijigenerate::compat::phobos::ngReadUtf8Text(text("source/nijigenerate/viewport/model/depthboneoverlay.d"));
    check(overlaySource.view().find("foreach (pivot; ngDepthBoneSourceEffectivePivots(root, selectedBone))") == std::string_view::npos &&
        overlaySource.view().find("foreach (pivot; ngCachedDepthBoneSourceEffectivePivots(root, selectedBone))") != std::string_view::npos,
        "ModelEdit drawing must consume the cache instead of rebuilding effective pivots");
    root->notifyChange(root, nicxlive::core::nodes::NotifyReason::AttributeChanged);
    check(ngHasPendingDepthBoneEffectivePivotRefresh(), "a selected rig dependency change should invalidate the pivot cache");
    check(ngDepthBoneEffectivePivotCacheGeneration() == cachedGeneration,
        "dependency notification should only queue work, not calculate synchronously");
    ngFlushDepthBoneEffectivePivotDirty();
    check(ngDepthBoneEffectivePivotCacheGeneration() == cachedGeneration + 1 &&
        near(ngCachedDepthBoneSourceEffectivePivots(root, bone)[0].rotationPivotXShift, displayedPivots[0].rotationPivotXShift),
        "the queued dependency update should refresh the same visible pivot once");
    ngSetDepthBoneEffectivePivotSelection({}, {});
    auto smallTarget = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    smallTarget->setName(text("gpu-packet-small-grid").toNullableString());
    smallTarget->rebuffer(nicxlive::core::math::ngVecaFromVectorsSource(SharedArray<Vec2>{
        Vec2{-1, 0}, Vec2{1, 0}, Vec2{-1, 10}, Vec2{1, 10}}));
    smallTarget->replaceDepths(Floats{0.5f, -0.25f, 0.0f, 1.0f});
    ExDepthRigBinding smallBinding = binding;
    smallBinding.targetUuid = smallTarget->uuid;
    root->bindings.append(smallBinding);
    DepthBoneGpuOffsetPacket smallPacket;
    checkError(buildPacket(1, smallTarget, smallPacket, true, true), "small GridDeformer GPU packet construction should succeed: ");
    auto sharedDepthScale = ngDepthDisplayScaleForTargetsInNodeSpace(root,
        nijigenerate::compat::Slice<std::shared_ptr<Deformable>>{target, smallTarget});
    check(near(smallPacket.depths[0], 0.5f * sharedDepthScale) && near(smallPacket.sources[6], 1.25f * sharedDepthScale),
        "different target bounds must not change the model-wide adjusted-depth scale");
    auto targetPacketBeforeOtherDepthChange = packet.depths.duplicate();
    smallTarget->replaceDepths(Floats{100.0f, 100.0f, 100.0f, 100.0f});
    DepthBoneGpuOffsetPacket targetPacketAfterOtherDepthChange;
    checkError(buildPacket(0, target, targetPacketAfterOtherDepthChange, true, true),
        "target GPU packet rebuild after another target depth change should succeed: ");
    check(sameFloats(targetPacketAfterOtherDepthChange.depths, targetPacketBeforeOtherDepthChange),
        "changing one target depth must not rescale another target");
    auto converted = [&] {
        Floats xs{1.0f, -2.0f};
        Floats ys{3.0f, 4.0f};
        return ngDepthBoneGpuReadbackToOffsets(xs, ys);
    }();
    check(converted.size() == 2 && nearVec2(converted[0], Vec2{1, 3}) && nearVec2(converted[1], Vec2{-2, 4}),
        "GPU readback conversion should preserve XY order");
    auto asyncSource = nijigenerate::compat::phobos::ngReadUtf8Text(text("source/nijigenerate/commands/depth/bone_gpu_async.d"));
    for (auto forbidden : nijigenerate::compat::Slice<Text>{text("GL_COMPUTE_SHADER"), text("GL_SHADER_STORAGE_BUFFER"),
            text("glBindImageTexture"), text("imageLoad"), text("imageStore")}) {
        const bool absent = asyncSource.view().find(forbidden.view()) == std::string_view::npos;
        auto message = concatenateSourceStrings({text("DepthBone GPU async path should not reference forbidden OpenGL feature "), forbidden});
        require(absent, message);
    }
    const float matrixValues[4][4]{{1.0f, 2.0f, 3.0f, 4.0f}, {5.0f, 6.0f, 7.0f, 8.0f},
        {9.0f, 10.0f, 11.0f, 12.0f}, {13.0f, 14.0f, 15.0f, 16.0f}};
    Mat4 nonSymmetricMatrix;
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column) nonSymmetricMatrix[row][column] = matrixValues[row][column];
    Floats rowMajorPacket;
    for (std::size_t i = 0; i < 16; ++i) rowMajorPacket.append(nonSymmetricMatrix[i / 4][i % 4]);
    check(nearMat4(emulateDepthBoneShaderMat4FromRowMajorPacket(
        std::span<const float>(rowMajorPacket.data(), rowMajorPacket.size())), nonSymmetricMatrix),
        "DepthBone shader matrix reconstruction should preserve D row-major mat4 packet layout");
    check(asyncSource.view().find("boneValue(base + 0u), boneValue(base + 4u), boneValue(base + 8u), boneValue(base + 12u)") != std::string_view::npos &&
        asyncSource.view().find("boneValue(base + 1u), boneValue(base + 5u), boneValue(base + 9u), boneValue(base + 13u)") != std::string_view::npos &&
        asyncSource.view().find("boneValue(base + 2u), boneValue(base + 6u), boneValue(base + 10u), boneValue(base + 14u)") != std::string_view::npos &&
        asyncSource.view().find("boneValue(base + 3u), boneValue(base + 7u), boneValue(base + 11u), boneValue(base + 15u)") != std::string_view::npos,
        "DepthBone shader should transpose row-major packet floats for GLSL mat4 construction");
    check(asyncSource.view().find("score *= exp(-distanceSq / radiusSq);") != std::string_view::npos,
        "DepthBone GPU shader must use the same Gaussian influence equation as depth-draw");
    check(asyncSource.view().find("#define SOURCE_STRIDE 28u") != std::string_view::npos &&
        asyncSource.view().find("vec3 sourceRestLocal = vec3(x, y, sourceDepth)") != std::string_view::npos &&
        asyncSource.view().find("vec3 rotationRest = rests[j] + vec3(rotationPivotXShift, 0.0, 0.0)") != std::string_view::npos &&
        asyncSource.view().find("sourceNoYawSkinMatrix(sourceIndex)") != std::string_view::npos &&
        asyncSource.view().find("baseDeformed + rotationWithYaw - rotationWithoutYaw") != std::string_view::npos &&
        asyncSource.view().find("rotationDepth = sourceDepth") == std::string_view::npos &&
        asyncSource.view().find("rest += rests[j] * w") != std::string_view::npos &&
        asyncSource.view().find("local.x - restLocal.x") != std::string_view::npos &&
        asyncSource.view().find("local.y - restLocal.y") != std::string_view::npos,
        "DepthBone GPU shader must apply one source-wide pivot correction so relative mesh relief stays rigid under transform.r.y yaw");
    if (!ngDepthBoneGpuSupported()) {
        check(!ngDepthBoneGpuSupportDiagnostic().empty(), "GPU support diagnostic should explain why GPU dispatch is unavailable");
        check(asyncSource.view().find("ngDispatchDepthBoneGpuOffsetPacketSync") == std::string_view::npos &&
            asyncSource.view().find("ngTryGenerateDepthBoneGpuOffsetsSync") == std::string_view::npos &&
            asyncSource.view().find("ngWaitDepthBoneGpuAsync") == std::string_view::npos,
            "DepthBone GPU runtime should not expose synchronous dispatch/wait helpers");
    }
    auto path = ExPathDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    path->setName(text("gpu-packet-path").toNullableString());
    path->rebuffer(nicxlive::core::math::ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{-2, 0}, Vec2{0, 5}, Vec2{2, 10}}));
    path->replaceDepths(Floats{0.5f, 0.5f, 0.5f});
    ExDepthRigBinding pathBinding = binding;
    pathBinding.targetUuid = path->uuid;
    pathBinding.targetKind = ExDepthTargetKind::Path;
    root->bindings.append(pathBinding);
    DepthBoneGpuOffsetPacket pathPacket;
    checkError(buildPacket(1, path, pathPacket), "PathDeformer GPU packet construction should succeed: ");
    check(pathPacket.target == path, "PathDeformer GPU packet should preserve target identity");
    check(pathPacket.vertices.size() == path->vertices.size(), "PathDeformer GPU packet should include control points");
    check(pathPacket.depths.size() == path->vertices.size(), "PathDeformer GPU packet should include control depths");
    check(pathPacket.sources.size() == DepthBoneGpuSourceStride, "PathDeformer GPU packet should include sources");
}
