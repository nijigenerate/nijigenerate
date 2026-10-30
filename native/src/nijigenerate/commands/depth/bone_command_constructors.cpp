#include "bone_commands.hpp"
#include <nijigenerate/core/translation.hpp>
#include <cstddef>
#include <utility>

namespace nijigenerate::commands::depth::bone {
namespace {
template<std::size_t LabelSize, std::size_t DescriptionSize>
std::pair<compat::RetainedString, compat::RetainedString> translatedTexts(
        const char (&labelKey)[LabelSize], const char (&descriptionKey)[DescriptionSize]) {
    auto label = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(labelKey));
    auto description = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(descriptionKey));
    return {std::move(label), std::move(description)};
}
} // namespace

// Translate source super arguments in order before entering the actual base.
#define NG_BONE_CONSTRUCTOR(Class, Label, Description) \
    std::pair<compat::RetainedString, compat::RetainedString> Class::sourceConstructorTexts() { \
        return translatedTexts(Label, Description); \
    } \
    Class::Class() : Class(sourceConstructorTexts()) {} \
    Class::Class(std::pair<compat::RetainedString, compat::RetainedString> texts) \
        : ExCommand<ArgumentsType>(std::move(texts.first), std::move(texts.second)) {}

NG_BONE_CONSTRUCTOR(CreateDepthRigRootCommand,
    "Create Depth Rig Root", "Create DepthRigRoot node")

NG_BONE_CONSTRUCTOR(AddDepthBoneCommand,
    "Add Depth Bone", "Create DepthBone node")

NG_BONE_CONSTRUCTOR(AddStandardDepthSkeletonCommand,
    "Add Standard Depth Skeleton", "Create standard depth bone hierarchy")

AddStandardDepthParametersCommand::AddStandardDepthParametersCommand(core::settings::SettingsJsonValue definitions)
    : AddStandardDepthParametersCommand(sourceConstructorTexts(), std::move(definitions)) {}

std::pair<compat::RetainedString, compat::RetainedString>
AddStandardDepthParametersCommand::sourceConstructorTexts() {
    return translatedTexts("Add Standard Depth Parameters",
        "Create standard face/body parameters and depth bone bindings");
}

void AddStandardDepthParametersCommand::initializeSourceDefinitions(core::settings::SettingsJsonValue& target,
    core::settings::SettingsJsonValue& value) {
    SourceCommandInitialization<core::settings::SettingsJsonValue,
        core::settings::SettingsJsonValue&>::construct(target, value);
}

AddStandardDepthParametersCommand::AddStandardDepthParametersCommand(
        std::pair<compat::RetainedString, compat::RetainedString> texts,
        core::settings::SettingsJsonValue definitions)
    : ExCommand<ArgumentsType>(std::move(texts.first), std::move(texts.second)) {
    initializeSourceDefinitions(bindingDefinitions, definitions);
}

NG_BONE_CONSTRUCTOR(FitDepthRigRootZToDepthCommand,
    "Fit DepthRigRoot Z to Depth", "Fit descendant DepthBone translation Z values to their current mapped depths")

NG_BONE_CONSTRUCTOR(FitDepthBoneZToDepthCommand,
    "Fit DepthBone Z to Depth", "Fit one DepthBone translation Z value to its current mapped depth")

NG_BONE_CONSTRUCTOR(SetDepthBoneRestCommand,
    "Set Depth Bone Rest", "Set rest pose for a depth bone")

NG_BONE_CONSTRUCTOR(SetDepthBoneConstraintCommand,
    "Set Depth Bone Constraint", "Set depth bone constraint")

NG_BONE_CONSTRUCTOR(ListDepthBonesCommand,
    "List Depth Bones", "List depth bones under root")

NG_BONE_CONSTRUCTOR(AddDepthBoneSourceCommand,
    "Add Depth Bone Source", "Add depth bone source to target binding")

NG_BONE_CONSTRUCTOR(RemoveDepthBoneSourceCommand,
    "Remove Depth Bone Source", "Remove depth bone source from target binding")

NG_BONE_CONSTRUCTOR(ListDepthBoneSourcesCommand,
    "List Depth Bone Sources", "List depth bone source UUIDs for target")

NG_BONE_CONSTRUCTOR(SetDepthBoneSourceSettingsCommand,
    "Set Depth Bone Source Settings", "Set per-source depth bone settings")

NG_BONE_CONSTRUCTOR(SetDepthBoneInfluenceRuleCommand,
    "Set Depth Bone Influence Rule", "Set depth bone influence rule")

NG_BONE_CONSTRUCTOR(GetDepthBoneInfluenceRuleCommand,
    "Get Depth Bone Influence Rule", "Get depth bone influence rule")

NG_BONE_CONSTRUCTOR(PreviewDepthBoneInfluenceCommand,
    "Preview Depth Bone Influence", "Preview depth bone influence")

NG_BONE_CONSTRUCTOR(PreviewDepthBoneDeformCommand,
    "Preview Depth Bone Deform", "Preview depth bone deformation")

NG_BONE_CONSTRUCTOR(ApplyDepthBoneDeformCommand,
    "Apply Depth Bone Deform", "Apply depth bone deformation to current key")
#undef NG_BONE_CONSTRUCTOR

} // namespace nijigenerate::commands::depth::bone
