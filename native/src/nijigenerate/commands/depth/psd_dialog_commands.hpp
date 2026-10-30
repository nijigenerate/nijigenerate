#pragma once
#include "psd_dialog.hpp"
#include "psd_dialog_scope.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/source_diagnostic_text.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/io/depthmap_psd_types.hpp>
#include <nijigenerate/windows/psddepthmap_complete_values.hpp>
#include <memory>
#include <concepts>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::windows::psddepthmap { class PSDDepthMapWindow; }
namespace nijigenerate::commands::depth::psd_dialog {
using ContextRef = std::shared_ptr<Context>;
using ResultRef = std::shared_ptr<CommandResult>;
using SettingsState = windows::psddepthmap::PsdDepthDialogSettingsState;
using LayerState = windows::psddepthmap::PsdDepthDialogLayerState;

// Additional source value types must supply their real name and .init mapping.
template<class T> struct PsdDepthDialogValueContract;
template<class T, CommandLiteral Name> struct PsdDepthDialogScalarContract {
    static constexpr auto typeName = Name;
    static T initialValue() { return commands::detail::CommandResultInitialValue<T>::get(); }
};
template<> struct PsdDepthDialogValueContract<bool> : PsdDepthDialogScalarContract<bool, "bool"> {};
template<> struct PsdDepthDialogValueContract<std::int32_t>
    : PsdDepthDialogScalarContract<std::int32_t, "int"> {};
template<> struct PsdDepthDialogValueContract<float> : PsdDepthDialogScalarContract<float, "float"> {};
template<> struct PsdDepthDialogValueContract<compat::RetainedString>
    : PsdDepthDialogScalarContract<compat::RetainedString, "string"> {};
template<class T, CommandLiteral Name> struct PsdDepthDialogEnumContract {
    static constexpr auto typeName = Name;
    static T initialValue() { return commands::detail::CommandResultInitialValue<T>::get(); }
};
template<> struct PsdDepthDialogValueContract<io::depthmap_psd::PsdDepthChannel>
    : PsdDepthDialogEnumContract<io::depthmap_psd::PsdDepthChannel, "PsdDepthChannel"> {};
template<> struct PsdDepthDialogValueContract<io::depthmap_psd::PsdDepthConvolution>
    : PsdDepthDialogEnumContract<io::depthmap_psd::PsdDepthConvolution, "PsdDepthConvolution"> {};
template<> struct PsdDepthDialogValueContract<io::depthmap_psd::PsdDepthMissingPolicy>
    : PsdDepthDialogEnumContract<io::depthmap_psd::PsdDepthMissingPolicy, "PsdDepthMissingPolicy"> {};

template<class T, CommandLiteral Description> struct PsdDepthDialogSettingsValueArguments {
    T value = PsdDepthDialogValueContract<T>::initialValue();
};
template<class T, CommandLiteral Description> struct PsdDepthDialogLayerValueArguments {
    compat::RetainedString layerPath;
    T value = PsdDepthDialogValueContract<T>::initialValue();
};
namespace detail {
// Real constructor argument values, captured in the Source expression order.
struct CommandPairArguments {
    compat::RetainedString label;
    compat::RetainedString description;
};
struct SettingsConstructorArguments {
    compat::RetainedString label;
    compat::RetainedString description;
    compat::RetainedString key;
    compat::RetainedString changeLabel;
    bool mergeable = false;
};
struct LayerConstructorArguments {
    compat::RetainedString label;
    compat::RetainedString description;
    compat::RetainedString key;
    compat::RetainedString changeLabel;
    bool stageForApply = false;
    bool mergeable = false;
};
template<class T, CommandLiteral... Description> struct SettingsValueStorage;
template<class Storage> struct SettingsValueStorage<Storage> { using Type = Storage; };
template<class T, CommandLiteral Description> struct SettingsValueStorage<T, Description> {
    using Type = PsdDepthDialogSettingsValueArguments<T, Description>;
};
template<class T, CommandLiteral... Description> struct LayerValueStorage;
template<class Storage> struct LayerValueStorage<Storage> { using Type = Storage; };
template<class T, CommandLiteral Description> struct LayerValueStorage<T, Description> {
    using Type = PsdDepthDialogLayerValueArguments<T, Description>;
};
using ValueCommandDialog = std::shared_ptr<windows::psddepthmap::PSDDepthMapWindow>;
ValueCommandDialog ngValueCommandDialog(const ContextRef& ctx);
bool ngValueCommandAvailable(const ValueCommandDialog& dialog);
SettingsState ngValueCommandSettings(const ValueCommandDialog& dialog);
bool ngApplyValueCommandSettings(const ValueCommandDialog& dialog, const SettingsState& state);
bool ngCaptureValueCommandLayer(const ValueCommandDialog& dialog, const ContextRef& ctx,
    const compat::RetainedString& path, LayerState& state);
bool ngApplyValueCommandLayer(const ValueCommandDialog& dialog, const LayerState& state, const bool& stage);
void ngPushValueCommandSettings(const ValueCommandDialog& dialog, const SettingsState& oldState,
    const SettingsState& nextState, const compat::RetainedString& key,
    const compat::RetainedString& label, const bool& mergeable);
void ngPushValueCommandLayer(const ValueCommandDialog& dialog, const LayerState& oldState,
    const LayerState& nextState, const compat::RetainedString& key, const compat::RetainedString& path,
    const compat::RetainedString& label, const bool& stage, const bool& mergeable);
}
struct SetPsdDepthDialogColorSourceArguments {
    compat::RetainedString value{};
};
struct SetPsdDepthDialogInvertArguments {
    bool value{};
};
struct SetPsdDepthDialogBackDepthArguments {
    float value = std::numeric_limits<float>::quiet_NaN();
};
struct SetPsdDepthDialogFrontDepthArguments {
    float value = std::numeric_limits<float>::quiet_NaN();
};
struct SetPsdDepthDialogDepthScaleArguments {
    float value = std::numeric_limits<float>::quiet_NaN();
};
struct SetPsdDepthDialogChannelArguments {
    io::depthmap_psd::PsdDepthChannel value{};
};
struct SetPsdDepthDialogSamplingArguments {
    io::depthmap_psd::PsdDepthConvolution value{};
};
struct SetPsdDepthDialogCustomRadiusArguments {
    std::int32_t value{};
};
struct SetPsdDepthDialogAlphaThresholdArguments {
    float value = std::numeric_limits<float>::quiet_NaN();
};
struct SetPsdDepthDialogMissingPolicyArguments {
    io::depthmap_psd::PsdDepthMissingPolicy value{};
};
struct SetPsdDepthDialogContourRepairArguments {
    bool value{};
};
struct SetPsdDepthDialogSurfaceSmoothingArguments {
    bool value{};
};
struct SetPsdDepthDialogGpuCompositionArguments {
    bool value{};
};
struct SetPsdDepthDialogDirectGridMatchArguments {
    bool value{};
};
struct SetPsdDepthDialogProblemFilterArguments {
    bool value{};
};
struct SetPsdDepthDialogLayerMappingAutoArguments {
    compat::RetainedString layerPath{};
};
struct SetPsdDepthDialogLayerMappingIgnoredArguments {
    compat::RetainedString layerPath{};
};
struct SetPsdDepthDialogLayerMappingTargetArguments {
    compat::RetainedString layerPath{};
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
};
struct SetPsdDepthDialogTargetEnabledArguments {
    bool value{};
};
struct SetPsdDepthDialogLayerEnabledArguments {
    compat::RetainedString layerPath{};
    bool value{};
};
struct SetPsdDepthDialogLayerVisibleArguments {
    compat::RetainedString layerPath{};
    bool value{};
};
struct SetPsdDepthDialogLayerDepthEnabledArguments {
    compat::RetainedString layerPath{};
    bool value{};
};
struct SetPsdDepthDialogLayerDepthInvertedArguments {
    compat::RetainedString layerPath{};
    bool value{};
};
struct SetPsdDepthDialogLayerDepthScaleArguments {
    compat::RetainedString layerPath{};
    float value = std::numeric_limits<float>::quiet_NaN();
};
struct SetPsdDepthDialogLayerDepthOffsetArguments {
    compat::RetainedString layerPath{};
    float value = std::numeric_limits<float>::quiet_NaN();
};
struct ResetPsdDepthDialogLayerDepthTransformArguments {
    compat::RetainedString layerPath{};
};
}
namespace nijigenerate::commands {
template<class T, CommandLiteral Description>
struct CommandFields<depth::psd_dialog::PsdDepthDialogSettingsValueArguments<T, Description>> {
    using Storage = depth::psd_dialog::PsdDepthDialogSettingsValueArguments<T, Description>;
    using Contract = depth::psd_dialog::PsdDepthDialogValueContract<T>;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::value, "value", Description, Contract::typeName>{}};
};
template<class T, CommandLiteral Description>
struct CommandFields<depth::psd_dialog::PsdDepthDialogLayerValueArguments<T, Description>> {
    using Storage = depth::psd_dialog::PsdDepthDialogLayerValueArguments<T, Description>;
    using Contract = depth::psd_dialog::PsdDepthDialogValueContract<T>;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::layerPath, "layerPath", "PSD composed layer path", "string">{},
        FieldDescriptor<&Storage::value, "value", Description, Contract::typeName>{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogColorSourceArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogColorSourceArguments::value, "value", "Color source image path; empty uses active target artwork", "string">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogInvertArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogInvertArguments::value, "value", "Whether white and black depth interpretation is inverted", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogBackDepthArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogBackDepthArguments::value, "value", "Depth assigned to the back of the imported range", "float">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogFrontDepthArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogFrontDepthArguments::value, "value", "Depth assigned to the front of the imported range", "float">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogDepthScaleArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogDepthScaleArguments::value, "value", "Finite non-negative multiplier applied to imported depth values", "float">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogChannelArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogChannelArguments::value, "value", "Image channel used to read depth", "PsdDepthChannel">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogSamplingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogSamplingArguments::value, "value", "Sampling filter used to read depth", "PsdDepthConvolution">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogCustomRadiusArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogCustomRadiusArguments::value, "value", "Radius used by custom sampling filters in [1, 64]", "int">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogAlphaThresholdArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogAlphaThresholdArguments::value, "value", "Finite minimum alpha accepted as a valid depth sample in [0, 1]", "float">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogMissingPolicyArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogMissingPolicyArguments::value, "value", "Policy for vertices without a valid depth pixel", "PsdDepthMissingPolicy">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogContourRepairArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogContourRepairArguments::value, "value", "Whether the contour band is repaired", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingArguments::value, "value", "Whether wavy depth surfaces are smoothed", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogGpuCompositionArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogGpuCompositionArguments::value, "value", "Whether composition must use the GPU path", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogDirectGridMatchArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogDirectGridMatchArguments::value, "value", "Whether layer names may directly match GridDeformer names", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogProblemFilterArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogProblemFilterArguments::value, "value", "Whether Source / Mapping shows only problem layers", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoArguments::layerPath, "layerPath", "PSD source layer path to return to automatic mapping", "string">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredArguments::layerPath, "layerPath", "PSD source layer path to ignore", "string">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetArguments::layerPath, "layerPath", "PSD source layer path to remap", "string">{},
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetArguments::target, "target", "GridDeformer or PathDeformer selected as the mapping target", "Node">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogTargetEnabledArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogTargetEnabledArguments::value, "value", "Whether the Context target GridDeformer or PathDeformer is used", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerEnabledArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerEnabledArguments::layerPath, "layerPath", "PSD composed layer path", "string">{},
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerEnabledArguments::value, "value", "Whether the composed layer is used", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerVisibleArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerVisibleArguments::layerPath, "layerPath", "PSD composed layer path", "string">{},
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerVisibleArguments::value, "value", "Whether the selected layer is shown in 3D Adjust", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledArguments::layerPath, "layerPath", "PSD composed layer path", "string">{},
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledArguments::value, "value", "Whether the selected layer supplies its own depth", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedArguments::layerPath, "layerPath", "PSD composed layer path", "string">{},
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedArguments::value, "value", "Whether depth is inverted for the selected layer", "bool">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleArguments::layerPath, "layerPath", "PSD composed layer path", "string">{},
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleArguments::value, "value", "Z scale applied to the selected layer", "float">{}};
};
template<> struct CommandFields<depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetArguments::layerPath, "layerPath", "PSD composed layer path", "string">{},
        FieldDescriptor<&depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetArguments::value, "value", "Z offset applied to the selected layer", "float">{}};
};
template<> struct CommandFields<depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformArguments::layerPath, "layerPath", "PSD composed layer path to reset", "string">{}};
};
}
namespace nijigenerate::commands::depth::psd_dialog {
template<class T, CommandLiteral... Description> class PsdDepthDialogSettingsValueCommand
    : public ExCommand<typename detail::SettingsValueStorage<T, Description...>::Type>,
      public std::enable_shared_from_this<PsdDepthDialogSettingsValueCommand<T, Description...>> {
public:
    using Storage = typename detail::SettingsValueStorage<T, Description...>::Type;
    struct SourceConstructorProgram {
        using Self = PsdDepthDialogSettingsValueCommand;
        using String = compat::RetainedString;
        static consteval bool supported() {
            return SourceExCommandConstructorProgram<Storage>::pairSupported() &&
                ngCommandInitializationSupported<String, String&>() &&
                ngCommandInitializationSupported<bool, bool&>();
        }
        static void initialize(Self& target, String& key, String& label, bool& mergeable)
            requires(supported()) {
            SourceCommandInitialization<String, String&>::construct(target.changeKey, key);
            SourceCommandInitialization<String, String&>::construct(target.changeLabel, label);
            SourceCommandInitialization<bool, bool&>::construct(target.mergeable, mergeable);
        }
    };
    PsdDepthDialogSettingsValueCommand(const PsdDepthDialogSettingsValueCommand&) = delete;
    PsdDepthDialogSettingsValueCommand& operator=(const PsdDepthDialogSettingsValueCommand&) = delete;
    PsdDepthDialogSettingsValueCommand(PsdDepthDialogSettingsValueCommand&&) = delete;
    PsdDepthDialogSettingsValueCommand& operator=(PsdDepthDialogSettingsValueCommand&&) = delete;
protected:
    void prepareSourceClassInfo() override;
    PsdDepthDialogSettingsValueCommand(compat::RetainedString label, compat::RetainedString description,
        compat::RetainedString key, compat::RetainedString changeLabel, bool mergeable = false)
        requires(SourceConstructorProgram::supported());
    virtual void update(SettingsState& state) = 0;
    virtual compat::RetainedString validate(SettingsState& state) { return {}; }
public:
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
private:
    compat::RetainedString changeKey, changeLabel;
    bool mergeable = false;
};
template<class T, CommandLiteral... Description> class PsdDepthDialogLayerValueCommand
    : public ExCommand<typename detail::LayerValueStorage<T, Description...>::Type>,
      public std::enable_shared_from_this<PsdDepthDialogLayerValueCommand<T, Description...>> {
public:
    using Storage = typename detail::LayerValueStorage<T, Description...>::Type;
    struct SourceConstructorProgram {
        using Self = PsdDepthDialogLayerValueCommand;
        using String = compat::RetainedString;
        static consteval bool supported() {
            return SourceExCommandConstructorProgram<Storage>::pairSupported() &&
                ngCommandInitializationSupported<String, String&>() &&
                ngCommandInitializationSupported<bool, bool&>();
        }
        static void initialize(Self& target, String& key, String& label, bool& stage, bool& mergeable)
            requires(supported()) {
            SourceCommandInitialization<String, String&>::construct(target.changeKey, key);
            SourceCommandInitialization<String, String&>::construct(target.changeLabel, label);
            SourceCommandInitialization<bool, bool&>::construct(target.stageForApply, stage);
            SourceCommandInitialization<bool, bool&>::construct(target.mergeable, mergeable);
        }
    };
    PsdDepthDialogLayerValueCommand(const PsdDepthDialogLayerValueCommand&) = delete;
    PsdDepthDialogLayerValueCommand& operator=(const PsdDepthDialogLayerValueCommand&) = delete;
    PsdDepthDialogLayerValueCommand(PsdDepthDialogLayerValueCommand&&) = delete;
    PsdDepthDialogLayerValueCommand& operator=(PsdDepthDialogLayerValueCommand&&) = delete;
protected:
    void prepareSourceClassInfo() override;
    PsdDepthDialogLayerValueCommand(compat::RetainedString label, compat::RetainedString description,
        compat::RetainedString key, compat::RetainedString changeLabel, bool stageForApply, bool mergeable = false)
        requires(SourceConstructorProgram::supported());
    virtual void update(LayerState& state) = 0;
public:
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
private:
    compat::RetainedString changeKey, changeLabel;
    bool stageForApply = false;
    bool mergeable = false;
};
class InspectPsdDepthDialogCommand
    : public ::nijigenerate::commands::detail::CovariantCommand<InspectPsdDepthDialogCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<InspectPsdDepthDialogCommand>;
private:
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit InspectPsdDepthDialogCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<InspectPsdDepthDialogCommand>;
public:
    using Storage = CommandNoArguments;
    using Result = ExCommandResult<core::settings::SettingsJsonValue>;
    InspectPsdDepthDialogCommand();
    InspectPsdDepthDialogCommand(const InspectPsdDepthDialogCommand&) = delete;
    InspectPsdDepthDialogCommand& operator=(const InspectPsdDepthDialogCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    virtual std::shared_ptr<Result> run(const ContextRef& ctx);
    virtual std::shared_ptr<Result> runTyped(ContextRef ctx);
};
class GetPsdDepthDialogPartDataCommand
    : public ::nijigenerate::commands::detail::CovariantCommand<GetPsdDepthDialogPartDataCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<GetPsdDepthDialogPartDataCommand>;
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit GetPsdDepthDialogPartDataCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<GetPsdDepthDialogPartDataCommand>;
public:
    using Storage = CommandNoArguments;
    using Result = ExCommandResult<core::settings::SettingsJsonValue>;
    GetPsdDepthDialogPartDataCommand();
    GetPsdDepthDialogPartDataCommand(const GetPsdDepthDialogPartDataCommand&) = delete;
    GetPsdDepthDialogPartDataCommand& operator=(const GetPsdDepthDialogPartDataCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    virtual std::shared_ptr<Result> run(const ContextRef& ctx);
    virtual std::shared_ptr<Result> runTyped(ContextRef ctx);
};
class SetPsdDepthDialogColorSourceCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogColorSourceArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogColorSourceCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogColorSourceCommand>;
public:
    using Storage = SetPsdDepthDialogColorSourceArguments;
    SetPsdDepthDialogColorSourceCommand();
    SetPsdDepthDialogColorSourceCommand(const SetPsdDepthDialogColorSourceCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogInvertCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogInvertArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogInvertCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogInvertCommand>;
public:
    using Storage = SetPsdDepthDialogInvertArguments;
    SetPsdDepthDialogInvertCommand();
    SetPsdDepthDialogInvertCommand(const SetPsdDepthDialogInvertCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogBackDepthCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogBackDepthArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogBackDepthCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogBackDepthCommand>;
public:
    using Storage = SetPsdDepthDialogBackDepthArguments;
    SetPsdDepthDialogBackDepthCommand();
    SetPsdDepthDialogBackDepthCommand(const SetPsdDepthDialogBackDepthCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogFrontDepthCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogFrontDepthArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogFrontDepthCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogFrontDepthCommand>;
public:
    using Storage = SetPsdDepthDialogFrontDepthArguments;
    SetPsdDepthDialogFrontDepthCommand();
    SetPsdDepthDialogFrontDepthCommand(const SetPsdDepthDialogFrontDepthCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogDepthScaleCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogDepthScaleArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogDepthScaleCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogDepthScaleCommand>;
public:
    using Storage = SetPsdDepthDialogDepthScaleArguments;
    SetPsdDepthDialogDepthScaleCommand();
    SetPsdDepthDialogDepthScaleCommand(const SetPsdDepthDialogDepthScaleCommand&) = delete;
public:
    void update(SettingsState& state) override;
    compat::RetainedString validate(SettingsState& state) override;
};
class SetPsdDepthDialogChannelCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogChannelArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogChannelCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogChannelCommand>;
public:
    using Storage = SetPsdDepthDialogChannelArguments;
    SetPsdDepthDialogChannelCommand();
    SetPsdDepthDialogChannelCommand(const SetPsdDepthDialogChannelCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogSamplingCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogSamplingArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogSamplingCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogSamplingCommand>;
public:
    using Storage = SetPsdDepthDialogSamplingArguments;
    SetPsdDepthDialogSamplingCommand();
    SetPsdDepthDialogSamplingCommand(const SetPsdDepthDialogSamplingCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogCustomRadiusCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogCustomRadiusArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogCustomRadiusCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogCustomRadiusCommand>;
public:
    using Storage = SetPsdDepthDialogCustomRadiusArguments;
    SetPsdDepthDialogCustomRadiusCommand();
    SetPsdDepthDialogCustomRadiusCommand(const SetPsdDepthDialogCustomRadiusCommand&) = delete;
public:
    void update(SettingsState& state) override;
    compat::RetainedString validate(SettingsState& state) override;
};
class SetPsdDepthDialogAlphaThresholdCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogAlphaThresholdArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogAlphaThresholdCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogAlphaThresholdCommand>;
public:
    using Storage = SetPsdDepthDialogAlphaThresholdArguments;
    SetPsdDepthDialogAlphaThresholdCommand();
    SetPsdDepthDialogAlphaThresholdCommand(const SetPsdDepthDialogAlphaThresholdCommand&) = delete;
public:
    void update(SettingsState& state) override;
    compat::RetainedString validate(SettingsState& state) override;
};
class SetPsdDepthDialogMissingPolicyCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogMissingPolicyArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogMissingPolicyCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogMissingPolicyCommand>;
public:
    using Storage = SetPsdDepthDialogMissingPolicyArguments;
    SetPsdDepthDialogMissingPolicyCommand();
    SetPsdDepthDialogMissingPolicyCommand(const SetPsdDepthDialogMissingPolicyCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogContourRepairCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogContourRepairArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogContourRepairCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogContourRepairCommand>;
public:
    using Storage = SetPsdDepthDialogContourRepairArguments;
    SetPsdDepthDialogContourRepairCommand();
    SetPsdDepthDialogContourRepairCommand(const SetPsdDepthDialogContourRepairCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogSurfaceSmoothingCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogSurfaceSmoothingArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogSurfaceSmoothingCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogSurfaceSmoothingCommand>;
public:
    using Storage = SetPsdDepthDialogSurfaceSmoothingArguments;
    SetPsdDepthDialogSurfaceSmoothingCommand();
    SetPsdDepthDialogSurfaceSmoothingCommand(const SetPsdDepthDialogSurfaceSmoothingCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class FillPsdDepthDialogAlphaDepthGapsCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<FillPsdDepthDialogAlphaDepthGapsCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit FillPsdDepthDialogAlphaDepthGapsCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<FillPsdDepthDialogAlphaDepthGapsCommand>;
public:
    using Storage = CommandNoArguments;
    FillPsdDepthDialogAlphaDepthGapsCommand();
    FillPsdDepthDialogAlphaDepthGapsCommand(const FillPsdDepthDialogAlphaDepthGapsCommand&) = delete;
    FillPsdDepthDialogAlphaDepthGapsCommand& operator=(
        const FillPsdDepthDialogAlphaDepthGapsCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
class SetPsdDepthDialogGpuCompositionCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogGpuCompositionArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogGpuCompositionCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogGpuCompositionCommand>;
public:
    using Storage = SetPsdDepthDialogGpuCompositionArguments;
    SetPsdDepthDialogGpuCompositionCommand();
    SetPsdDepthDialogGpuCompositionCommand(const SetPsdDepthDialogGpuCompositionCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogDirectGridMatchCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogDirectGridMatchArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogDirectGridMatchCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogDirectGridMatchCommand>;
public:
    using Storage = SetPsdDepthDialogDirectGridMatchArguments;
    SetPsdDepthDialogDirectGridMatchCommand();
    SetPsdDepthDialogDirectGridMatchCommand(const SetPsdDepthDialogDirectGridMatchCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogProblemFilterCommand : public PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogProblemFilterArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::SettingsConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogProblemFilterCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogProblemFilterCommand>;
public:
    using Storage = SetPsdDepthDialogProblemFilterArguments;
    SetPsdDepthDialogProblemFilterCommand();
    SetPsdDepthDialogProblemFilterCommand(const SetPsdDepthDialogProblemFilterCommand&) = delete;
public:
    void update(SettingsState& state) override;
};
class SetPsdDepthDialogLayerMappingAutoCommand : public ExCommand<SetPsdDepthDialogLayerMappingAutoArguments>,
    public std::enable_shared_from_this<SetPsdDepthDialogLayerMappingAutoCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerMappingAutoCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerMappingAutoCommand>;
public:
    using Storage = SetPsdDepthDialogLayerMappingAutoArguments;
    SetPsdDepthDialogLayerMappingAutoCommand();
    SetPsdDepthDialogLayerMappingAutoCommand(const SetPsdDepthDialogLayerMappingAutoCommand&) = delete;
    SetPsdDepthDialogLayerMappingAutoCommand& operator=(
        const SetPsdDepthDialogLayerMappingAutoCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
class SetPsdDepthDialogLayerMappingIgnoredCommand : public ExCommand<SetPsdDepthDialogLayerMappingIgnoredArguments>,
    public std::enable_shared_from_this<SetPsdDepthDialogLayerMappingIgnoredCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerMappingIgnoredCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerMappingIgnoredCommand>;
public:
    using Storage = SetPsdDepthDialogLayerMappingIgnoredArguments;
    SetPsdDepthDialogLayerMappingIgnoredCommand();
    SetPsdDepthDialogLayerMappingIgnoredCommand(const SetPsdDepthDialogLayerMappingIgnoredCommand&) = delete;
    SetPsdDepthDialogLayerMappingIgnoredCommand& operator=(
        const SetPsdDepthDialogLayerMappingIgnoredCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
class SetPsdDepthDialogLayerMappingTargetCommand : public ExCommand<SetPsdDepthDialogLayerMappingTargetArguments>,
    public std::enable_shared_from_this<SetPsdDepthDialogLayerMappingTargetCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerMappingTargetCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerMappingTargetCommand>;
public:
    using Storage = SetPsdDepthDialogLayerMappingTargetArguments;
    SetPsdDepthDialogLayerMappingTargetCommand();
    SetPsdDepthDialogLayerMappingTargetCommand(const SetPsdDepthDialogLayerMappingTargetCommand&) = delete;
    SetPsdDepthDialogLayerMappingTargetCommand& operator=(
        const SetPsdDepthDialogLayerMappingTargetCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
class SetPsdDepthDialogTargetEnabledCommand : public ExCommand<SetPsdDepthDialogTargetEnabledArguments>,
    public std::enable_shared_from_this<SetPsdDepthDialogTargetEnabledCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogTargetEnabledCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogTargetEnabledCommand>;
public:
    using Storage = SetPsdDepthDialogTargetEnabledArguments;
    SetPsdDepthDialogTargetEnabledCommand();
    SetPsdDepthDialogTargetEnabledCommand(const SetPsdDepthDialogTargetEnabledCommand&) = delete;
    SetPsdDepthDialogTargetEnabledCommand& operator=(
        const SetPsdDepthDialogTargetEnabledCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
class SetPsdDepthDialogLayerEnabledCommand : public PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerEnabledArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::LayerConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerEnabledCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerEnabledCommand>;
public:
    using Storage = SetPsdDepthDialogLayerEnabledArguments;
    SetPsdDepthDialogLayerEnabledCommand();
    SetPsdDepthDialogLayerEnabledCommand(const SetPsdDepthDialogLayerEnabledCommand&) = delete;
public:
    void update(LayerState& state) override;
};
class SetPsdDepthDialogLayerVisibleCommand : public PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerVisibleArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::LayerConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerVisibleCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerVisibleCommand>;
public:
    using Storage = SetPsdDepthDialogLayerVisibleArguments;
    SetPsdDepthDialogLayerVisibleCommand();
    SetPsdDepthDialogLayerVisibleCommand(const SetPsdDepthDialogLayerVisibleCommand&) = delete;
public:
    void update(LayerState& state) override;
};
class SetPsdDepthDialogLayerDepthEnabledCommand : public PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthEnabledArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::LayerConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerDepthEnabledCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerDepthEnabledCommand>;
public:
    using Storage = SetPsdDepthDialogLayerDepthEnabledArguments;
    SetPsdDepthDialogLayerDepthEnabledCommand();
    SetPsdDepthDialogLayerDepthEnabledCommand(const SetPsdDepthDialogLayerDepthEnabledCommand&) = delete;
public:
    void update(LayerState& state) override;
};
class SetPsdDepthDialogLayerDepthInvertedCommand : public PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthInvertedArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::LayerConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerDepthInvertedCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerDepthInvertedCommand>;
public:
    using Storage = SetPsdDepthDialogLayerDepthInvertedArguments;
    SetPsdDepthDialogLayerDepthInvertedCommand();
    SetPsdDepthDialogLayerDepthInvertedCommand(const SetPsdDepthDialogLayerDepthInvertedCommand&) = delete;
public:
    void update(LayerState& state) override;
};
class SetPsdDepthDialogLayerDepthScaleCommand : public PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthScaleArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::LayerConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerDepthScaleCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerDepthScaleCommand>;
public:
    using Storage = SetPsdDepthDialogLayerDepthScaleArguments;
    SetPsdDepthDialogLayerDepthScaleCommand();
    SetPsdDepthDialogLayerDepthScaleCommand(const SetPsdDepthDialogLayerDepthScaleCommand&) = delete;
public:
    void update(LayerState& state) override;
};
class SetPsdDepthDialogLayerDepthOffsetCommand : public PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthOffsetArguments> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::LayerConstructorArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit SetPsdDepthDialogLayerDepthOffsetCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetPsdDepthDialogLayerDepthOffsetCommand>;
public:
    using Storage = SetPsdDepthDialogLayerDepthOffsetArguments;
    SetPsdDepthDialogLayerDepthOffsetCommand();
    SetPsdDepthDialogLayerDepthOffsetCommand(const SetPsdDepthDialogLayerDepthOffsetCommand&) = delete;
public:
    void update(LayerState& state) override;
};
class ResetPsdDepthDialogLayerDepthTransformCommand : public ExCommand<ResetPsdDepthDialogLayerDepthTransformArguments>,
    public std::enable_shared_from_this<ResetPsdDepthDialogLayerDepthTransformCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    using SourceConstructorArguments = detail::CommandPairArguments;
    static SourceConstructorArguments sourceConstructorArguments();
    explicit ResetPsdDepthDialogLayerDepthTransformCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ResetPsdDepthDialogLayerDepthTransformCommand>;
public:
    using Storage = ResetPsdDepthDialogLayerDepthTransformArguments;
    ResetPsdDepthDialogLayerDepthTransformCommand();
    ResetPsdDepthDialogLayerDepthTransformCommand(const ResetPsdDepthDialogLayerDepthTransformCommand&) = delete;
    ResetPsdDepthDialogLayerDepthTransformCommand& operator=(
        const ResetPsdDepthDialogLayerDepthTransformCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
class ApplyPsdDepthDialogCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<ApplyPsdDepthDialogCommand> {
    struct SourcePresentation { compat::RetainedString label; compat::RetainedString description; };
    static SourcePresentation sourceConstructorPresentation();
    explicit ApplyPsdDepthDialogCommand(SourcePresentation presentation);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ApplyPsdDepthDialogCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Storage = CommandNoArguments;
    ApplyPsdDepthDialogCommand();
    ApplyPsdDepthDialogCommand(const ApplyPsdDepthDialogCommand&) = delete;
    ApplyPsdDepthDialogCommand& operator=(const ApplyPsdDepthDialogCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
class CancelPsdDepthDialogCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<CancelPsdDepthDialogCommand> {
    struct SourcePresentation { compat::RetainedString label; compat::RetainedString description; };
    static SourcePresentation sourceConstructorPresentation();
    explicit CancelPsdDepthDialogCommand(SourcePresentation presentation);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CancelPsdDepthDialogCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Storage = CommandNoArguments;
    CancelPsdDepthDialogCommand();
    CancelPsdDepthDialogCommand(const CancelPsdDepthDialogCommand&) = delete;
    CancelPsdDepthDialogCommand& operator=(const CancelPsdDepthDialogCommand&) = delete;
    bool runnable(ContextRef ctx) override;
    ResultRef run(ContextRef ctx) override;
};
void ngInitPsdDepthDialogCommands();
template<PsdDepthDialogCommand> struct PsdDepthDialogCommandType;
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::InspectPsdDepthDialog> { using Type = InspectPsdDepthDialogCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::GetPsdDepthDialogPartData> { using Type = GetPsdDepthDialogPartDataCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogColorSource> { using Type = SetPsdDepthDialogColorSourceCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogInvert> { using Type = SetPsdDepthDialogInvertCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogBackDepth> { using Type = SetPsdDepthDialogBackDepthCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogFrontDepth> { using Type = SetPsdDepthDialogFrontDepthCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogDepthScale> { using Type = SetPsdDepthDialogDepthScaleCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogChannel> { using Type = SetPsdDepthDialogChannelCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogSampling> { using Type = SetPsdDepthDialogSamplingCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius> { using Type = SetPsdDepthDialogCustomRadiusCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold> { using Type = SetPsdDepthDialogAlphaThresholdCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogMissingPolicy> { using Type = SetPsdDepthDialogMissingPolicyCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogContourRepair> { using Type = SetPsdDepthDialogContourRepairCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogSurfaceSmoothing> { using Type = SetPsdDepthDialogSurfaceSmoothingCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps> { using Type = FillPsdDepthDialogAlphaDepthGapsCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogGpuComposition> { using Type = SetPsdDepthDialogGpuCompositionCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogDirectGridMatch> { using Type = SetPsdDepthDialogDirectGridMatchCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogProblemFilter> { using Type = SetPsdDepthDialogProblemFilterCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto> { using Type = SetPsdDepthDialogLayerMappingAutoCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored> { using Type = SetPsdDepthDialogLayerMappingIgnoredCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget> { using Type = SetPsdDepthDialogLayerMappingTargetCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogTargetEnabled> { using Type = SetPsdDepthDialogTargetEnabledCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerEnabled> { using Type = SetPsdDepthDialogLayerEnabledCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible> { using Type = SetPsdDepthDialogLayerVisibleCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthEnabled> { using Type = SetPsdDepthDialogLayerDepthEnabledCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthInverted> { using Type = SetPsdDepthDialogLayerDepthInvertedCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale> { using Type = SetPsdDepthDialogLayerDepthScaleCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthOffset> { using Type = SetPsdDepthDialogLayerDepthOffsetCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::ResetPsdDepthDialogLayerDepthTransform> { using Type = ResetPsdDepthDialogLayerDepthTransformCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::ApplyPsdDepthDialog> { using Type = ApplyPsdDepthDialogCommand; };
template<> struct PsdDepthDialogCommandType<PsdDepthDialogCommand::CancelPsdDepthDialog> { using Type = CancelPsdDepthDialogCommand; };
template<PsdDepthDialogCommand id, class... Args>
ResultRef ngRunPsdDepthDialogCommand(ContextRef ctx, Args&&... args) {
    auto found = commands.find(id);
    if (found == commands.end()) throw std::runtime_error("No registered PSD depth dialog command");
    auto base = found->second;
    if (!ngCommandAllowedInCurrentContext(base))
        return CommandResult::opCall(false, "Command is not available in the current command scope");
    using C = typename PsdDepthDialogCommandType<id>::Type;
    auto command = std::dynamic_pointer_cast<C>(base);
    if (!command) throw std::runtime_error("Registered PSD depth dialog command instance type mismatch");
    ngAssignCommandFields(static_cast<typename C::Storage&>(*command), std::forward<Args>(args)...);
    return ngRunCommand(command, ctx);
}
}
namespace nijigenerate::commands {
template<class T, CommandLiteral... Description>
struct CommandTraits<depth::psd_dialog::PsdDepthDialogSettingsValueCommand<T, Description...>> {
    using Storage = typename depth::psd_dialog::PsdDepthDialogSettingsValueCommand<T, Description...>::Storage;
    static constexpr auto attributes = std::tuple{CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<Storage>, Command>;
};
template<class T, CommandLiteral... Description>
struct CommandTraits<depth::psd_dialog::PsdDepthDialogLayerValueCommand<T, Description...>> {
    using Storage = typename depth::psd_dialog::PsdDepthDialogLayerValueCommand<T, Description...>::Storage;
    static constexpr auto attributes = std::tuple{CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<Storage>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::InspectPsdDepthDialogCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::GetPsdDepthDialogPartDataCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogColorSourceCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogColorSourceArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogColorSourceArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogInvertCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogInvertArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogInvertArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogBackDepthCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogBackDepthArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogBackDepthArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogFrontDepthCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogFrontDepthArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogFrontDepthArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogDepthScaleCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogDepthScaleArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogDepthScaleArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogChannelCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogChannelArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogChannelArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogSamplingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogSamplingArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogSamplingArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogCustomRadiusCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogCustomRadiusArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogCustomRadiusArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogAlphaThresholdCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogAlphaThresholdArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogAlphaThresholdArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogMissingPolicyCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogMissingPolicyArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogMissingPolicyArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogContourRepairCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogContourRepairArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogContourRepairArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::FillPsdDepthDialogAlphaDepthGapsCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogGpuCompositionCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogGpuCompositionArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogGpuCompositionArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogDirectGridMatchCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogDirectGridMatchArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogDirectGridMatchArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogProblemFilterCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogSettingsValueCommand<
            depth::psd_dialog::SetPsdDepthDialogProblemFilterArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogProblemFilterArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogTargetEnabledCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<
        ExCommand<depth::psd_dialog::SetPsdDepthDialogTargetEnabledArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerEnabledCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogLayerValueCommand<
            depth::psd_dialog::SetPsdDepthDialogLayerEnabledArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerEnabledArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerVisibleCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogLayerValueCommand<
            depth::psd_dialog::SetPsdDepthDialogLayerVisibleArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerVisibleArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogLayerValueCommand<
            depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogLayerValueCommand<
            depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogLayerValueCommand<
            depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<
        depth::psd_dialog::PsdDepthDialogLayerValueCommand<
            depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetArguments>,
        ExCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<
        ExCommand<depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::ApplyPsdDepthDialogCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>, Command>;
};
template<> struct CommandTraits<depth::psd_dialog::CancelPsdDepthDialogCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<depth::psd_dialog::PsdDepthDialogCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>, Command>;
};
}

namespace nijigenerate::commands::depth::psd_dialog {
template<class T, CommandLiteral... Description>
PsdDepthDialogSettingsValueCommand<T, Description...>::PsdDepthDialogSettingsValueCommand(
        compat::RetainedString label, compat::RetainedString description, compat::RetainedString key,
        compat::RetainedString changeLabel, bool mergeable)
    requires(SourceConstructorProgram::supported())
    : ExCommand<Storage>(label, description) {
    SourceConstructorProgram::initialize(*this, key, changeLabel, mergeable);
}

template<class T, CommandLiteral... Description>
bool PsdDepthDialogSettingsValueCommand<T, Description...>::runnable(ContextRef ctx) {
    const auto keepAlive = this->weak_from_this().lock();
    auto dialog = detail::ngValueCommandDialog(ctx);
    return dialog && detail::ngValueCommandAvailable(dialog);
}

template<class T, CommandLiteral... Description>
ResultRef PsdDepthDialogSettingsValueCommand<T, Description...>::run(ContextRef ctx) {
    const auto keepAlive = this->weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("PSD depth import dialog is not displayed"));
    auto dialog = detail::ngValueCommandDialog(ctx);
    auto oldState = detail::ngValueCommandSettings(dialog);
    auto nextState = oldState;
    update(nextState);
    auto error = validate(nextState);
    if (!error.empty()) return CommandResult::opCall(false, error);
    if (!detail::ngApplyValueCommandSettings(dialog, nextState)) return CommandResult::opCall(true);
    detail::ngPushValueCommandSettings(dialog, oldState, nextState, changeKey, changeLabel, mergeable);
    return CommandResult::opCall(true);
}

template<class T, CommandLiteral... Description>
PsdDepthDialogLayerValueCommand<T, Description...>::PsdDepthDialogLayerValueCommand(
        compat::RetainedString label, compat::RetainedString description, compat::RetainedString key,
        compat::RetainedString changeLabel, bool stageForApply, bool mergeable)
    requires(SourceConstructorProgram::supported())
    : ExCommand<Storage>(label, description) {
    SourceConstructorProgram::initialize(*this, key, changeLabel, stageForApply, mergeable);
}

template<class T, CommandLiteral... Description>
bool PsdDepthDialogLayerValueCommand<T, Description...>::runnable(ContextRef ctx) {
    const auto keepAlive = this->weak_from_this().lock();
    auto dialog = detail::ngValueCommandDialog(ctx);
    LayerState state;
    return dialog && detail::ngValueCommandAvailable(dialog) &&
        detail::ngCaptureValueCommandLayer(dialog, ctx, this->layerPath, state);
}

template<class T, CommandLiteral... Description>
ResultRef PsdDepthDialogLayerValueCommand<T, Description...>::run(ContextRef ctx) {
    const auto keepAlive = this->weak_from_this().lock();
    auto dialog = detail::ngValueCommandDialog(ctx);
    LayerState oldState;
    if (!dialog || !detail::ngValueCommandAvailable(dialog) ||
        !detail::ngCaptureValueCommandLayer(dialog, ctx, this->layerPath, oldState))
        return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("PSD depth dialog layer is not selected in Context"));
    auto nextState = oldState;
    update(nextState);
    if (!detail::ngApplyValueCommandLayer(dialog, nextState, stageForApply)) return CommandResult::opCall(true);
    detail::ngPushValueCommandLayer(dialog, oldState, nextState, changeKey, this->layerPath,
        changeLabel, stageForApply, mergeable);
    return CommandResult::opCall(true);
}
}

namespace nijigenerate::commands {

// All actual zero-argument constructors capture their Source expressions
// inside allocated C construction, then run the same base/own-field programs.
#define NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(Type, ...) \
    template<> struct SourceCommandConstructor<depth::psd_dialog::Type> { \
        template<class... Args> struct Program { \
            using C = depth::psd_dialog::Type; \
            static consteval bool supported() { \
                return sizeof...(Args) == 0 && requires { \
                    { C::sourceConstructorArguments() } -> std::same_as<typename C::SourceConstructorArguments>; \
                } && (__VA_ARGS__); \
            } \
            static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, \
                Args&&... args) requires(supported()) { \
                return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
            } \
        }; \
    };

NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(InspectPsdDepthDialogCommand, SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(GetPsdDepthDialogPartDataCommand, SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogColorSourceCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogColorSourceArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogInvertCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogInvertArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogBackDepthCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogBackDepthArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogFrontDepthCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogFrontDepthArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogDepthScaleCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogDepthScaleArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogChannelCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogChannelArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogSamplingCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogSamplingArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogCustomRadiusCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogCustomRadiusArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogAlphaThresholdCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogAlphaThresholdArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogMissingPolicyCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogMissingPolicyArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogContourRepairCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogContourRepairArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogSurfaceSmoothingCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(FillPsdDepthDialogAlphaDepthGapsCommand, SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogGpuCompositionCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogGpuCompositionArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogDirectGridMatchCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogDirectGridMatchArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogProblemFilterCommand, depth::psd_dialog::PsdDepthDialogSettingsValueCommand<depth::psd_dialog::SetPsdDepthDialogProblemFilterArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerMappingAutoCommand, SourceExCommandConstructorProgram<depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoArguments>::pairSupported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerMappingIgnoredCommand, SourceExCommandConstructorProgram<depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredArguments>::pairSupported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerMappingTargetCommand, SourceExCommandConstructorProgram<depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetArguments>::pairSupported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogTargetEnabledCommand, SourceExCommandConstructorProgram<depth::psd_dialog::SetPsdDepthDialogTargetEnabledArguments>::pairSupported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerEnabledCommand, depth::psd_dialog::PsdDepthDialogLayerValueCommand<depth::psd_dialog::SetPsdDepthDialogLayerEnabledArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerVisibleCommand, depth::psd_dialog::PsdDepthDialogLayerValueCommand<depth::psd_dialog::SetPsdDepthDialogLayerVisibleArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerDepthEnabledCommand, depth::psd_dialog::PsdDepthDialogLayerValueCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerDepthInvertedCommand, depth::psd_dialog::PsdDepthDialogLayerValueCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerDepthScaleCommand, depth::psd_dialog::PsdDepthDialogLayerValueCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(SetPsdDepthDialogLayerDepthOffsetCommand, depth::psd_dialog::PsdDepthDialogLayerValueCommand<depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetArguments>::SourceConstructorProgram::supported())
NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM(ResetPsdDepthDialogLayerDepthTransformCommand, SourceExCommandConstructorProgram<depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformArguments>::pairSupported())

#undef NG_PSD_DIALOG_CONSTRUCTOR_PROGRAM

template<> struct SourceCommandConstructor<depth::psd_dialog::ApplyPsdDepthDialogCommand> {
    template<class... Args> struct Program {
        using C = depth::psd_dialog::ApplyPsdDepthDialogCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorPresentation() } -> std::same_as<typename C::SourcePresentation>;
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<depth::psd_dialog::CancelPsdDepthDialogCommand> {
    template<class... Args> struct Program {
        using C = depth::psd_dialog::CancelPsdDepthDialogCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorPresentation() } -> std::same_as<typename C::SourcePresentation>;
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::depth::psd_dialog::initialization {
struct Table {
    using map_type = CommandTable;
    static map_type& get() { return commands; }
};

template<auto Id, class C>
using Required = ::nijigenerate::commands::detail::DefaultCommandRegistration<Id, C>;
// Source EnumMembers order; the production entry and semantic predicates share
// these exact construction, metadata and current-TLS assignment operations.
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Required<PsdDepthDialogCommand::InspectPsdDepthDialog, InspectPsdDepthDialogCommand>,
    Required<PsdDepthDialogCommand::GetPsdDepthDialogPartData, GetPsdDepthDialogPartDataCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogColorSource, SetPsdDepthDialogColorSourceCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogInvert, SetPsdDepthDialogInvertCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogBackDepth, SetPsdDepthDialogBackDepthCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogFrontDepth, SetPsdDepthDialogFrontDepthCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogDepthScale, SetPsdDepthDialogDepthScaleCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogChannel, SetPsdDepthDialogChannelCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogSampling, SetPsdDepthDialogSamplingCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius, SetPsdDepthDialogCustomRadiusCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold, SetPsdDepthDialogAlphaThresholdCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogMissingPolicy, SetPsdDepthDialogMissingPolicyCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogContourRepair, SetPsdDepthDialogContourRepairCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogSurfaceSmoothing, SetPsdDepthDialogSurfaceSmoothingCommand>,
    Required<PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps, FillPsdDepthDialogAlphaDepthGapsCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogGpuComposition, SetPsdDepthDialogGpuCompositionCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogDirectGridMatch, SetPsdDepthDialogDirectGridMatchCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogProblemFilter, SetPsdDepthDialogProblemFilterCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto, SetPsdDepthDialogLayerMappingAutoCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored, SetPsdDepthDialogLayerMappingIgnoredCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget, SetPsdDepthDialogLayerMappingTargetCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogTargetEnabled, SetPsdDepthDialogTargetEnabledCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerEnabled, SetPsdDepthDialogLayerEnabledCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible, SetPsdDepthDialogLayerVisibleCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthEnabled, SetPsdDepthDialogLayerDepthEnabledCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthInverted, SetPsdDepthDialogLayerDepthInvertedCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale, SetPsdDepthDialogLayerDepthScaleCommand>,
    Required<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthOffset, SetPsdDepthDialogLayerDepthOffsetCommand>,
    Required<PsdDepthDialogCommand::ResetPsdDepthDialogLayerDepthTransform, ResetPsdDepthDialogLayerDepthTransformCommand>,
    Required<PsdDepthDialogCommand::ApplyPsdDepthDialog, ApplyPsdDepthDialogCommand>,
    Required<PsdDepthDialogCommand::CancelPsdDepthDialog, CancelPsdDepthDialogCommand>>;
}

namespace nijigenerate::commands::depth::psd_dialog {
template<class T> requires (std::same_as<T, PsdDepthDialogCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitPsdDepthDialogCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<depth::psd_dialog::PsdDepthDialogCommand> {
    static consteval bool supported() {
        return depth::psd_dialog::initialization::Program::supported<
            depth::psd_dialog::initialization::Table>();
    }
    template<class ActualKey = depth::psd_dialog::PsdDepthDialogCommand>
        requires (std::same_as<ActualKey, depth::psd_dialog::PsdDepthDialogCommand> && supported())
    static void invoke() { depth::psd_dialog::ngInitCommands<ActualKey>(); }
};
}


namespace nicxlive::core::d_runtime {
#define NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::depth::psd_dialog::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.depth.psd_dialog." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.depth.psd_dialog." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::psd_dialog::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.psd_dialog." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.psd_dialog." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
        NG_SOURCE_TYPE_DIAGNOSTIC_MEMBERS(#Type) \
    };
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(InspectPsdDepthDialogCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(GetPsdDepthDialogPartDataCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogColorSourceCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogInvertCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogBackDepthCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogFrontDepthCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogDepthScaleCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogChannelCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogSamplingCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogCustomRadiusCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogAlphaThresholdCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogMissingPolicyCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogContourRepairCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogSurfaceSmoothingCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogGpuCompositionCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogDirectGridMatchCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogProblemFilterCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerMappingAutoCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerMappingIgnoredCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerMappingTargetCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogTargetEnabledCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerEnabledCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerVisibleCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerDepthEnabledCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerDepthInvertedCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerDepthScaleCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(SetPsdDepthDialogLayerDepthOffsetCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(ResetPsdDepthDialogLayerDepthTransformCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(FillPsdDepthDialogAlphaDepthGapsCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(ApplyPsdDepthDialogCommand)
NG_PSD_DIALOG_COMMAND_SOURCE_NAMES(CancelPsdDepthDialogCommand)
#undef NG_PSD_DIALOG_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::depth::psd_dialog::detail {

template<class Storage, std::size_t Index> struct ValueCommandSourceField {
    using Fields = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    using Field = std::tuple_element_t<Index, Fields>;
    using Value = typename Field::Type;
    static constexpr auto description = Field::fieldDesc;
};

template<class T, CommandLiteral Description> struct ValueCommandSourceParameters {
    using Value = T;
    static constexpr auto description = Description.view();
};

template<class T, CommandLiteral... Description> struct SettingsValueSourceParameters;
template<class Storage> struct SettingsValueSourceParameters<Storage> : ValueCommandSourceField<Storage, 0> {};
template<class T, CommandLiteral Description>
struct SettingsValueSourceParameters<T, Description> : ValueCommandSourceParameters<T, Description> {};

template<class T, CommandLiteral... Description> struct LayerValueSourceParameters;
template<class Storage> struct LayerValueSourceParameters<Storage> : ValueCommandSourceField<Storage, 1> {};
template<class T, CommandLiteral Description>
struct LayerValueSourceParameters<T, Description> : ValueCommandSourceParameters<T, Description> {};

template<class Parameters> compat::RetainedString valueCommandSourceArguments(bool qualified) {
    const auto type = ::nijigenerate::commands::detail::
        commandResultSourceElement<typename Parameters::Value>(qualified);
    const auto description = nicxlive::core::d_runtime::ngSourceStringValueArgument(Parameters::description);
    try {
        std::string arguments = "(";
        arguments.append(type.view());
        arguments += ", ";
        arguments.append(description.view());
        arguments += ')';
        return compat::RetainedString(std::move(arguments));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Parameters> compat::RetainedString valueCommandSourceClassName(std::string_view declaration) {
    const auto arguments = valueCommandSourceArguments<Parameters>(true);
    return ::nijigenerate::commands::detail::composeCommandTemplateName(
        "nijigenerate.commands.depth.psd_dialog", declaration, arguments, true);
}

template<class Parameters>
nicxlive::core::d_runtime::SourceTypeSpellingNames valueCommandSourceSpellings(std::string_view declaration) {
    const auto arguments = valueCommandSourceArguments<Parameters>(false);
    const auto qualified = ::nijigenerate::commands::detail::composeCommandTemplateName(
        "nijigenerate.commands.depth.psd_dialog", declaration, arguments, false);
    const auto unqualified = ::nijigenerate::commands::detail::
        composeCommandTemplateName({}, declaration, arguments, false);
    return {::nijigenerate::commands::detail::commandTypeArgumentGroup(qualified),
        ::nijigenerate::commands::detail::commandTypeArgumentGroup(unqualified), qualified, unqualified};
}

template<class List> struct ValueCommandSourceBase;
template<class First, class... Rest> struct ValueCommandSourceBase<CommandTypeList<First, Rest...>> {
    using Type = First;
};

} // namespace nijigenerate::commands::depth::psd_dialog::detail

namespace nicxlive::core::d_runtime {

template<class T, ::nijigenerate::commands::CommandLiteral... Description>
struct SourceObjectTypeNames<
        ::nijigenerate::commands::depth::psd_dialog::PsdDepthDialogSettingsValueCommand<T, Description...>> {
    using Parameters = ::nijigenerate::commands::depth::psd_dialog::detail::
        SettingsValueSourceParameters<T, Description...>;
    static RetainedString className() {
        return ::nijigenerate::commands::depth::psd_dialog::detail::
            valueCommandSourceClassName<Parameters>("PsdDepthDialogSettingsValueCommand");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::commands::depth::psd_dialog::detail::
            valueCommandSourceSpellings<Parameters>("PsdDepthDialogSettingsValueCommand").qualifiedArgument;
    }
};

template<class T, ::nijigenerate::commands::CommandLiteral... Description>
struct SourceTypeSpellings<
        ::nijigenerate::commands::depth::psd_dialog::PsdDepthDialogSettingsValueCommand<T, Description...>> {
    inline static constexpr bool isNativeObject = true;
    using Parameters = ::nijigenerate::commands::depth::psd_dialog::detail::
        SettingsValueSourceParameters<T, Description...>;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::commands::depth::psd_dialog::detail::
            valueCommandSourceSpellings<Parameters>("PsdDepthDialogSettingsValueCommand");
    }
};

template<class T, ::nijigenerate::commands::CommandLiteral... Description>
struct SourceObjectTypeNames<
        ::nijigenerate::commands::depth::psd_dialog::PsdDepthDialogLayerValueCommand<T, Description...>> {
    using Parameters = ::nijigenerate::commands::depth::psd_dialog::detail::
        LayerValueSourceParameters<T, Description...>;
    static RetainedString className() {
        return ::nijigenerate::commands::depth::psd_dialog::detail::
            valueCommandSourceClassName<Parameters>("PsdDepthDialogLayerValueCommand");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::commands::depth::psd_dialog::detail::
            valueCommandSourceSpellings<Parameters>("PsdDepthDialogLayerValueCommand").qualifiedArgument;
    }
};

template<class T, ::nijigenerate::commands::CommandLiteral... Description>
struct SourceTypeSpellings<
        ::nijigenerate::commands::depth::psd_dialog::PsdDepthDialogLayerValueCommand<T, Description...>> {
    inline static constexpr bool isNativeObject = true;
    using Parameters = ::nijigenerate::commands::depth::psd_dialog::detail::
        LayerValueSourceParameters<T, Description...>;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::commands::depth::psd_dialog::detail::
            valueCommandSourceSpellings<Parameters>("PsdDepthDialogLayerValueCommand");
    }
};

} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::depth::psd_dialog {

template<class T, CommandLiteral... Description>
void PsdDepthDialogSettingsValueCommand<T, Description...>::prepareSourceClassInfo() {
    ExCommand<Storage>::prepareSourceClassInfo();
    using Self = PsdDepthDialogSettingsValueCommand<T, Description...>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

template<class T, CommandLiteral... Description>
void PsdDepthDialogLayerValueCommand<T, Description...>::prepareSourceClassInfo() {
    ExCommand<Storage>::prepareSourceClassInfo();
    using Self = PsdDepthDialogLayerValueCommand<T, Description...>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

} // namespace nijigenerate::commands::depth::psd_dialog
