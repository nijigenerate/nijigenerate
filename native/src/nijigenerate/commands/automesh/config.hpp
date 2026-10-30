#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include "config_tables.hpp"
#include "detail/config_snapshot.hpp"
#include "detail/config_fields.hpp"
#include <concepts>
#include <utility>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta.hpp>
#include <nijigenerate/viewport/vertex/automesh/optimum.hpp>
#include <nijigenerate/viewport/vertex/automesh/contours.hpp>
#include <nijigenerate/viewport/vertex/automesh/grid.hpp>
#include <nijigenerate/viewport/vertex/automesh/skeletonize.hpp>
namespace nijigenerate::commands::automesh::config {
using AutoMeshConfigSnapshot = detail::AutoMeshConfigSnapshot;
namespace detail {
template<class C, class Storage, class... Parameters> struct ConfigConstructorProgram;
struct ConfigCommandText { compat::RetainedString label; compat::RetainedString description; };
ConfigCommandText formattedCommandText(std::string_view pattern, std::string_view processorName,
    std::string_view description);
compat::Slice<std::shared_ptr<viewport::vertex::automesh::AutoMeshProcessor>> configProcessors();
std::shared_ptr<CommandResult> runGetConfig(
    std::shared_ptr<viewport::vertex::automesh::AutoMeshProcessor> processor, const compat::RetainedString& level);
std::shared_ptr<CommandResult> runSetConfig(
    std::shared_ptr<viewport::vertex::automesh::AutoMeshProcessor> processor,
    const compat::RetainedString& level, const compat::RetainedString& updates);
std::shared_ptr<CommandResult> runPresetConfig(
    std::shared_ptr<viewport::vertex::automesh::AutoMeshProcessor> processor, const compat::RetainedString& preset);
template<class PT> std::shared_ptr<viewport::vertex::automesh::AutoMeshProcessor> resolveConfigProcessor() {
    for (auto processor : configProcessors())
        if (std::dynamic_pointer_cast<PT>(processor)) return processor;
    return {};
}
template<class PT> struct GetConfigArguments { compat::RetainedString level{}; };
template<class PT> struct SetConfigArguments {
    compat::RetainedString level{};
    compat::RetainedString updates{};
};
template<class PT> struct PresetConfigArguments { compat::RetainedString preset{}; };
compat::RetainedString configMixinName(std::string_view declaration, std::string_view child,
    compat::RetainedString argument);
nicxlive::core::d_runtime::SourceTypeSpellingNames configMixinTypeSpellings(
    std::string_view declaration, std::string_view child, compat::RetainedString argument);
nicxlive::core::d_runtime::SourceTypeSpellingNames configMixinRetainedTypeSpellings(
    std::string_view declaration, compat::RetainedString child, compat::RetainedString argument);
}
class AutoMeshConfigChangeAction : public actions::Action,
    public std::enable_shared_from_this<AutoMeshConfigChangeAction> {
    std::shared_ptr<viewport::vertex::automesh::AutoMeshProcessor> processor;
    compat::RetainedString processorName;
    AutoMeshConfigSnapshot oldState, newState;
public:
    AutoMeshConfigChangeAction(std::shared_ptr<viewport::vertex::automesh::AutoMeshProcessor>,
        AutoMeshConfigSnapshot, AutoMeshConfigSnapshot);
    AutoMeshConfigChangeAction(const AutoMeshConfigChangeAction&) = delete;
    AutoMeshConfigChangeAction& operator=(const AutoMeshConfigChangeAction&) = delete;
    void rollback() override;
    void redo() override;
    actions::ActionText describe() override;
    actions::ActionText describeUndo() override;
    actions::ActionText getName() override;
    bool merge(actions::ActionPtr) override {
        const auto receiver = weak_from_this().lock();
        return false;
    }
    bool canMerge(actions::ActionPtr) override {
        const auto receiver = weak_from_this().lock();
        return false;
    }
    virtual compat::RetainedString toString();
};
struct AutoMeshSetActiveCommandArguments {
    compat::RetainedString processorId{};
};
struct AutoMeshGetSchemaCommandArguments {
    compat::RetainedString processorId{};
};
struct AutoMeshGetValuesCommandArguments {
    compat::RetainedString processorId{};
    compat::RetainedString level{};
};
struct AutoMeshSetPresetCommandArguments {
    compat::RetainedString processorId{};
    compat::RetainedString preset{};
};
struct AutoMeshSetValuesCommandArguments {
    compat::RetainedString processorId{};
    compat::RetainedString level{};
    compat::RetainedString updates{};
};
struct GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments {
    compat::RetainedString level{};
};
struct SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments {
    compat::RetainedString level{};
    compat::RetainedString updates{};
};
struct AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments {
    compat::Slice<float> scales{};
    float min_distance = std::numeric_limits<float>::quiet_NaN();
    float mask_threshold = std::numeric_limits<float>::quiet_NaN();
    float div_per_part = std::numeric_limits<float>::quiet_NaN();
};
struct AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments {
    float large_threshold = std::numeric_limits<float>::quiet_NaN();
    float length_threshold = std::numeric_limits<float>::quiet_NaN();
    float ratio_threshold = std::numeric_limits<float>::quiet_NaN();
    float sharp_expand = std::numeric_limits<float>::quiet_NaN();
    float unsharp_expand = std::numeric_limits<float>::quiet_NaN();
    float unsharp_contract = std::numeric_limits<float>::quiet_NaN();
};
struct AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments {
    compat::RetainedString preset{};
};
struct GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments {
    compat::RetainedString level{};
};
struct SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments {
    compat::RetainedString level{};
    compat::RetainedString updates{};
};
struct AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments {
    float sampling_step = std::numeric_limits<float>::quiet_NaN();
    float mask_threshold = std::numeric_limits<float>::quiet_NaN();
};
struct AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments {
    float min_distance = std::numeric_limits<float>::quiet_NaN();
    float max_distance = std::numeric_limits<float>::quiet_NaN();
    compat::Slice<float> scales{};
};
struct AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments {
    compat::RetainedString preset{};
};
struct GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments {
    compat::RetainedString level{};
};
struct SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments {
    compat::RetainedString level{};
    compat::RetainedString updates{};
};
struct AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments {
    float mask_threshold = std::numeric_limits<float>::quiet_NaN();
    float x_segments = std::numeric_limits<float>::quiet_NaN();
    float y_segments = std::numeric_limits<float>::quiet_NaN();
    float margin = std::numeric_limits<float>::quiet_NaN();
};
struct AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments {
    compat::Slice<float> scale_x{};
    compat::Slice<float> scale_y{};
};
struct AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments {
    compat::RetainedString preset{};
};
struct GetAutoMeshConfig_SkeletonExtractorCommandArguments {
    compat::RetainedString level{};
};
struct SetAutoMeshConfig_SkeletonExtractorCommandArguments {
    compat::RetainedString level{};
    compat::RetainedString updates{};
};
struct AutoMeshSetSimple_SkeletonExtractorCommandArguments {
    float mask_threshold = std::numeric_limits<float>::quiet_NaN();
    float target_point_count = std::numeric_limits<float>::quiet_NaN();
};
struct AutoMeshSetPreset_SkeletonExtractorCommandArguments {
    compat::RetainedString preset{};
};
}
namespace nijigenerate::commands {
template<> struct CommandFields<automesh::config::AutoMeshSetActiveCommandArguments> {
    using A=automesh::config::AutoMeshSetActiveCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::processorId,"processorId","AutoMesh processor identifier","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshGetSchemaCommandArguments> {
    using A=automesh::config::AutoMeshGetSchemaCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::processorId,"processorId","AutoMesh processor identifier","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshGetValuesCommandArguments> {
    using A=automesh::config::AutoMeshGetValuesCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::processorId,"processorId","AutoMesh processor identifier","string">{},
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshSetPresetCommandArguments> {
    using A=automesh::config::AutoMeshSetPresetCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::processorId,"processorId","AutoMesh processor identifier","string">{},
        FieldDescriptor<&A::preset,"preset","Preset name","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshSetValuesCommandArguments> {
    using A=automesh::config::AutoMeshSetValuesCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::processorId,"processorId","AutoMesh processor identifier","string">{},
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{},
        FieldDescriptor<&A::updates,"updates","JSON object of updates","string">{}
    };
};
template<> struct CommandFields<automesh::config::GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments> {
    using A=automesh::config::GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{}
    };
};
template<> struct CommandFields<automesh::config::SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments> {
    using A=automesh::config::SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{},
        FieldDescriptor<&A::updates,"updates","JSON object of updates","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=automesh::config::detail::configSourceFields<
        viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor,
        viewport::vertex::automesh::meta::AutoMeshLevel::Simple,
        &A::scales,&A::min_distance,&A::mask_threshold,&A::div_per_part>();
};
template<> struct CommandFields<automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=automesh::config::detail::configSourceFields<
        viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor,
        viewport::vertex::automesh::meta::AutoMeshLevel::Advanced,
        &A::large_threshold,&A::length_threshold,&A::ratio_threshold,
        &A::sharp_expand,&A::unsharp_expand,&A::unsharp_contract>();
};
template<> struct CommandFields<automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::preset,"preset","Preset name","string">{}
    };
};
template<> struct CommandFields<automesh::config::GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments> {
    using A=automesh::config::GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{}
    };
};
template<> struct CommandFields<automesh::config::SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments> {
    using A=automesh::config::SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{},
        FieldDescriptor<&A::updates,"updates","JSON object of updates","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=automesh::config::detail::configSourceFields<
        viewport::vertex::automesh::contours::ContourAutoMeshProcessor,
        viewport::vertex::automesh::meta::AutoMeshLevel::Simple,
        &A::sampling_step,&A::mask_threshold>();
};
template<> struct CommandFields<automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=automesh::config::detail::configSourceFields<
        viewport::vertex::automesh::contours::ContourAutoMeshProcessor,
        viewport::vertex::automesh::meta::AutoMeshLevel::Advanced,
        &A::min_distance,&A::max_distance,&A::scales>();
};
template<> struct CommandFields<automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::preset,"preset","Preset name","string">{}
    };
};
template<> struct CommandFields<automesh::config::GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments> {
    using A=automesh::config::GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{}
    };
};
template<> struct CommandFields<automesh::config::SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments> {
    using A=automesh::config::SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{},
        FieldDescriptor<&A::updates,"updates","JSON object of updates","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=automesh::config::detail::configSourceFields<
        viewport::vertex::automesh::grid::GridAutoMeshProcessor,
        viewport::vertex::automesh::meta::AutoMeshLevel::Simple,
        &A::mask_threshold,&A::x_segments,&A::y_segments,&A::margin>();
};
template<> struct CommandFields<automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=automesh::config::detail::configSourceFields<
        viewport::vertex::automesh::grid::GridAutoMeshProcessor,
        viewport::vertex::automesh::meta::AutoMeshLevel::Advanced,
        &A::scale_x,&A::scale_y>();
};
template<> struct CommandFields<automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments> {
    using A=automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::preset,"preset","Preset name","string">{}
    };
};
template<> struct CommandFields<automesh::config::GetAutoMeshConfig_SkeletonExtractorCommandArguments> {
    using A=automesh::config::GetAutoMeshConfig_SkeletonExtractorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{}
    };
};
template<> struct CommandFields<automesh::config::SetAutoMeshConfig_SkeletonExtractorCommandArguments> {
    using A=automesh::config::SetAutoMeshConfig_SkeletonExtractorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{},
        FieldDescriptor<&A::updates,"updates","JSON object of updates","string">{}
    };
};
template<> struct CommandFields<automesh::config::AutoMeshSetSimple_SkeletonExtractorCommandArguments> {
    using A=automesh::config::AutoMeshSetSimple_SkeletonExtractorCommandArguments;
    static constexpr auto descriptors=automesh::config::detail::configSourceFields<
        viewport::vertex::automesh::skeletonize::SkeletonExtractor,
        viewport::vertex::automesh::meta::AutoMeshLevel::Simple,
        &A::mask_threshold,&A::target_point_count>();
};
template<> struct CommandFields<automesh::config::AutoMeshSetPreset_SkeletonExtractorCommandArguments> {
    using A=automesh::config::AutoMeshSetPreset_SkeletonExtractorCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::preset,"preset","Preset name","string">{}
    };
};
}
namespace nijigenerate::commands::automesh::config {
class AutoMeshListProcessorsCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<AutoMeshListProcessorsCommand> {
    AutoMeshListProcessorsCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshListProcessorsCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshGetActiveCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<AutoMeshGetActiveCommand> {
    AutoMeshGetActiveCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshGetActiveCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetActiveCommand : public ExCommand<AutoMeshSetActiveCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetActiveCommand> {
    AutoMeshSetActiveCommand(detail::ConfigCommandText text, compat::RetainedString processorId);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetActiveCommand(compat::RetainedString processorId);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshApplyActiveCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<AutoMeshApplyActiveCommand> {
    AutoMeshApplyActiveCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshApplyActiveCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshGetSchemaCommand : public ExCommand<AutoMeshGetSchemaCommandArguments>,
    public std::enable_shared_from_this<AutoMeshGetSchemaCommand> {
    AutoMeshGetSchemaCommand(detail::ConfigCommandText text, compat::RetainedString processorId);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshGetSchemaCommand(compat::RetainedString processorId);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshGetValuesCommand : public ExCommand<AutoMeshGetValuesCommandArguments>,
    public std::enable_shared_from_this<AutoMeshGetValuesCommand> {
    AutoMeshGetValuesCommand(detail::ConfigCommandText text, compat::RetainedString processorId, compat::RetainedString level);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshGetValuesCommand(compat::RetainedString processorId, compat::RetainedString level);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetPresetCommand : public ExCommand<AutoMeshSetPresetCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetPresetCommand> {
    AutoMeshSetPresetCommand(detail::ConfigCommandText text, compat::RetainedString processorId, compat::RetainedString preset);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetPresetCommand(compat::RetainedString processorId, compat::RetainedString preset);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetValuesCommand : public ExCommand<AutoMeshSetValuesCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetValuesCommand> {
    AutoMeshSetValuesCommand(detail::ConfigCommandText text, compat::RetainedString processorId, compat::RetainedString level, compat::RetainedString updates);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetValuesCommand(compat::RetainedString processorId, compat::RetainedString level, compat::RetainedString updates);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class GetAutoMeshConfig_OptimumAutoMeshProcessorCommand : public ExCommand<GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<GetAutoMeshConfig_OptimumAutoMeshProcessorCommand> {
    GetAutoMeshConfig_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    GetAutoMeshConfig_OptimumAutoMeshProcessorCommand(compat::RetainedString level);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class SetAutoMeshConfig_OptimumAutoMeshProcessorCommand : public ExCommand<SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<SetAutoMeshConfig_OptimumAutoMeshProcessorCommand> {
    SetAutoMeshConfig_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    SetAutoMeshConfig_OptimumAutoMeshProcessorCommand(compat::RetainedString level, compat::RetainedString updates);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetSimple_OptimumAutoMeshProcessorCommand : public ExCommand<AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetSimple_OptimumAutoMeshProcessorCommand> {
    AutoMeshSetSimple_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetSimple_OptimumAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand : public ExCommand<AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand> {
    AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetPreset_OptimumAutoMeshProcessorCommand : public ExCommand<AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetPreset_OptimumAutoMeshProcessorCommand> {
    AutoMeshSetPreset_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetPreset_OptimumAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class GetAutoMeshConfig_ContourAutoMeshProcessorCommand : public ExCommand<GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<GetAutoMeshConfig_ContourAutoMeshProcessorCommand> {
    GetAutoMeshConfig_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    GetAutoMeshConfig_ContourAutoMeshProcessorCommand(compat::RetainedString level);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class SetAutoMeshConfig_ContourAutoMeshProcessorCommand : public ExCommand<SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<SetAutoMeshConfig_ContourAutoMeshProcessorCommand> {
    SetAutoMeshConfig_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    SetAutoMeshConfig_ContourAutoMeshProcessorCommand(compat::RetainedString level, compat::RetainedString updates);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetSimple_ContourAutoMeshProcessorCommand : public ExCommand<AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetSimple_ContourAutoMeshProcessorCommand> {
    AutoMeshSetSimple_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetSimple_ContourAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand : public ExCommand<AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand> {
    AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetPreset_ContourAutoMeshProcessorCommand : public ExCommand<AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetPreset_ContourAutoMeshProcessorCommand> {
    AutoMeshSetPreset_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetPreset_ContourAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class GetAutoMeshConfig_GridAutoMeshProcessorCommand : public ExCommand<GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<GetAutoMeshConfig_GridAutoMeshProcessorCommand> {
    GetAutoMeshConfig_GridAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    GetAutoMeshConfig_GridAutoMeshProcessorCommand(compat::RetainedString level);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class SetAutoMeshConfig_GridAutoMeshProcessorCommand : public ExCommand<SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<SetAutoMeshConfig_GridAutoMeshProcessorCommand> {
    SetAutoMeshConfig_GridAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    SetAutoMeshConfig_GridAutoMeshProcessorCommand(compat::RetainedString level, compat::RetainedString updates);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetSimple_GridAutoMeshProcessorCommand : public ExCommand<AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetSimple_GridAutoMeshProcessorCommand> {
    AutoMeshSetSimple_GridAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetSimple_GridAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetAdvanced_GridAutoMeshProcessorCommand : public ExCommand<AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetAdvanced_GridAutoMeshProcessorCommand> {
    AutoMeshSetAdvanced_GridAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetAdvanced_GridAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetPreset_GridAutoMeshProcessorCommand : public ExCommand<AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetPreset_GridAutoMeshProcessorCommand> {
    AutoMeshSetPreset_GridAutoMeshProcessorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetPreset_GridAutoMeshProcessorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class GetAutoMeshConfig_SkeletonExtractorCommand : public ExCommand<GetAutoMeshConfig_SkeletonExtractorCommandArguments>,
    public std::enable_shared_from_this<GetAutoMeshConfig_SkeletonExtractorCommand> {
    GetAutoMeshConfig_SkeletonExtractorCommand(detail::ConfigCommandText text, compat::RetainedString level);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    GetAutoMeshConfig_SkeletonExtractorCommand(compat::RetainedString level);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class SetAutoMeshConfig_SkeletonExtractorCommand : public ExCommand<SetAutoMeshConfig_SkeletonExtractorCommandArguments>,
    public std::enable_shared_from_this<SetAutoMeshConfig_SkeletonExtractorCommand> {
    SetAutoMeshConfig_SkeletonExtractorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    SetAutoMeshConfig_SkeletonExtractorCommand(compat::RetainedString level, compat::RetainedString updates);
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetSimple_SkeletonExtractorCommand : public ExCommand<AutoMeshSetSimple_SkeletonExtractorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetSimple_SkeletonExtractorCommand> {
    AutoMeshSetSimple_SkeletonExtractorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetSimple_SkeletonExtractorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetAdvanced_SkeletonExtractorCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<AutoMeshSetAdvanced_SkeletonExtractorCommand> {
    AutoMeshSetAdvanced_SkeletonExtractorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetAdvanced_SkeletonExtractorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AutoMeshSetPreset_SkeletonExtractorCommand : public ExCommand<AutoMeshSetPreset_SkeletonExtractorCommandArguments>,
    public std::enable_shared_from_this<AutoMeshSetPreset_SkeletonExtractorCommand> {
    AutoMeshSetPreset_SkeletonExtractorCommand(detail::ConfigCommandText text);
    static detail::ConfigCommandText sourceConstructorText();
    template<class, class, class...> friend struct detail::ConfigConstructorProgram;
protected:
    void prepareSourceClassInfo() override;
public:
    AutoMeshSetPreset_SkeletonExtractorCommand();
    bool runnable(std::shared_ptr<Context>) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
std::shared_ptr<Command> ensureGetAutoMeshConfigCommand(compat::RetainedString id);
std::shared_ptr<Command> ensureSetAutoMeshConfigCommand(compat::RetainedString id);
template<class PT> class GetConfigCommand : public ExCommand<detail::GetConfigArguments<PT>>,
    public std::enable_shared_from_this<GetConfigCommand<PT>> {
    using Base=ExCommand<detail::GetConfigArguments<PT>>;
    GetConfigCommand(detail::ConfigCommandText text, compat::RetainedString level)
        : Base(std::move(text.label),std::move(text.description),level) {}
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::commands::detail::prepareCommandResultClass<GetConfigCommand>();
    }
public:
    explicit GetConfigCommand(compat::RetainedString level)
        : GetConfigCommand(detail::formattedCommandText("Get Config (%s)",
            viewport::vertex::automesh::meta::AMProcInfo<PT>::name,
            "Get AutoMesh config (per processor)"),level) {}
    bool runnable(std::shared_ptr<Context>) override {
        const auto receiver=this->weak_from_this().lock();
        return true;
    }
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override {
        const auto receiver=this->weak_from_this().lock();
        auto processor=detail::resolveConfigProcessor<PT>();
        return detail::runGetConfig(processor,this->level);
    }
};
template<class PT> class SetConfigCommand : public ExCommand<detail::SetConfigArguments<PT>>,
    public std::enable_shared_from_this<SetConfigCommand<PT>> {
    using Base=ExCommand<detail::SetConfigArguments<PT>>;
    SetConfigCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates)
        : Base(std::move(text.label),std::move(text.description),level,updates) {}
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::commands::detail::prepareCommandResultClass<SetConfigCommand>();
    }
public:
    SetConfigCommand(compat::RetainedString level, compat::RetainedString updates)
        : SetConfigCommand(detail::formattedCommandText("Set Config (%s)",
            viewport::vertex::automesh::meta::AMProcInfo<PT>::name,
            "Set AutoMesh config (per processor)"),level,updates) {}
    bool runnable(std::shared_ptr<Context>) override {
        const auto receiver=this->weak_from_this().lock();
        return !this->updates.empty();
    }
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override {
        const auto receiver=this->weak_from_this().lock();
        auto processor=detail::resolveConfigProcessor<PT>();
        return detail::runSetConfig(processor,this->level,this->updates);
    }
};
template<class PT> class PresetConfigCommand : public ExCommand<detail::PresetConfigArguments<PT>>,
    public std::enable_shared_from_this<PresetConfigCommand<PT>> {
    using Base=ExCommand<detail::PresetConfigArguments<PT>>;
    PresetConfigCommand(detail::ConfigCommandText text)
        : Base(std::move(text.label),std::move(text.description)) {}
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::commands::detail::prepareCommandResultClass<PresetConfigCommand>();
    }
public:
    PresetConfigCommand()
        : PresetConfigCommand(detail::formattedCommandText("Set Preset (%s)",
            viewport::vertex::automesh::meta::AMProcInfo<PT>::name,"Apply preset")) {}
    bool runnable(std::shared_ptr<Context>) override {
        const auto receiver=this->weak_from_this().lock();
        return true;
    }
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override {
        const auto receiver=this->weak_from_this().lock();
        auto processor=detail::resolveConfigProcessor<PT>();
        return detail::runPresetConfig(processor,this->preset);
    }
};
template<class PT> struct ConfigCommandTypes {
    using Get=GetConfigCommand<PT>;
    using Set=SetConfigCommand<PT>;
    using Preset=PresetConfigCommand<PT>;
};
template<> struct ConfigCommandTypes<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor> { using Get=GetAutoMeshConfig_OptimumAutoMeshProcessorCommand; using Set=SetAutoMeshConfig_OptimumAutoMeshProcessorCommand; using Simple=AutoMeshSetSimple_OptimumAutoMeshProcessorCommand; using Advanced=AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand; using Preset=AutoMeshSetPreset_OptimumAutoMeshProcessorCommand; };
template<> struct ConfigCommandTypes<viewport::vertex::automesh::contours::ContourAutoMeshProcessor> { using Get=GetAutoMeshConfig_ContourAutoMeshProcessorCommand; using Set=SetAutoMeshConfig_ContourAutoMeshProcessorCommand; using Simple=AutoMeshSetSimple_ContourAutoMeshProcessorCommand; using Advanced=AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand; using Preset=AutoMeshSetPreset_ContourAutoMeshProcessorCommand; };
template<> struct ConfigCommandTypes<viewport::vertex::automesh::grid::GridAutoMeshProcessor> { using Get=GetAutoMeshConfig_GridAutoMeshProcessorCommand; using Set=SetAutoMeshConfig_GridAutoMeshProcessorCommand; using Simple=AutoMeshSetSimple_GridAutoMeshProcessorCommand; using Advanced=AutoMeshSetAdvanced_GridAutoMeshProcessorCommand; using Preset=AutoMeshSetPreset_GridAutoMeshProcessorCommand; };
template<> struct ConfigCommandTypes<viewport::vertex::automesh::skeletonize::SkeletonExtractor> { using Get=GetAutoMeshConfig_SkeletonExtractorCommand; using Set=SetAutoMeshConfig_SkeletonExtractorCommand; using Simple=AutoMeshSetSimple_SkeletonExtractorCommand; using Advanced=AutoMeshSetAdvanced_SkeletonExtractorCommand; using Preset=AutoMeshSetPreset_SkeletonExtractorCommand; };
template<class PT> using GetAutoMeshConfigPT=typename ConfigCommandTypes<PT>::Get;
template<class PT> using SetAutoMeshConfigPT=typename ConfigCommandTypes<PT>::Set;
template<class PT> using AutoMeshSetSimpleConfigCommand=typename ConfigCommandTypes<PT>::Simple;
template<class PT> using AutoMeshSetAdvancedConfigCommand=typename ConfigCommandTypes<PT>::Advanced;
template<class PT> using AutoMeshSetPresetTypedCommand=typename ConfigCommandTypes<PT>::Preset;
}
namespace nijigenerate::commands {
template<class PT> struct CommandFields<automesh::config::detail::GetConfigArguments<PT>> {
    using A=automesh::config::detail::GetConfigArguments<PT>;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{}
    };
};
template<class PT> struct CommandFields<automesh::config::detail::SetConfigArguments<PT>> {
    using A=automesh::config::detail::SetConfigArguments<PT>;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::level,"level","Config level: Simple/Advanced","string">{},
        FieldDescriptor<&A::updates,"updates","JSON object of updates","string">{}
    };
};
template<class PT> struct CommandTraits<automesh::config::GetConfigCommand<PT>> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::detail::GetConfigArguments<PT>>,Command>;
};
template<class PT> struct CommandFields<automesh::config::detail::PresetConfigArguments<PT>> {
    using A=automesh::config::detail::PresetConfigArguments<PT>;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::preset,"preset","Preset name","string">{}
    };
};
template<class PT> struct CommandTraits<automesh::config::PresetConfigCommand<PT>> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::detail::PresetConfigArguments<PT>>,Command>;
};
template<class PT> struct CommandTraits<automesh::config::SetConfigCommand<PT>> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::detail::SetConfigArguments<PT>>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshListProcessorsCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshGetActiveCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetActiveCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetActiveCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshApplyActiveCommand> {
    static constexpr auto attributes=std::tuple{EffectApply};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshGetSchemaCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshGetSchemaCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshGetValuesCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshGetValuesCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetPresetCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetPresetCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetValuesCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetValuesCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::GetAutoMeshConfig_OptimumAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::SetAutoMeshConfig_OptimumAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::GetAutoMeshConfig_ContourAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::SetAutoMeshConfig_ContourAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::GetAutoMeshConfig_GridAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::SetAutoMeshConfig_GridAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::GetAutoMeshConfig_SkeletonExtractorCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogOutput};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::GetAutoMeshConfig_SkeletonExtractorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::SetAutoMeshConfig_SkeletonExtractorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::SetAutoMeshConfig_SkeletonExtractorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetSimple_SkeletonExtractorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetSimple_SkeletonExtractorCommandArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetAdvanced_SkeletonExtractorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<automesh::config::AutoMeshSetPreset_SkeletonExtractorCommand> {
    static constexpr auto attributes=std::tuple{EffectConfigEdit};
    using BaseClasses=CommandTypeList<ExCommand<automesh::config::AutoMeshSetPreset_SkeletonExtractorCommandArguments>,Command>;
};
}

namespace nicxlive::core::d_runtime {
#define NG_AUTOMESH_CONFIG_OPEN_ORIGIN(Type, Declaration) \
template<class PT> struct SourceObjectTypeNames<::nijigenerate::commands::automesh::config::Type<PT>> { \
    static RetainedString className() { \
        return ::nijigenerate::commands::detail::commandTemplateClassName<PT>( \
            "nijigenerate.commands.automesh.config", Declaration); \
    } \
    static RetainedString templateArgumentName() { \
        return ::nijigenerate::commands::detail::commandTemplateSourceSpellings<PT>( \
            "nijigenerate.commands.automesh.config", Declaration).qualifiedArgument; \
    } \
}; \
template<class PT> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::Type<PT>> { \
    inline static constexpr bool isNativeObject=true; \
    static SourceTypeSpellingNames get() { \
        return ::nijigenerate::commands::detail::commandTemplateSourceSpellings<PT>( \
            "nijigenerate.commands.automesh.config", Declaration); \
    } \
};
NG_AUTOMESH_CONFIG_OPEN_ORIGIN(GetConfigCommand,"GetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_OPEN_ORIGIN(SetConfigCommand,"SetAutoMeshConfigPT")
#undef NG_AUTOMESH_CONFIG_OPEN_ORIGIN
template<class PT> struct SourceObjectTypeNames<::nijigenerate::commands::automesh::config::PresetConfigCommand<PT>> {
    static RetainedString className() {
        namespace Config=::nijigenerate::commands::automesh::config;
        constexpr auto child=Config::detail::configClassLeafName<PT,2>();
        auto argument=ngSourceTypeSpelling<PT>(SourceTypeSpelling::QualifiedArgument);
        return Config::detail::configMixinName("AutoMeshSetPresetTypedCommand",child,std::move(argument));
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<::nijigenerate::commands::automesh::config::PresetConfigCommand<PT>>::get()
            .qualifiedArgument;
    }
};
template<class PT> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::PresetConfigCommand<PT>> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        namespace Config=::nijigenerate::commands::automesh::config;
        constexpr auto child=Config::detail::configClassLeafName<PT,2>();
        auto argument=ngSourceTypeSpelling<PT>(SourceTypeSpelling::UnqualifiedArgument);
        return Config::detail::configMixinTypeSpellings(
            "AutoMeshSetPresetTypedCommand",child,std::move(argument));
    }
};
#define NG_AUTOMESH_CONFIG_CLASS_ORIGIN(Type) \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::automesh::config::Type> { \
    inline static constexpr char className[] = "nijigenerate.commands.automesh.config." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.automesh.config." #Type ")"; \
}; \
template<> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.automesh.config." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.automesh.config." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshConfigChangeAction)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshListProcessorsCommand)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshGetActiveCommand)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshSetActiveCommand)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshApplyActiveCommand)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshGetSchemaCommand)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshGetValuesCommand)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshSetPresetCommand)
NG_AUTOMESH_CONFIG_CLASS_ORIGIN(AutoMeshSetValuesCommand)
#undef NG_AUTOMESH_CONFIG_CLASS_ORIGIN
#define NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(Type, Processor, Declaration) \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::automesh::config::Type> { \
    static RetainedString className() { \
        return ::nijigenerate::commands::detail::commandTemplateClassName<Processor>( \
            "nijigenerate.commands.automesh.config", Declaration); \
    } \
    static RetainedString templateArgumentName() { \
        return ::nijigenerate::commands::detail::commandTemplateSourceSpellings<Processor>( \
            "nijigenerate.commands.automesh.config", Declaration).qualifiedArgument; \
    } \
}; \
template<> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::Type> { \
    inline static constexpr bool isNativeObject = true; \
    static SourceTypeSpellingNames get() { \
        return ::nijigenerate::commands::detail::commandTemplateSourceSpellings<Processor>( \
            "nijigenerate.commands.automesh.config", Declaration); \
    } \
};
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(GetAutoMeshConfig_OptimumAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor, "GetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(SetAutoMeshConfig_OptimumAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor, "SetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(GetAutoMeshConfig_ContourAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::contours::ContourAutoMeshProcessor, "GetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(SetAutoMeshConfig_ContourAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::contours::ContourAutoMeshProcessor, "SetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(GetAutoMeshConfig_GridAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor, "GetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(SetAutoMeshConfig_GridAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor, "SetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(GetAutoMeshConfig_SkeletonExtractorCommand,
    ::nijigenerate::viewport::vertex::automesh::skeletonize::SkeletonExtractor, "GetAutoMeshConfigPT")
NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN(SetAutoMeshConfig_SkeletonExtractorCommand,
    ::nijigenerate::viewport::vertex::automesh::skeletonize::SkeletonExtractor, "SetAutoMeshConfigPT")
#undef NG_AUTOMESH_CONFIG_TEMPLATE_ORIGIN
#define NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(Type, Processor, Declaration, Kind) \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::automesh::config::Type> { \
    static RetainedString className() { \
        return ::nijigenerate::commands::automesh::config::detail::configMixinName(Declaration, \
            ::nijigenerate::commands::automesh::config::detail::configClassLeafName<Processor,Kind>(), \
            ngSourceTypeSpelling<Processor>(SourceTypeSpelling::QualifiedArgument)); \
    } \
    static RetainedString templateArgumentName() { \
        return ::nijigenerate::commands::automesh::config::detail::configMixinTypeSpellings(Declaration, \
            ::nijigenerate::commands::automesh::config::detail::configClassLeafName<Processor,Kind>(), \
            ngSourceTypeSpelling<Processor>(SourceTypeSpelling::UnqualifiedArgument)).qualifiedArgument; \
    } \
}; \
template<> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::Type> { \
    inline static constexpr bool isNativeObject = true; \
    static SourceTypeSpellingNames get() { \
        return ::nijigenerate::commands::automesh::config::detail::configMixinTypeSpellings(Declaration, \
            ::nijigenerate::commands::automesh::config::detail::configClassLeafName<Processor,Kind>(), \
            ngSourceTypeSpelling<Processor>(SourceTypeSpelling::UnqualifiedArgument)); \
    } \
};
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetSimple_OptimumAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor, "AutoMeshSetSimpleConfigCommand",0)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor, "AutoMeshSetAdvancedConfigCommand",1)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetPreset_OptimumAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor, "AutoMeshSetPresetTypedCommand",2)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetSimple_ContourAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::contours::ContourAutoMeshProcessor, "AutoMeshSetSimpleConfigCommand",0)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::contours::ContourAutoMeshProcessor, "AutoMeshSetAdvancedConfigCommand",1)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetPreset_ContourAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::contours::ContourAutoMeshProcessor, "AutoMeshSetPresetTypedCommand",2)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetSimple_GridAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor, "AutoMeshSetSimpleConfigCommand",0)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetAdvanced_GridAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor, "AutoMeshSetAdvancedConfigCommand",1)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetPreset_GridAutoMeshProcessorCommand,
    ::nijigenerate::viewport::vertex::automesh::grid::GridAutoMeshProcessor, "AutoMeshSetPresetTypedCommand",2)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetSimple_SkeletonExtractorCommand,
    ::nijigenerate::viewport::vertex::automesh::skeletonize::SkeletonExtractor, "AutoMeshSetSimpleConfigCommand",0)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetAdvanced_SkeletonExtractorCommand,
    ::nijigenerate::viewport::vertex::automesh::skeletonize::SkeletonExtractor, "AutoMeshSetAdvancedConfigCommand",1)
NG_AUTOMESH_CONFIG_MIXIN_ORIGIN(AutoMeshSetPreset_SkeletonExtractorCommand,
    ::nijigenerate::viewport::vertex::automesh::skeletonize::SkeletonExtractor, "AutoMeshSetPresetTypedCommand",2)
#undef NG_AUTOMESH_CONFIG_MIXIN_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::automesh::config::detail {

// Each defining specialization below names the actual production constructor,
// its declared parameters and its own ordered presentation producer.
template<class C, class Storage, class... Parameters>
struct ConfigConstructorProgram {
    template<class... Args> struct Program {
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != sizeof...(Parameters)) return false;
            else return requires {
                { C::sourceConstructorText() } -> std::same_as<ConfigCommandText>;
            } && ::nijigenerate::commands::detail::declaredSourceParametersSupported<
                std::tuple<Parameters...>, Args...>() && baseSupported();
        }
        static consteval bool baseSupported() {
            if constexpr (sizeof...(Parameters) == 0)
                return SourceExCommandConstructorProgram<Storage>::pairSupported();
            else return SourceExCommandConstructorProgram<Storage>::template preferredSupported<Parameters&...>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
                Args&&... args) requires(supported()) {
            return ::nijigenerate::commands::detail::constructDeclaredSourceCommand<C,
                std::tuple<Parameters...>>(allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands::automesh::config::detail

namespace nijigenerate::commands {
template<> struct SourceCommandConstructor<automesh::config::AutoMeshListProcessorsCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshListProcessorsCommand,
        CommandNoArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshGetActiveCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshGetActiveCommand,
        CommandNoArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetActiveCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetActiveCommand,
        automesh::config::AutoMeshSetActiveCommandArguments, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshApplyActiveCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshApplyActiveCommand,
        CommandNoArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshGetSchemaCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshGetSchemaCommand,
        automesh::config::AutoMeshGetSchemaCommandArguments, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshGetValuesCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshGetValuesCommand,
        automesh::config::AutoMeshGetValuesCommandArguments, compat::RetainedString, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetPresetCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetPresetCommand,
        automesh::config::AutoMeshSetPresetCommandArguments, compat::RetainedString, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetValuesCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetValuesCommand,
        automesh::config::AutoMeshSetValuesCommandArguments, compat::RetainedString, compat::RetainedString, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::GetAutoMeshConfig_OptimumAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::GetAutoMeshConfig_OptimumAutoMeshProcessorCommand,
        automesh::config::GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::SetAutoMeshConfig_OptimumAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::SetAutoMeshConfig_OptimumAutoMeshProcessorCommand,
        automesh::config::SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments, compat::RetainedString, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::GetAutoMeshConfig_ContourAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::GetAutoMeshConfig_ContourAutoMeshProcessorCommand,
        automesh::config::GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::SetAutoMeshConfig_ContourAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::SetAutoMeshConfig_ContourAutoMeshProcessorCommand,
        automesh::config::SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments, compat::RetainedString, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::GetAutoMeshConfig_GridAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::GetAutoMeshConfig_GridAutoMeshProcessorCommand,
        automesh::config::GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::SetAutoMeshConfig_GridAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::SetAutoMeshConfig_GridAutoMeshProcessorCommand,
        automesh::config::SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments, compat::RetainedString, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommand,
        automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::GetAutoMeshConfig_SkeletonExtractorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::GetAutoMeshConfig_SkeletonExtractorCommand,
        automesh::config::GetAutoMeshConfig_SkeletonExtractorCommandArguments, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::SetAutoMeshConfig_SkeletonExtractorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::SetAutoMeshConfig_SkeletonExtractorCommand,
        automesh::config::SetAutoMeshConfig_SkeletonExtractorCommandArguments, compat::RetainedString, compat::RetainedString> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetSimple_SkeletonExtractorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetSimple_SkeletonExtractorCommand,
        automesh::config::AutoMeshSetSimple_SkeletonExtractorCommandArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetAdvanced_SkeletonExtractorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetAdvanced_SkeletonExtractorCommand,
        CommandNoArguments> {};
template<> struct SourceCommandConstructor<automesh::config::AutoMeshSetPreset_SkeletonExtractorCommand>
    : automesh::config::detail::ConfigConstructorProgram<automesh::config::AutoMeshSetPreset_SkeletonExtractorCommand,
        automesh::config::AutoMeshSetPreset_SkeletonExtractorCommandArguments> {};
} // namespace nijigenerate::commands


#include "detail/config_initialization.hpp"
