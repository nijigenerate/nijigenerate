// Existing regression.d11638-12000; include once outside all namespaces.
#include "root_depthbone_composite.hpp"
#include "root_existing_depth_gpu.hpp"
#include "regression_internal.hpp"
#include "uint_associative_array.hpp"
#include "source_strings.hpp"
#include <nijigenerate/commands/cmd.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/depth/bone.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <nijigenerate/ext/nodes/expathdeformer_node.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace nijigenerate_tests::regression::detail {
extern thread_local std::uint32_t fakeDepthBoneGpuNextJobId;
extern thread_local UIntAssociativeArray<std::size_t> fakeDepthBoneGpuJobVertexCounts;
extern thread_local std::uint32_t fakeDepthBoneGpuSubmitCount;
extern thread_local std::uint32_t fakeDepthBoneGpuPollCount;
extern thread_local std::uint32_t fakeDepthBoneGpuSubmitFailAfter;
extern thread_local std::uint32_t fakeDepthBoneGpuNotReadyPolls;
extern thread_local float fakeDepthBoneGpuOutputX;
extern thread_local float fakeDepthBoneGpuOutputY;
extern thread_local bool fakeDepthBoneGpuVaryOutputByVertex;
extern thread_local std::uint32_t fakeDepthBoneTransformNextJobId;
extern thread_local UIntAssociativeArray<
    nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult> fakeDepthBoneTransformResults;
bool fakeDepthBoneGpuSupported();
std::shared_ptr<nicxlive::core::param::ValueParameterBinding> newValueBinding(
    std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::shared_ptr<nicxlive::core::nodes::Node> node,
    nijigenerate::compat::RetainedString name, bool setZero);

