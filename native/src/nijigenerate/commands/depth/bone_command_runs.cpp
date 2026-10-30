#include "bone_commands.hpp"
#include "detail/bone_input.hpp"
#include "detail/bone_fit.hpp"
#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_deform_enqueue.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/nodes/exdepthrig_create.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::commands::depth::bone {
namespace {
// Keep the exact derived handle by reference until the actual Result has been
// allocated. Its constructor then materializes the Source Node array argument.
template<class T> struct ResultNodeSliceArgument {
    const std::shared_ptr<T>& value;
    operator compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>() const { return {value}; }
};

template<class Array> auto firstCommandObject(const Array& values, std::size_t sourceLine) {
    const auto length = values.size();
    if (length == 0) throw compat::ngStaticError<compat::ArrayIndexError>(std::size_t{0}, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
    return values[0];
}

std::shared_ptr<nicxlive::core::param::Parameter> commandArmedParameter(std::shared_ptr<Context> context,
        std::size_t sourceLine) {
    compat::ngRequireObject(context);
    // Preserve the two actual virtual property reads, not a cached array or a
    // fallback when the selected first entry itself is null.
    return context->hasArmedParameters() && !context->armedParameters().empty()
        ? firstCommandObject(context->armedParameters(), sourceLine) : project::incArmedParameter();
}

compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> commandDeformTargets(
        std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> targets, std::size_t sourceLine) {
    auto actualTargets = targets;
    if (actualTargets.isNull() || actualTargets.empty()) {
        auto bindings = root->bindings;
        for (auto& binding : bindings) {
            auto puppet = project::incActivePuppet();
            if (auto node = compat::ngRequireObject(puppet)->findNodeById(static_cast<std::uint32_t>(binding.targetUuid)))
                actualTargets.append(node);
        }
    }
    if (actualTargets.empty())
        detail::failInput("No targets", sourceLine);
    return actualTargets;
}
} // namespace

std::shared_ptr<CommandResult> FitDepthRigRootZToDepthCommand::run(std::shared_ptr<Context>) {
    auto actualRoot = detail::requireRoot(root);
    if (!ngFitDepthRigNodeTranslationZToCurrentDepth(actualRoot))
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral(
            "No usable mapped depth samples were found for the DepthRigRoot"));
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> FitDepthBoneZToDepthCommand::run(std::shared_ptr<Context>) {
    auto actualBone = detail::requireBone(bone);
    if (!ngFitDepthRigNodeTranslationZToCurrentDepth(actualBone))
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral(
            "No usable mapped depth sample was found for the DepthBone"));
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> PreviewDepthBoneDeformCommand::run(std::shared_ptr<Context> context) {
    auto actualRoot = detail::requireRoot(root);
    auto parameter = commandArmedParameter(context, 4746);
    if (parameter && !detail::depthBoneParameterDrivesRig(actualRoot, parameter))
        detail::failInput("Armed parameter does not drive this DepthRigRoot", 4747);
    compat::Vec2u keypoint;
    if (parameter) {
        const auto closest = parameter->findClosestKeypoint();
        keypoint = {closest.x, closest.y};
    }
    auto actualTargets = commandDeformTargets(actualRoot, targets, 4758);
    return detail::enqueueDepthBoneGpuDeformCommand(actualRoot, parameter, keypoint, actualTargets,
        compat::RetainedString::fromStaticLiteral("Preview Depth Bone Deform"), true, false);
}

std::shared_ptr<CommandResult> ApplyDepthBoneDeformCommand::run(std::shared_ptr<Context> context) {
    auto actualRoot = detail::requireRoot(root);
    auto parameter = commandArmedParameter(context, 4773);
    if (!parameter) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No armed parameter"));
    if (!detail::depthBoneParameterDrivesRig(actualRoot, parameter))
        detail::failInput("Armed parameter does not drive this DepthRigRoot", 4775);
    const auto closest = parameter->findClosestKeypoint();
    const compat::Vec2u keypoint{closest.x, closest.y};
    auto actualTargets = commandDeformTargets(actualRoot, targets, 4787);
    return detail::enqueueDepthBoneGpuDeformCommand(actualRoot, parameter, keypoint, actualTargets,
        compat::RetainedString::fromStaticLiteral("Apply Depth Bone Deform"), false, true);
}

std::shared_ptr<CommandResult> CreateDepthRigRootCommand::run(std::shared_ptr<Context> context) {
    using Node = nicxlive::core::nodes::Node;
    auto actualParent = parent;
    if (!actualParent) {
        compat::ngRequireObject(context);
        if (context->hasNodes() && !context->nodes().empty())
            actualParent = firstCommandObject(context->nodes(), 3820);
    }
    if (!actualParent) {
        compat::ngRequireObject(context);
        if (context->hasPuppet() && context->puppet()) {
            auto puppet = context->puppet();
            actualParent = compat::ngRequireObject(puppet)->root;
        }
    }
    if (!actualParent) detail::failInput("No parent node", 3822);
    auto created = ext::nodes::ExDepthRigRoot::create(actualParent);
    created->setName((name.empty()
        ? compat::RetainedString::fromStaticLiteral("DepthRig") : name).toNullableString());
    if (actualParent->puppetRef()) {
        auto puppet = actualParent->puppetRef();
        compat::ngRequireObject(puppet)->rescanNodes();
    }
    created->notifyChange(created, nicxlive::core::nodes::NotifyReason::StructureChanged);
    using Result = CreateResult<std::shared_ptr<Node>>;
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultNodeSliceArgument<ext::nodes::ExDepthRigRoot>{created});
}

std::shared_ptr<CommandResult> AddDepthBoneCommand::run(std::shared_ptr<Context>) {
    using Node = nicxlive::core::nodes::Node;
    if (!parent) detail::failInput("parent is required", 3843);
    if (!std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(parent) &&
        !std::dynamic_pointer_cast<ext::nodes::ExDepthBone>(parent))
        detail::failInput("parent must be DepthRigRoot or DepthBone", 3844);
    const auto sourceParent = parent;
    const auto sourceBoneId = boneId;
    const auto sourceRestHead = detail::vec3From(restHead, compat::RetainedString::fromStaticLiteral("restHead"));
    const auto sourceRestTail = detail::vec3From(restTail, compat::RetainedString::fromStaticLiteral("restTail"));
    const auto sourceRestRoll = restRoll;
    auto created = ext::nodes::ngCreateDepthBone(sourceParent, sourceBoneId,
        sourceRestHead, sourceRestTail, sourceRestRoll);
    if (compat::ngRequireObject(parent)->puppetRef()) {
        auto puppet = compat::ngRequireObject(parent)->puppetRef();
        compat::ngRequireObject(puppet)->rescanNodes();
    }
    compat::ngRequireObject(created)->notifyChange(created, nicxlive::core::nodes::NotifyReason::StructureChanged);
    using Result = CreateResult<std::shared_ptr<Node>>;
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultNodeSliceArgument<ext::nodes::ExDepthBone>{created});
}

} // namespace nijigenerate::commands::depth::bone
