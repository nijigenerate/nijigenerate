#pragma once

#include "bone.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/compat/type_sequence.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <nodes/node.hpp>
#include <limits>
#include <utility>

namespace nijigenerate::commands::depth::bone {

// Identical original TW packs retain one actual ExCommand specialization.
struct BoneCreateRootArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> parent;
    compat::RetainedString name;
};
struct BoneAddArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> parent;
    compat::RetainedString boneId;
    compat::Slice<float> restHead, restTail;
    float restRoll = std::numeric_limits<float>::quiet_NaN();
};
struct BoneRootScaleArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> root;
    float scale = std::numeric_limits<float>::quiet_NaN();
};
struct BoneRootArguments { std::shared_ptr<nicxlive::core::nodes::Node> root; };
struct BoneFitRootArguments { std::shared_ptr<nicxlive::core::nodes::Node> root; };
struct BoneFitArguments { std::shared_ptr<nicxlive::core::nodes::Node> bone; };
struct BoneRestArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> bone;
    compat::Slice<float> restHead, restTail;
    float restRoll = std::numeric_limits<float>::quiet_NaN();
};
struct BoneConstraintArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> bone;
    compat::RetainedString constraint;
};
struct BoneSourceArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> root, target, bone;
};
struct BoneTargetArguments { std::shared_ptr<nicxlive::core::nodes::Node> root, target; };
struct BoneSourceSettingsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> root, target, bone;
    compat::RetainedString settings;
};
struct BoneRuleArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> root, target;
    compat::RetainedString rule;
};
struct BoneTargetsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> root;
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> targets;
};

} // namespace nijigenerate::commands::depth::bone