void testDepthBoneCompositeSourcePreviewWorkflow() {
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::compat::ngRunWithScopeExit;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::ext::ExParameter;
    using namespace nijigenerate::ext::nodes;
    using nijigenerate::project::incActivePuppet;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::param::Vec2u;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    resetCase();
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    fakeDepthBoneGpuOutputX = 6.0f;
    fakeDepthBoneGpuOutputY = 0.0f;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    ngRunWithScopeExit([&] {
        auto root = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        root->setName(NullableString::fromStaticLiteral("depthbone-composite-root"));
        auto target = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        target->setName(NullableString::fromStaticLiteral("depthbone-composite-target"));
        auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
        bone->setName(NullableString::fromStaticLiteral("depthbone-composite-bone"));
        bone->boneId = text("CompositeBone");
        ngRequireObject(incActivePuppet())->rescanNodes();

        auto param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthBoneCompositeParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        ngRequireObject(incActivePuppet())->parameters.push_back(param);
        auto tx = newValueBinding(param, bone, text("transform.t.x"), true);
        ngRequireObject(tx)->setValue(Vec2u{1, 0}, 6.0f);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(incActivePuppet());
        ctx->armedParameters(Context::Parameters{param});
        require(ngRequireObject(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, bone))->succeeded,
            text("depthbone composite should add source"));
        require(ngRequireObject(ngCmd<DepthBoneCommand::SetDepthBoneInfluenceRule>(ctx, root, target,
            text(R"({"maxInfluences":1,"radiusScale":1.0,"minimumRadius":1.0,"falloff":"constant"})")))->succeeded,
            text("depthbone composite should set influence rule"));
        require(ngRequireObject(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, target, bone,
            text(R"({"weight":1.0,"depthOffset":0.5,"depthScale":1.25})")))->succeeded,
            text("depthbone composite should set source settings"));
        require(ngRequireObject(ngCmd<DepthBoneCommand::PreviewDepthBoneDeform>(
            ctx, root, Slice<std::shared_ptr<Node>>{target}))->succeeded,
            text("depthbone composite should preview posed deform"));
        ngFlushDepthBoneDirtyImmediate();
        for (const auto offset : target->getDeformation())
            require(near(offset.x, 6.0f), text("depthbone preview should apply keyed bone translation"));

        incActionClearHistory();
        require(ngRequireObject(ngCmd<DepthBoneCommand::ApplyDepthBoneDeform>(
            ctx, root, Slice<std::shared_ptr<Node>>{target}))->succeeded,
            text("depthbone composite should apply preview to binding"));
        ngFlushDepthBoneDirtyImmediate();
        auto deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        require(deformBinding != nullptr, text("depthbone composite apply should create deform binding"));
        require(ngRequireObject(deformBinding)->getValue(Vec2u{1, 0}).vertexOffsets.size() == target->getVertices().size(),
            text("depthbone composite binding should match target topology"));
        incActionUndo();
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        require(deformBinding && deformBinding->getValue(Vec2u{1, 0}).vertexOffsets.size() == target->getVertices().size(),
            text("undo depthbone composite apply should preserve an undoable deform binding shape after async writeback"));
        incActionRedo();
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        require(deformBinding && deformBinding->getValue(Vec2u{1, 0}).vertexOffsets.size() == target->getVertices().size(),
            text("redo depthbone composite apply should restore coherent binding"));

        auto parentGrid = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        parentGrid->setName(NullableString::fromStaticLiteral("depthbone-nested-parent"));
        auto childGrid = ExGridDeformer::createSourceDeferred([&] { return parentGrid; });
        childGrid->setName(NullableString::fromStaticLiteral("depthbone-nested-child"));
        ngRequireObject(incActivePuppet())->rescanNodes();
        ExDepthRigBinding parentBinding;
        parentBinding.targetUuid = parentGrid->getUUID();
        parentBinding.targetKind = ExDepthTargetKind::Grid;
        parentBinding.sourceBoneUuids = Slice<std::uint64_t>{bone->getUUID()};
        ExDepthRigBinding childBinding;
        childBinding.targetUuid = childGrid->getUUID();
        childBinding.targetKind = ExDepthTargetKind::Grid;
        childBinding.sourceBoneUuids = Slice<std::uint64_t>{bone->getUUID()};
        root->bindings.append(parentBinding);
        root->bindings.append(childBinding);
        require(ngRequireObject(ngCmd<DepthBoneCommand::ApplyDepthBoneDeform>(
            ctx, root, Slice<std::shared_ptr<Node>>{childGrid, parentGrid}))->succeeded,
            text("nested depthbone apply should accept child-before-parent target order"));
        ngFlushDepthBoneDirtyImmediate();
        auto parentDeformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(parentGrid, NullableString::fromStaticLiteral("deform")));
        auto childDeformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(childGrid, NullableString::fromStaticLiteral("deform")));
        require(parentDeformBinding != nullptr && childDeformBinding != nullptr,
            text("nested depthbone apply should create parent and child bindings"));
        for (const auto offset : ngRequireObject(parentDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets)
            require(near(offset.x, 6.0f), text("nested depthbone parent should retain the generated deformation"));
        for (const auto offset : ngRequireObject(childDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets)
            require(near(offset.x, 0.0f),
                text("nested depthbone child should remove the inherited parent GridDeformer deformation"));

        Slice<float> parentDepths;
        parentDepths.resize(parentGrid->getVertices().size(), std::numeric_limits<float>::quiet_NaN());
        std::fill(parentDepths.begin(), parentDepths.end(), 0.25f);
        parentGrid->replaceDepths(parentDepths);
        Slice<float> childDepths;
        childDepths.resize(childGrid->getVertices().size(), std::numeric_limits<float>::quiet_NaN());
        std::fill(childDepths.begin(), childDepths.end(), -0.1f);
        childGrid->replaceDepths(childDepths);
        fakeDepthBoneTransformNextJobId = 1;
        fakeDepthBoneTransformResults = nullptr;
        ngSetDepthBoneGpuAsyncTestHooks(
            &fakeDepthBoneGpuSupported, &fakeDepthBoneSourceDepthSubmit, &fakeDepthBoneTransformPoll);
        require(ngRequireObject(ngCmd<DepthBoneCommand::ApplyDepthBoneDeform>(
            ctx, root, Slice<std::shared_ptr<Node>>{childGrid, parentGrid}))->succeeded,
            text("nested depthbone source/depth fixture should apply"));
        ngFlushDepthBoneDirtyImmediate();
        float initialParentOffset = ngRequireObject(parentDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets[0].x;
        float initialChildOffset = ngRequireObject(childDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets[0].x;
        require(ngRequireObject(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(ctx, root, parentGrid, bone,
            text(R"({"weight":1.0,"depthOffset":0.25,"depthScale":2.0})")))->succeeded,
            text("nested depthbone parent source settings should update"));
        ngFlushDepthBoneDirtyImmediate();
        float sourceChangedParentOffset = ngRequireObject(parentDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets[0].x;
        float sourceChangedChildOffset = ngRequireObject(childDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets[0].x;
        require(!near(sourceChangedParentOffset, initialParentOffset) &&
            !near(sourceChangedChildOffset, initialChildOffset) &&
            near(sourceChangedParentOffset + sourceChangedChildOffset, initialParentOffset + initialChildOffset),
            text("changing parent BoneSource offset/scale must refresh the child parent-influence compensation"));
        std::fill(parentDepths.begin(), parentDepths.end(), 0.75f);
        parentGrid->replaceDepths(parentDepths);
        ngMarkDepthBoneDirtyForTarget(parentGrid, text("Nested Parent Depth"));
        ngFlushDepthBoneDirtyImmediate();
        float depthChangedParentOffset = ngRequireObject(parentDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets[0].x;
        float depthChangedChildOffset = ngRequireObject(childDeformBinding)->getValue(Vec2u{1, 0}).vertexOffsets[0].x;
        require(!near(depthChangedParentOffset, sourceChangedParentOffset) &&
            !near(depthChangedChildOffset, sourceChangedChildOffset) &&
            near(depthChangedParentOffset + depthChangedChildOffset, sourceChangedParentOffset + sourceChangedChildOffset),
            text("changing parent GridDeformer depth must refresh descendant compensation in the same DepthRig"));
    }, [&] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        fakeDepthBoneGpuOutputX = 2.0f;
        fakeDepthBoneGpuOutputY = -1.0f;
    });
}

// Existing regression.d11818-12000, including its authored requireOffsets.
void testDepthBoneAncestorPropagationGates() {
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::compat::ngRunWithScopeExit;
    using nijigenerate::ext::ExParameter;
    using namespace nijigenerate::ext::nodes;
    using nijigenerate::project::incActivePuppet;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::param::Vec2u;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    resetCase();
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    fakeDepthBoneGpuOutputX = 6.0f;
    fakeDepthBoneGpuOutputY = 0.0f;
    fakeDepthBoneGpuVaryOutputByVertex = true;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    ngRunWithScopeExit([&] {
        auto root = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        root->setName(NullableString::fromStaticLiteral("depthbone-propagation-root"));
        auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
        bone->setName(NullableString::fromStaticLiteral("depthbone-propagation-bone"));
        bone->boneId = text("PropagationBone");
        auto inheritingBone = ExDepthBone::createSourceDeferred([&] { return bone; });
        inheritingBone->setName(NullableString::fromStaticLiteral("depthbone-inheriting-bone"));
        inheritingBone->boneId = text("InheritingBone");
        auto isolatedBone = ExDepthBone::createSourceDeferred([&] { return bone; });
        isolatedBone->setName(NullableString::fromStaticLiteral("depthbone-isolated-bone"));
        isolatedBone->boneId = text("IsolatedBone");
        isolatedBone->allowParentToTargets = false;
        auto unrelatedBone = ExDepthBone::createSourceDeferred([&] { return root; });
        unrelatedBone->setName(NullableString::fromStaticLiteral("depthbone-unrelated-bone"));
        unrelatedBone->boneId = text("UnrelatedBone");

        auto directParent = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        directParent->setName(NullableString::fromStaticLiteral("depthbone-direct-parent"));
        auto directChild = ExGridDeformer::createSourceDeferred([&] { return directParent; });
        directChild->setName(NullableString::fromStaticLiteral("depthbone-direct-child"));
        auto propagatedParent = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        propagatedParent->setName(NullableString::fromStaticLiteral("depthbone-propagated-parent"));
        auto propagatedBridgeA = ExPathDeformer::createSourceDeferred([&] { return propagatedParent; });
        propagatedBridgeA->setName(NullableString::fromStaticLiteral("depthbone-propagated-bridge-a"));
        auto propagatedBridgeB = ExPathDeformer::createSourceDeferred([&] { return propagatedBridgeA; });
        propagatedBridgeB->setName(NullableString::fromStaticLiteral("depthbone-propagated-bridge-b"));
        auto propagatedChild = ExGridDeformer::createSourceDeferred([&] { return propagatedBridgeB; });
        propagatedChild->setName(NullableString::fromStaticLiteral("depthbone-propagated-child"));

        auto blockedParent = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        blockedParent->setName(NullableString::fromStaticLiteral("depthbone-blocked-parent"));
        auto blocker = Node::createSourceDeferred([&] { return blockedParent; });
        blocker->setName(NullableString::fromStaticLiteral("depthbone-arm-root"));
        blocker->localTransform.translation = Vec3{0.25f, -0.25f, 0.0f};
        blocker->localTransform.update();
        auto blockedChild = ExGridDeformer::createSourceDeferred([&] { return blocker; });
        blockedChild->setName(NullableString::fromStaticLiteral("depthbone-blocked-child"));
        auto stationaryParent = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        stationaryParent->setName(NullableString::fromStaticLiteral("depthbone-stationary-parent"));
        stationaryParent->translateChildren = false;
        auto stationaryBlocker = Node::createSourceDeferred([&] { return stationaryParent; });
        stationaryBlocker->setName(NullableString::fromStaticLiteral("depthbone-stationary-blocker"));
        auto stationaryChild = ExGridDeformer::createSourceDeferred([&] { return stationaryBlocker; });
        stationaryChild->setName(NullableString::fromStaticLiteral("depthbone-stationary-child"));

        auto gatedParent = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        gatedParent->setName(NullableString::fromStaticLiteral("depthbone-parent-influence-gate-parent"));
        auto inheritingRoot = Node::createSourceDeferred([&] { return gatedParent; });
        inheritingRoot->setName(NullableString::fromStaticLiteral("depthbone-inheriting-arm-root"));
        inheritingRoot->localTransform.translation = Vec3{0.25f, -0.25f, 0.0f};
        inheritingRoot->localTransform.update();
        auto inheritingChild = ExGridDeformer::createSourceDeferred([&] { return inheritingRoot; });
        inheritingChild->setName(NullableString::fromStaticLiteral("depthbone-inheriting-child"));
        auto isolatedRoot = Node::createSourceDeferred([&] { return gatedParent; });
        isolatedRoot->setName(NullableString::fromStaticLiteral("depthbone-isolated-arm-root"));
        isolatedRoot->localTransform.translation = Vec3{0.25f, -0.25f, 0.0f};
        isolatedRoot->localTransform.update();
        auto isolatedChild = ExGridDeformer::createSourceDeferred([&] { return isolatedRoot; });
        isolatedChild->setName(NullableString::fromStaticLiteral("depthbone-isolated-child"));
        auto outerGrid = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
        outerGrid->setName(NullableString::fromStaticLiteral("depthbone-outer-grid"));
        auto innerGrid = ExGridDeformer::createSourceDeferred([&] { return outerGrid; });
        innerGrid->setName(NullableString::fromStaticLiteral("depthbone-inner-grid"));
        auto gridBlockedChild = ExGridDeformer::createSourceDeferred([&] { return innerGrid; });
        gridBlockedChild->setName(NullableString::fromStaticLiteral("depthbone-grid-blocked-child"));

        Slice<std::shared_ptr<ExGridDeformer>> targets{
            directParent, directChild, propagatedParent, propagatedChild, blockedParent, blockedChild,
            stationaryParent, stationaryChild, gatedParent, inheritingChild, isolatedChild,
            outerGrid, innerGrid, gridBlockedChild};
        for (const auto target : targets) {
            ExDepthRigBinding binding;
            binding.targetUuid = target->getUUID();
            binding.targetKind = ExDepthTargetKind::Grid;
            binding.sourceBoneUuids = Slice<std::uint64_t>{bone->getUUID()};
            root->bindings.append(binding);
        }
        auto inheritingIndex = root->findBindingIndex(inheritingChild->getUUID());
        auto isolatedIndex = root->findBindingIndex(isolatedChild->getUUID());
        auto gatedParentIndex = root->findBindingIndex(gatedParent->getUUID());
        require(inheritingIndex >= 0 && isolatedIndex >= 0 && gatedParentIndex >= 0,
            text("parent-influence gate fixture bindings should exist"));
        root->bindings[static_cast<std::size_t>(gatedParentIndex)].sourceBoneUuids =
            Slice<std::uint64_t>{bone->getUUID(), unrelatedBone->getUUID()};
        root->bindings[static_cast<std::size_t>(inheritingIndex)].sourceBoneUuids =
            Slice<std::uint64_t>{inheritingBone->getUUID()};
        root->bindings[static_cast<std::size_t>(isolatedIndex)].sourceBoneUuids =
            Slice<std::uint64_t>{isolatedBone->getUUID()};
        ngRequireObject(incActivePuppet())->rescanNodes();
        auto param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthBonePropagationParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        ngRequireObject(incActivePuppet())->parameters.push_back(param);
        auto tx = newValueBinding(param, bone, text("transform.t.x"), true);
        ngRequireObject(tx)->setValue(Vec2u{1, 0}, 6.0f);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(incActivePuppet());
        ctx->armedParameters(Context::Parameters{param});
        Slice<std::shared_ptr<Node>> reversedTargets;
        for (std::size_t index = targets.size(); index != 0;)
            reversedTargets.append(targets[--index]);
        require(ngRequireObject(ngCmd<DepthBoneCommand::ApplyDepthBoneDeform>(ctx, root, reversedTargets))->succeeded,
            text("DepthBone propagation-gate fixture should accept reverse hierarchy order"));
        ngFlushDepthBoneDirtyImmediate();

        const auto requireOffsets = [&](std::shared_ptr<ExGridDeformer> target,
            std::span<const float> expected, RetainedString message) {
            auto binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
                param->getBinding(target, NullableString::fromStaticLiteral("deform")));
            require(binding != nullptr, concatenateSourceStrings({message, text(": missing deformation binding")}));
            auto offsets = ngRequireObject(binding)->getValue(Vec2u{1, 0}).vertexOffsets;
            const bool sameLength = offsets.size() == expected.size();
            const std::array<format::TypedFormatArgument, 3> lengthArguments{
                format::ngFormatArgument(message), format::ngFormatArgument(expected.size()),
                format::ngFormatArgument(offsets.size())};
            auto lengthRendered = format::ngFormatTyped("%s: expected %s offsets, got %s", lengthArguments);
            auto lengthMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
                return RetainedString(std::move(lengthRendered));
            });
            require(sameLength, lengthMessage);
            const auto count = offsets.size();
            for (std::size_t i = 0; i < count; ++i) {
                const auto offset = offsets[i];
                const bool sameOffset = near(offset.x, expected[i]);
                const std::array<format::TypedFormatArgument, 4> valueArguments{
                    format::ngFormatArgument(message), format::ngFormatArgument(i),
                    format::ngFormatArgument(expected[i]), format::ngFormatArgument(static_cast<float>(offset.x))};
                auto valueRendered = format::ngFormatTyped("%s[%s]: expected %.4f, got %.4f", valueArguments);
                auto valueMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
                    return RetainedString(std::move(valueRendered));
                });
                require(sameOffset, valueMessage);
            }
        };
        static constexpr std::array rawOffsets{6.0f, 7.0f, 8.0f, 9.0f};
        static constexpr std::array zeroOffsets{0.0f, 0.0f, 0.0f, 0.0f};
        requireOffsets(directParent, rawOffsets, text("direct parent should retain generated deformation"));
        requireOffsets(directChild, zeroOffsets,
            text("direct child should remove deformation inherited from its parent GridDeformer"));
        requireOffsets(propagatedParent, rawOffsets, text("propagated parent should retain generated deformation"));
        requireOffsets(propagatedChild, zeroOffsets,
            text("child should remove parent deformation when every intermediate node must propagate"));
        requireOffsets(blockedParent, rawOffsets, text("blocked parent should retain generated deformation"));
        auto blockedExpected = Slice<float>{-1.25f, -0.25f, 0.75f, 1.75f};
        requireOffsets(blockedChild, std::span<const float>(blockedExpected.data(), blockedExpected.size()),
            text("child should remove the rigid origin movement inherited from an intermediate Node"));
        requireOffsets(stationaryParent, rawOffsets, text("stationary parent should retain generated deformation"));
        requireOffsets(stationaryChild, rawOffsets,
            text("child must retain its deformation when translateChildren leaves the intermediate Node origin stationary"));
        requireOffsets(gatedParent, rawOffsets, text("parent-influence gate parent should retain generated deformation"));
        auto inheritingExpected = Slice<float>{-1.25f, -0.25f, 0.75f, 1.75f};
        requireOffsets(inheritingChild, std::span<const float>(inheritingExpected.data(), inheritingExpected.size()),
            text("child should remove Node-origin motion inherited through an allowed parent DepthBone"));
        requireOffsets(isolatedChild, rawOffsets,
            text("child must retain its deformation when its DepthBone rejects parent influence"));
        requireOffsets(outerGrid, rawOffsets, text("outer GridDeformer should retain generated deformation"));
        requireOffsets(innerGrid, zeroOffsets,
            text("direct inner GridDeformer should remove deformation inherited from the outer GridDeformer"));
        requireOffsets(gridBlockedChild, rawOffsets,
            text("outer GridDeformer compensation must not cross an intermediate GridDeformer"));
    }, [&] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        fakeDepthBoneGpuOutputX = 2.0f;
        fakeDepthBoneGpuOutputY = -1.0f;
        fakeDepthBoneGpuVaryOutputByVertex = false;
    });
}
}