namespace nijigenerate::commands {

#define NG_BONE_FIELD(Storage, Member, Help, Type) \
    FieldDescriptor<&depth::bone::Storage::Member, #Member, Help, Type>{}
#define NG_BONE_FIELDS(Storage, ...) \
    template<> struct CommandFields<depth::bone::Storage> { \
        static constexpr auto descriptors = std::tuple{__VA_ARGS__}; \
    }
NG_BONE_FIELDS(BoneCreateRootArguments,
    NG_BONE_FIELD(BoneCreateRootArguments, parent, "Parent node", "Node"),
    NG_BONE_FIELD(BoneCreateRootArguments, name, "Depth rig root name", "string"));
NG_BONE_FIELDS(BoneAddArguments,
    NG_BONE_FIELD(BoneAddArguments, parent, "DepthRigRoot or DepthBone parent", "Node"),
    NG_BONE_FIELD(BoneAddArguments, boneId, "Bone id", "string"),
    NG_BONE_FIELD(BoneAddArguments, restHead, "Rest head [x,y,z]", "float[]"),
    NG_BONE_FIELD(BoneAddArguments, restTail, "Rest tail [x,y,z]", "float[]"),
    NG_BONE_FIELD(BoneAddArguments, restRoll, "Rest roll", "float"));
NG_BONE_FIELDS(BoneRootScaleArguments,
    NG_BONE_FIELD(BoneRootScaleArguments, root, "DepthRigRoot node", "Node"),
    NG_BONE_FIELD(BoneRootScaleArguments, scale, "Template scale", "float"));
NG_BONE_FIELDS(BoneRootArguments, NG_BONE_FIELD(BoneRootArguments, root, "DepthRigRoot node", "Node"));
NG_BONE_FIELDS(BoneFitRootArguments,
    NG_BONE_FIELD(BoneFitRootArguments, root,
        "DepthRigRoot node whose descendant DepthBones will be fitted to mapped depth", "Node"));
NG_BONE_FIELDS(BoneFitArguments,
    NG_BONE_FIELD(BoneFitArguments, bone, "DepthBone node to fit to its current mapped depth", "Node"));
NG_BONE_FIELDS(BoneRestArguments,
    NG_BONE_FIELD(BoneRestArguments, bone, "DepthBone node", "Node"),
    NG_BONE_FIELD(BoneRestArguments, restHead, "Rest head [x,y,z]", "float[]"),
    NG_BONE_FIELD(BoneRestArguments, restTail, "Rest tail [x,y,z]", "float[]"),
    NG_BONE_FIELD(BoneRestArguments, restRoll, "Rest roll", "float"));
NG_BONE_FIELDS(BoneConstraintArguments,
    NG_BONE_FIELD(BoneConstraintArguments, bone, "DepthBone node", "Node"),
    NG_BONE_FIELD(BoneConstraintArguments, constraint, "Constraint JSON", "string"));
NG_BONE_FIELDS(BoneSourceArguments,
    NG_BONE_FIELD(BoneSourceArguments, root, "DepthRigRoot node", "Node"),
    NG_BONE_FIELD(BoneSourceArguments, target, "GridDeformer or PathDeformer target", "Node"),
    NG_BONE_FIELD(BoneSourceArguments, bone, "DepthBone source", "Node"));
NG_BONE_FIELDS(BoneTargetArguments,
    NG_BONE_FIELD(BoneTargetArguments, root, "DepthRigRoot node", "Node"),
    NG_BONE_FIELD(BoneTargetArguments, target, "GridDeformer or PathDeformer target", "Node"));
NG_BONE_FIELDS(BoneSourceSettingsArguments,
    NG_BONE_FIELD(BoneSourceSettingsArguments, root, "DepthRigRoot node", "Node"),
    NG_BONE_FIELD(BoneSourceSettingsArguments, target, "GridDeformer or PathDeformer target", "Node"),
    NG_BONE_FIELD(BoneSourceSettingsArguments, bone, "DepthBone source", "Node"),
    NG_BONE_FIELD(BoneSourceSettingsArguments, settings, "Source settings JSON", "string"));
NG_BONE_FIELDS(BoneRuleArguments,
    NG_BONE_FIELD(BoneRuleArguments, root, "DepthRigRoot node", "Node"),
    NG_BONE_FIELD(BoneRuleArguments, target, "GridDeformer or PathDeformer target", "Node"),
    NG_BONE_FIELD(BoneRuleArguments, rule, "Influence rule JSON", "string"));
NG_BONE_FIELDS(BoneTargetsArguments,
    NG_BONE_FIELD(BoneTargetsArguments, root, "DepthRigRoot node", "Node"),
    NG_BONE_FIELD(BoneTargetsArguments, targets, "GridDeformer or PathDeformer targets", "Node[]"));
#undef NG_BONE_FIELDS
#undef NG_BONE_FIELD

} // namespace nijigenerate::commands

namespace nijigenerate::commands::depth::bone {

// These macros declare real concrete types, not successful generic handlers.
// Every run is an explicit required override, so missing bodies cannot fall
// through to ExCommand's default run implementation.
#define NG_BONE_COMMAND(Class, Storage) \
    class Class : public ExCommand<Storage> { \
    public: \
        using ArgumentsType = Storage; \
        Class(); \
        Class(const Class&) = delete; \
        Class& operator=(const Class&) = delete; \
        std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override; \
    private: \
        static std::pair<compat::RetainedString, compat::RetainedString> sourceConstructorTexts(); \
        friend struct ::nijigenerate::commands::SourceCommandConstructor<Class>; \
        explicit Class(std::pair<compat::RetainedString, compat::RetainedString> texts); \
    }
NG_BONE_COMMAND(CreateDepthRigRootCommand, BoneCreateRootArguments);
NG_BONE_COMMAND(AddDepthBoneCommand, BoneAddArguments);
NG_BONE_COMMAND(AddStandardDepthSkeletonCommand, BoneRootScaleArguments);
class AddStandardDepthParametersCommand : public ExCommand<BoneRootArguments> {
    core::settings::SettingsJsonValue bindingDefinitions;
    static std::pair<compat::RetainedString, compat::RetainedString> sourceConstructorTexts();
    static void initializeSourceDefinitions(core::settings::SettingsJsonValue& target,
        core::settings::SettingsJsonValue& value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddStandardDepthParametersCommand>;
    AddStandardDepthParametersCommand(std::pair<compat::RetainedString, compat::RetainedString> texts,
        core::settings::SettingsJsonValue definitions);
public:
    using ArgumentsType = BoneRootArguments;
    explicit AddStandardDepthParametersCommand(core::settings::SettingsJsonValue definitions = {});
    AddStandardDepthParametersCommand(const AddStandardDepthParametersCommand&) = delete;
    AddStandardDepthParametersCommand& operator=(const AddStandardDepthParametersCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
NG_BONE_COMMAND(FitDepthRigRootZToDepthCommand, BoneFitRootArguments);
NG_BONE_COMMAND(FitDepthBoneZToDepthCommand, BoneFitArguments);
NG_BONE_COMMAND(SetDepthBoneRestCommand, BoneRestArguments);
NG_BONE_COMMAND(SetDepthBoneConstraintCommand, BoneConstraintArguments);
NG_BONE_COMMAND(ListDepthBonesCommand, BoneRootArguments);
NG_BONE_COMMAND(AddDepthBoneSourceCommand, BoneSourceArguments);
NG_BONE_COMMAND(RemoveDepthBoneSourceCommand, BoneSourceArguments);
NG_BONE_COMMAND(ListDepthBoneSourcesCommand, BoneTargetArguments);
NG_BONE_COMMAND(SetDepthBoneSourceSettingsCommand, BoneSourceSettingsArguments);
NG_BONE_COMMAND(SetDepthBoneInfluenceRuleCommand, BoneRuleArguments);
NG_BONE_COMMAND(GetDepthBoneInfluenceRuleCommand, BoneTargetArguments);
NG_BONE_COMMAND(PreviewDepthBoneInfluenceCommand, BoneSourceArguments);
NG_BONE_COMMAND(PreviewDepthBoneDeformCommand, BoneTargetsArguments);
NG_BONE_COMMAND(ApplyDepthBoneDeformCommand, BoneTargetsArguments);
#undef NG_BONE_COMMAND

template<DepthBoneCommand> struct DepthBoneCommandType;
#define NG_BONE_COMMAND_TYPE(Id, Class) \
    template<> struct DepthBoneCommandType<DepthBoneCommand::Id> { \
        using Type = Class; \
        static constexpr CommandDispatchSource source{"DepthBoneCommand." #Id, "DepthBoneCommand", #Class}; \
    }
NG_BONE_COMMAND_TYPE(CreateDepthRigRoot, CreateDepthRigRootCommand);
NG_BONE_COMMAND_TYPE(AddDepthBone, AddDepthBoneCommand);
NG_BONE_COMMAND_TYPE(AddStandardDepthSkeleton, AddStandardDepthSkeletonCommand);
NG_BONE_COMMAND_TYPE(AddStandardDepthParameters, AddStandardDepthParametersCommand);
NG_BONE_COMMAND_TYPE(FitDepthRigRootZToDepth, FitDepthRigRootZToDepthCommand);
NG_BONE_COMMAND_TYPE(FitDepthBoneZToDepth, FitDepthBoneZToDepthCommand);
NG_BONE_COMMAND_TYPE(SetDepthBoneRest, SetDepthBoneRestCommand);
NG_BONE_COMMAND_TYPE(SetDepthBoneConstraint, SetDepthBoneConstraintCommand);
NG_BONE_COMMAND_TYPE(ListDepthBones, ListDepthBonesCommand);
NG_BONE_COMMAND_TYPE(AddDepthBoneSource, AddDepthBoneSourceCommand);
NG_BONE_COMMAND_TYPE(RemoveDepthBoneSource, RemoveDepthBoneSourceCommand);
NG_BONE_COMMAND_TYPE(ListDepthBoneSources, ListDepthBoneSourcesCommand);
NG_BONE_COMMAND_TYPE(SetDepthBoneSourceSettings, SetDepthBoneSourceSettingsCommand);
NG_BONE_COMMAND_TYPE(SetDepthBoneInfluenceRule, SetDepthBoneInfluenceRuleCommand);
NG_BONE_COMMAND_TYPE(GetDepthBoneInfluenceRule, GetDepthBoneInfluenceRuleCommand);
NG_BONE_COMMAND_TYPE(PreviewDepthBoneInfluence, PreviewDepthBoneInfluenceCommand);
NG_BONE_COMMAND_TYPE(PreviewDepthBoneDeform, PreviewDepthBoneDeformCommand);
NG_BONE_COMMAND_TYPE(ApplyDepthBoneDeform, ApplyDepthBoneDeformCommand);
#undef NG_BONE_COMMAND_TYPE

void ngInitDepthBoneCommands();

template<DepthBoneCommand id, class... Args>
std::shared_ptr<CommandResult> ngRunDepthBoneCommand(std::shared_ptr<Context> context, Args&&... args) {
    using Concrete = typename DepthBoneCommandType<id>::Type;
    return ngDispatchRegisteredCommand<Concrete>(commands, id, context, DepthBoneCommandType<id>::source,
        std::forward<Args>(args)...);
}

} // namespace nijigenerate::commands::depth::bone

namespace nijigenerate::commands {

#define NG_BONE_TRAITS(Class, ...) \
    template<> struct CommandTraits<depth::bone::Class> { \
        static constexpr auto attributes = std::tuple{__VA_ARGS__}; \
        using BaseClasses = CommandTypeList<ExCommand<typename depth::bone::Class::ArgumentsType>, Command>; \
    }
NG_BONE_TRAITS(CreateDepthRigRootCommand, EffectCreate);
NG_BONE_TRAITS(AddDepthBoneCommand, EffectCreate);
NG_BONE_TRAITS(AddStandardDepthSkeletonCommand, EffectCreate);
NG_BONE_TRAITS(AddStandardDepthParametersCommand, EffectBindingEdit);
NG_BONE_TRAITS(FitDepthRigRootZToDepthCommand, McpTool, EffectConfigEdit);
NG_BONE_TRAITS(FitDepthBoneZToDepthCommand, McpTool, EffectConfigEdit);
NG_BONE_TRAITS(SetDepthBoneRestCommand, EffectStructuralEdit);
NG_BONE_TRAITS(SetDepthBoneConstraintCommand, EffectConfigEdit);
NG_BONE_TRAITS(ListDepthBonesCommand, ShortcutHidden);
NG_BONE_TRAITS(AddDepthBoneSourceCommand, EffectConfigEdit);
NG_BONE_TRAITS(RemoveDepthBoneSourceCommand, EffectConfigEdit);
NG_BONE_TRAITS(ListDepthBoneSourcesCommand, ShortcutHidden);
NG_BONE_TRAITS(SetDepthBoneSourceSettingsCommand, EffectConfigEdit);
NG_BONE_TRAITS(SetDepthBoneInfluenceRuleCommand, EffectConfigEdit);
NG_BONE_TRAITS(GetDepthBoneInfluenceRuleCommand, ShortcutHidden);
NG_BONE_TRAITS(PreviewDepthBoneInfluenceCommand);
NG_BONE_TRAITS(PreviewDepthBoneDeformCommand);
NG_BONE_TRAITS(ApplyDepthBoneDeformCommand, EffectBindingEdit);
#undef NG_BONE_TRAITS

#define NG_BONE_CONSTRUCTOR_PROGRAM(Class) \
template<> struct SourceCommandConstructor<depth::bone::Class> { \
    template<class... Args> struct Program { \
        using C = depth::bone::Class; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { C::sourceConstructorTexts(); } \
                && SourceExCommandConstructorProgram<typename C::ArgumentsType>::pairSupported(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_BONE_CONSTRUCTOR_PROGRAM(CreateDepthRigRootCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(AddDepthBoneCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(AddStandardDepthSkeletonCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(FitDepthRigRootZToDepthCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(FitDepthBoneZToDepthCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(SetDepthBoneRestCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(SetDepthBoneConstraintCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(ListDepthBonesCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(AddDepthBoneSourceCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(RemoveDepthBoneSourceCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(ListDepthBoneSourcesCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(SetDepthBoneSourceSettingsCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(SetDepthBoneInfluenceRuleCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(GetDepthBoneInfluenceRuleCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(PreviewDepthBoneInfluenceCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(PreviewDepthBoneDeformCommand)
NG_BONE_CONSTRUCTOR_PROGRAM(ApplyDepthBoneDeformCommand)
#undef NG_BONE_CONSTRUCTOR_PROGRAM

template<> struct SourceCommandConstructor<depth::bone::AddStandardDepthParametersCommand> {
    template<class... Args> struct Program {
        using C = depth::bone::AddStandardDepthParametersCommand;
        using Json = core::settings::SettingsJsonValue;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 1) return false;
            else return (ngCommandInitializationSupported<Json, Args&&>() && ...) && requires(Json& value) {
                C::sourceConstructorTexts();
                C::initializeSourceDefinitions(value, value);
            } && ngCommandInitializationSupported<Json, Json&>()
                && ngCommandParameterCopySupported<Json, Json&>()
                && SourceExCommandConstructorProgram<typename C::ArgumentsType>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            if constexpr (sizeof...(Args) == 0) return std::allocate_shared<C>(allocator);
            else return detail::constructDeclaredSourceCommand<C, std::tuple<Json>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::depth::bone {
namespace initialization {
template<DepthBoneCommand Id> using Key = std::integral_constant<DepthBoneCommand, Id>;
using Keys = compat::TypeSequence<
    Key<DepthBoneCommand::CreateDepthRigRoot>,
    Key<DepthBoneCommand::AddDepthBone>,
    Key<DepthBoneCommand::AddStandardDepthSkeleton>,
    Key<DepthBoneCommand::AddStandardDepthParameters>,
    Key<DepthBoneCommand::FitDepthRigRootZToDepth>,
    Key<DepthBoneCommand::FitDepthBoneZToDepth>,
    Key<DepthBoneCommand::SetDepthBoneRest>,
    Key<DepthBoneCommand::SetDepthBoneConstraint>,
    Key<DepthBoneCommand::ListDepthBones>,
    Key<DepthBoneCommand::AddDepthBoneSource>,
    Key<DepthBoneCommand::RemoveDepthBoneSource>,
    Key<DepthBoneCommand::ListDepthBoneSources>,
    Key<DepthBoneCommand::SetDepthBoneSourceSettings>,
    Key<DepthBoneCommand::SetDepthBoneInfluenceRule>,
    Key<DepthBoneCommand::GetDepthBoneInfluenceRule>,
    Key<DepthBoneCommand::PreviewDepthBoneInfluence>,
    Key<DepthBoneCommand::PreviewDepthBoneDeform>,
    Key<DepthBoneCommand::ApplyDepthBoneDeform>>;
template<DepthBoneCommand Id> consteval bool registrationSupported() {
    return ngCommandRegistrationSupported<Id, typename DepthBoneCommandType<Id>::Type, DepthBoneCommandMap>();
}
template<class... Item> consteval bool supported(compat::TypeSequence<Item...>) {
    return (registrationSupported<Item::value>() && ...);
}
}
template<class T> requires (std::same_as<T, DepthBoneCommand> &&
    initialization::supported(initialization::Keys{}))
void ngInitCommands() { ngInitDepthBoneCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<depth::bone::DepthBoneCommand> {
    static consteval bool supported() { return depth::bone::initialization::supported(depth::bone::initialization::Keys{}); }
    template<class ActualKey = depth::bone::DepthBoneCommand>
        requires (std::same_as<ActualKey, depth::bone::DepthBoneCommand> && supported())
    static void invoke() { depth::bone::ngInitCommands<ActualKey>(); }
};
}
