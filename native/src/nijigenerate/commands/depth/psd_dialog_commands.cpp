#include "psd_dialog_commands.hpp"
#include "psd_dialog_content.hpp"
#include <nijigenerate/windows/psddepthmap.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/io/depthmap_psd_enum_functions.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <cmath>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace nijigenerate::commands::depth::psd_dialog {
#define NG_PSD_DIALOG_COMMAND_PREPARE(Type) \
    void Type::prepareSourceClassInfo() { \
        using SourceBase = typename detail::ValueCommandSourceBase< \
            typename ::nijigenerate::commands::CommandTraits<Type>::BaseClasses>::Type; \
        SourceBase::prepareSourceClassInfo(); \
        ::nijigenerate::commands::detail::prepareCommandResultClass<Type>(); \
    }
NG_PSD_DIALOG_COMMAND_PREPARE(InspectPsdDepthDialogCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(GetPsdDepthDialogPartDataCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogColorSourceCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogInvertCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogBackDepthCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogFrontDepthCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogDepthScaleCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogChannelCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogSamplingCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogCustomRadiusCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogAlphaThresholdCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogMissingPolicyCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogContourRepairCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogSurfaceSmoothingCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogGpuCompositionCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogDirectGridMatchCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogProblemFilterCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerMappingAutoCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerMappingIgnoredCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerMappingTargetCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogTargetEnabledCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerEnabledCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerVisibleCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerDepthEnabledCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerDepthInvertedCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerDepthScaleCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(SetPsdDepthDialogLayerDepthOffsetCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(ResetPsdDepthDialogLayerDepthTransformCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(FillPsdDepthDialogAlphaDepthGapsCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(ApplyPsdDepthDialogCommand)
NG_PSD_DIALOG_COMMAND_PREPARE(CancelPsdDepthDialogCommand)
#undef NG_PSD_DIALOG_COMMAND_PREPARE
} // namespace nijigenerate::commands::depth::psd_dialog

namespace nijigenerate::commands::depth::psd_dialog {
using namespace windows::psddepthmap;
using core::actionstack::incActionPush;
using Text = compat::RetainedString;
using Dialog = std::shared_ptr<PSDDepthMapWindow>;
namespace {
template<class E> Text enumText(E value, Text (*name)(E), std::string_view typeName, E last) {
    const auto ordinal = static_cast<std::int32_t>(value);
    // All three original enums have contiguous, zero-based members.
    if (ordinal >= 0 && ordinal <= static_cast<std::int32_t>(last)) return name(value);
    try {
        return Text("cast(" + std::string(typeName) + ")" + std::to_string(ordinal));
    } catch (const std::bad_alloc&) {
        throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
            std::source_location::current());
    } catch (const std::length_error&) {
        throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
            std::source_location::current());
    }
}
template<std::size_t N> Text tr(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(Text::fromStaticLiteral(value));
}
template<std::size_t N> Text translateCommandLiteral(const char (&value)[N]) { return tr(value); }
Dialog contextDialog(ContextRef) { return ngActivePsdDepthMapWindow(); }
Text join(Text a, Text b) { return Text(std::string(a.view()) + std::string(b.view())); }
class PsdDepthDialogSettingsChangeAction : public actions::Action,
    public std::enable_shared_from_this<PsdDepthDialogSettingsChangeAction> {
public:
    PsdDepthDialogSettingsChangeAction(const PsdDepthDialogSettingsChangeAction&) = delete;
    PsdDepthDialogSettingsChangeAction& operator=(const PsdDepthDialogSettingsChangeAction&) = delete;
    PsdDepthDialogSettingsChangeAction(PsdDepthDialogSettingsChangeAction&&) = delete;
    PsdDepthDialogSettingsChangeAction& operator=(PsdDepthDialogSettingsChangeAction&&) = delete;
    virtual Text toString() {
        const auto keepAlive = weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
private:
    Dialog dialog;
    SettingsState oldState, newState;
    Text changeKey, changeLabel;
    bool mergeable = false;
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    PsdDepthDialogSettingsChangeAction() = default;
    void constructSource(const Dialog& dialog, const SettingsState& oldState, const SettingsState& newState,
            const Text& changeKey, const Text& changeLabel, bool mergeable) {
        this->dialog = dialog;
        this->oldState = oldState;
        this->newState = newState;
        this->changeKey = changeKey;
        this->changeLabel = changeLabel;
        this->mergeable = mergeable;
    }
public:
    template<class BuildArguments>
    static std::shared_ptr<PsdDepthDialogSettingsChangeAction> createSource(BuildArguments&& buildArguments) {
        auto action = std::allocate_shared<PsdDepthDialogSettingsChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<PsdDepthDialogSettingsChangeAction>{});
        auto arguments = std::forward<BuildArguments>(buildArguments)();
        std::apply([&](auto&&... values) { action->constructSource(values...); }, arguments);
        return action;
    }
    PsdDepthDialogSettingsChangeAction(Dialog dialog, SettingsState oldState, SettingsState newState,
            Text changeKey, Text changeLabel, bool mergeable) {
        constructSource(dialog, oldState, newState, changeKey, changeLabel, mergeable);
    }
    void rollback() override {
        const auto keepAlive = weak_from_this().lock();
        actions::node::detail::requireActionObject(dialog)->applyDialogSettingsState(oldState);
    }
    void redo() override {
        const auto keepAlive = weak_from_this().lock();
        actions::node::detail::requireActionObject(dialog)->applyDialogSettingsState(newState);
    }
    actions::ActionText describe() override { return actions::ActionText(changeLabel); }
    actions::ActionText describeUndo() override { return actions::ActionText(changeLabel); }
    actions::ActionText getName() override {
        return actions::ActionText(Text::fromStaticLiteral("PsdDepthDialogSettingsChangeAction"));
    }
    bool merge(actions::ActionPtr other) override {
        const auto keepAlive = weak_from_this().lock();
        auto action = std::dynamic_pointer_cast<PsdDepthDialogSettingsChangeAction>(other);
        if (!action || !canMerge(other)) return false;
        newState = action->newState; return true;
    }
    bool canMerge(actions::ActionPtr other) override {
        auto action = std::dynamic_pointer_cast<PsdDepthDialogSettingsChangeAction>(other);
        return mergeable && action && action->mergeable && action->dialog == dialog &&
            action->changeKey == changeKey;
    }
};
class PsdDepthDialogLayerChangeAction : public actions::Action,
    public std::enable_shared_from_this<PsdDepthDialogLayerChangeAction> {
public:
    PsdDepthDialogLayerChangeAction(const PsdDepthDialogLayerChangeAction&) = delete;
    PsdDepthDialogLayerChangeAction& operator=(const PsdDepthDialogLayerChangeAction&) = delete;
    PsdDepthDialogLayerChangeAction(PsdDepthDialogLayerChangeAction&&) = delete;
    PsdDepthDialogLayerChangeAction& operator=(PsdDepthDialogLayerChangeAction&&) = delete;
    virtual Text toString() {
        const auto keepAlive = weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
private:
    Dialog dialog;
    LayerState oldState, newState;
    Text changeKey, changeLabel;
    bool stageForApply = false;
    bool mergeable = false;
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    PsdDepthDialogLayerChangeAction() = default;
    void constructSource(const Dialog& dialog, const LayerState& oldState, const LayerState& newState,
            const Text& changeKey, const Text& changeLabel, bool stageForApply, bool mergeable) {
        this->dialog = dialog;
        this->oldState = oldState;
        this->newState = newState;
        this->changeKey = changeKey;
        this->changeLabel = changeLabel;
        this->stageForApply = stageForApply;
        this->mergeable = mergeable;
    }
public:
    template<class BuildArguments>
    static std::shared_ptr<PsdDepthDialogLayerChangeAction> createSource(BuildArguments&& buildArguments) {
        auto action = std::allocate_shared<PsdDepthDialogLayerChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<PsdDepthDialogLayerChangeAction>{});
        auto arguments = std::forward<BuildArguments>(buildArguments)();
        std::apply([&](auto&&... values) { action->constructSource(values...); }, arguments);
        return action;
    }
    PsdDepthDialogLayerChangeAction(Dialog dialog, LayerState oldState, LayerState newState,
            Text changeKey, Text changeLabel, bool stageForApply, bool mergeable) {
        constructSource(dialog, oldState, newState, changeKey, changeLabel, stageForApply, mergeable);
    }
    void rollback() override {
        const auto keepAlive = weak_from_this().lock();
        actions::node::detail::requireActionObject(dialog)->applyDialogLayerState(oldState, stageForApply);
    }
    void redo() override {
        const auto keepAlive = weak_from_this().lock();
        actions::node::detail::requireActionObject(dialog)->applyDialogLayerState(newState, stageForApply);
    }
    actions::ActionText describe() override { return actions::ActionText(changeLabel); }
    actions::ActionText describeUndo() override { return actions::ActionText(changeLabel); }
    actions::ActionText getName() override {
        return actions::ActionText(Text::fromStaticLiteral("PsdDepthDialogLayerChangeAction"));
    }
    bool merge(actions::ActionPtr other) override {
        const auto keepAlive = weak_from_this().lock();
        auto action = std::dynamic_pointer_cast<PsdDepthDialogLayerChangeAction>(other);
        if (!action || !canMerge(other)) return false;
        newState = action->newState; return true;
    }
    bool canMerge(actions::ActionPtr other) override {
        auto action = std::dynamic_pointer_cast<PsdDepthDialogLayerChangeAction>(other);
        return mergeable && action && action->mergeable && action->dialog == dialog &&
            action->changeKey == changeKey && action->oldState.layerPath == oldState.layerPath &&
            action->oldState.targetGridUuid == oldState.targetGridUuid;
    }
};
class PsdDepthDialogPixelsChangeAction : public actions::Action,
    public std::enable_shared_from_this<PsdDepthDialogPixelsChangeAction> {
    Dialog dialog;
    compat::Slice<PsdDepthDialogLayerPixels> oldState, newState;
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    PsdDepthDialogPixelsChangeAction() = default;
    void constructSource(const Dialog& dialog, const compat::Slice<PsdDepthDialogLayerPixels>& oldState,
            const compat::Slice<PsdDepthDialogLayerPixels>& newState) {
        this->dialog = dialog;
        this->oldState = oldState;
        this->newState = newState;
    }
public:
    template<class BuildArguments>
    static std::shared_ptr<PsdDepthDialogPixelsChangeAction> createSource(BuildArguments&& buildArguments) {
        auto action = std::allocate_shared<PsdDepthDialogPixelsChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<PsdDepthDialogPixelsChangeAction>{});
        auto arguments = std::forward<BuildArguments>(buildArguments)();
        std::apply([&](auto&&... values) { action->constructSource(values...); }, arguments);
        return action;
    }
    PsdDepthDialogPixelsChangeAction(const PsdDepthDialogPixelsChangeAction&) = delete;
    PsdDepthDialogPixelsChangeAction& operator=(const PsdDepthDialogPixelsChangeAction&) = delete;
    PsdDepthDialogPixelsChangeAction(PsdDepthDialogPixelsChangeAction&&) = delete;
    PsdDepthDialogPixelsChangeAction& operator=(PsdDepthDialogPixelsChangeAction&&) = delete;
    virtual Text toString() {
        const auto keepAlive = weak_from_this().lock();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    PsdDepthDialogPixelsChangeAction(Dialog dialog, compat::Slice<PsdDepthDialogLayerPixels> oldState,
            compat::Slice<PsdDepthDialogLayerPixels> newState) {
        constructSource(dialog, oldState, newState);
    }
    void rollback() override {
        const auto keepAlive = weak_from_this().lock();
        actions::node::detail::requireActionObject(dialog)->applyDialogLayerPixels(oldState);
    }
    void redo() override {
        const auto keepAlive = weak_from_this().lock();
        actions::node::detail::requireActionObject(dialog)->applyDialogLayerPixels(newState);
    }
    actions::ActionText describe() override {
        const auto keepAlive = weak_from_this().lock();
        return actions::ActionText(tr("Filled alpha-depth gaps"));
    }
    actions::ActionText describeUndo() override {
        const auto keepAlive = weak_from_this().lock();
        return actions::ActionText(tr("Alpha-depth gap fill was reverted"));
    }
    actions::ActionText getName() override {
        return actions::ActionText(Text::fromStaticLiteral("PsdDepthDialogPixelsChangeAction"));
    }
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
};
} // namespace
} // namespace nijigenerate::commands::depth::psd_dialog

namespace nicxlive::core::d_runtime {
#define NG_PSD_PRIVATE_ACTION_SOURCE_NAMES(Type) \
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
    };
NG_PSD_PRIVATE_ACTION_SOURCE_NAMES(PsdDepthDialogSettingsChangeAction)
NG_PSD_PRIVATE_ACTION_SOURCE_NAMES(PsdDepthDialogLayerChangeAction)
NG_PSD_PRIVATE_ACTION_SOURCE_NAMES(PsdDepthDialogPixelsChangeAction)
#undef NG_PSD_PRIVATE_ACTION_SOURCE_NAMES
}

namespace nijigenerate::commands::depth::psd_dialog {
namespace {
[[maybe_unused]] const bool sourcePrivateActionClassesRegistered = [] {
    using nicxlive::core::d_runtime::ngRegisterObjectClassName;
    using nicxlive::core::d_runtime::SourceObjectTypeNames;
    ngRegisterObjectClassName(typeid(PsdDepthDialogSettingsChangeAction), Text::fromStaticLiteral(
        SourceObjectTypeNames<PsdDepthDialogSettingsChangeAction>::className));
    ngRegisterObjectClassName(typeid(PsdDepthDialogLayerChangeAction), Text::fromStaticLiteral(
        SourceObjectTypeNames<PsdDepthDialogLayerChangeAction>::className));
    ngRegisterObjectClassName(typeid(PsdDepthDialogPixelsChangeAction), Text::fromStaticLiteral(
        SourceObjectTypeNames<PsdDepthDialogPixelsChangeAction>::className));
    return true;
}();
ResultRef applyMappingChange(ContextRef ctx, Text layerPath,
    std::shared_ptr<nicxlive::core::nodes::Node> target, bool ignored, bool automatic) {
    auto dialog = contextDialog(ctx);
    if (!dialog || !dialog->dialogCommandsAvailable())
        return CommandResult::opCall(false, Text::fromStaticLiteral("PSD depth import dialog is not displayed"));
    auto oldState = dialog->captureDialogSettingsState();
    bool layerAvailable = false;
    for (auto& layer : oldState.layers) {
        if (layer.layerPath != layerPath) continue;
        layerAvailable = true; break;
    }
    if (!layerAvailable) return CommandResult::opCall(false,
        Text::fromStaticLiteral("PSD depth source layer is not available"));
    auto nextState = dialog->captureDialogSettingsState();
    nextState.settings.ignoredLayerPaths.remove(layerPath.view());
    nextState.settings.layerTargetGridUuidOverrides.remove(layerPath.view());
    if (ignored) nextState.settings.ignoredLayerPaths.setRetained(layerPath, true);
    else if (!automatic) {
        if (!target || !dialog->dialogMappingTargetAvailable(target))
            return CommandResult::opCall(false,
                Text::fromStaticLiteral("PSD depth mapping target is not available"));
        const auto uuid = compat::ngRequireObject(target)->getUUID();
        Text targetUuid;
        try {
            targetUuid = Text(std::to_string(uuid));
        } catch (const std::bad_alloc&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        } catch (const std::length_error&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        }
        nextState.settings.layerTargetGridUuidOverrides.setRetained(layerPath, targetUuid);
    }
    if (!dialog->applyDialogSettingsState(nextState)) return CommandResult::opCall(true);
    incActionPush(PsdDepthDialogSettingsChangeAction::createSource([&] {
        auto sourceDialog = dialog;
        auto sourceOldState = oldState;
        auto sourceNewState = nextState;
        Text sourceChangeKey;
        try {
            sourceChangeKey = join(Text::fromStaticLiteral("mapping:"), layerPath);
        } catch (const std::bad_alloc&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        } catch (const std::length_error&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        }
        auto sourceChangeLabel = translateCommandLiteral("Changed PSD depth layer mapping");
        const bool sourceMergeable = false;
        return std::tuple{std::move(sourceDialog), std::move(sourceOldState), std::move(sourceNewState),
            std::move(sourceChangeKey), std::move(sourceChangeLabel), sourceMergeable};
    }));
    return CommandResult::opCall(true);
}
}
namespace detail {
ValueCommandDialog ngValueCommandDialog(const ContextRef& ctx) { return contextDialog(ctx); }
bool ngValueCommandAvailable(const ValueCommandDialog& dialog) {
    return actions::node::detail::requireActionObject(dialog)->dialogCommandsAvailable();
}
SettingsState ngValueCommandSettings(const ValueCommandDialog& dialog) {
    return actions::node::detail::requireActionObject(dialog)->captureDialogSettingsState();
}
bool ngApplyValueCommandSettings(const ValueCommandDialog& dialog, const SettingsState& state) {
    return actions::node::detail::requireActionObject(dialog)->applyDialogSettingsState(state);
}
bool ngCaptureValueCommandLayer(const ValueCommandDialog& dialog, const ContextRef& ctx,
        const Text& path, LayerState& state) {
    return actions::node::detail::requireActionObject(dialog)->captureDialogContextLayerState(ctx, path, state);
}
bool ngApplyValueCommandLayer(const ValueCommandDialog& dialog, const LayerState& state, const bool& stage) {
    return actions::node::detail::requireActionObject(dialog)->applyDialogLayerState(state, stage);
}
void ngPushValueCommandSettings(const ValueCommandDialog& dialog, const SettingsState& oldState,
        const SettingsState& nextState, const Text& key, const Text& label, const bool& mergeable) {
    incActionPush(PsdDepthDialogSettingsChangeAction::createSource([&] {
        auto actionDialog = dialog;
        auto actionOldState = oldState;
        auto actionNewState = nextState;
        auto actionKey = key;
        auto actionLabel = label;
        const auto actionMergeable = mergeable;
        return std::tuple{std::move(actionDialog), std::move(actionOldState), std::move(actionNewState),
            std::move(actionKey), std::move(actionLabel), actionMergeable};
    }));
}
void ngPushValueCommandLayer(const ValueCommandDialog& dialog, const LayerState& oldState,
        const LayerState& nextState, const Text& key, const Text& path,
        const Text& label, const bool& stage, const bool& mergeable) {
    incActionPush(PsdDepthDialogLayerChangeAction::createSource([&] {
        auto actionDialog = dialog;
        auto actionOldState = oldState;
        auto actionNewState = nextState;
        Text actionKey;
        try {
            std::string bytes;
            if (!key.empty()) bytes.append(key.data(), key.size());
            bytes.push_back(':');
            if (!path.empty()) bytes.append(path.data(), path.size());
            actionKey = Text(std::move(bytes));
        } catch (const std::bad_alloc&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        } catch (const std::length_error&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        }
        auto actionLabel = label;
        const auto actionStageForApply = stage;
        const auto actionMergeable = mergeable;
        return std::tuple{std::move(actionDialog), std::move(actionOldState), std::move(actionNewState),
            std::move(actionKey), std::move(actionLabel), actionStageForApply, actionMergeable};
    }));
}
}
InspectPsdDepthDialogCommand::SourceConstructorArguments InspectPsdDepthDialogCommand::sourceConstructorArguments() {
    return {tr("Inspect PSD Depth Dialog"),
            tr("Read the settings, source layers, and target UUIDs of the displayed PSD depth dialog.")};
}
InspectPsdDepthDialogCommand::InspectPsdDepthDialogCommand()
    : InspectPsdDepthDialogCommand(sourceConstructorArguments()) {}
InspectPsdDepthDialogCommand::InspectPsdDepthDialogCommand(SourceConstructorArguments arguments)
    : NativeBase{arguments.label, arguments.description} {}
bool InspectPsdDepthDialogCommand::runnable(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        return dialog && dialog->dialogCommandsAvailable();
    
}
SetPsdDepthDialogColorSourceCommand::SourceConstructorArguments SetPsdDepthDialogColorSourceCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Depth Color Source"), translateCommandLiteral("Set the color source used by the displayed PSD depth dialog."),
        Text::fromStaticLiteral("color-source"), translateCommandLiteral("Changed PSD depth color source")};
}
SetPsdDepthDialogColorSourceCommand::SetPsdDepthDialogColorSourceCommand()
    : SetPsdDepthDialogColorSourceCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogColorSourceCommand::SetPsdDepthDialogColorSourceCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogColorSourceArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogColorSourceCommand::update(SettingsState& state) { state.settings.colorSourcePath = value; }
SetPsdDepthDialogInvertCommand::SourceConstructorArguments SetPsdDepthDialogInvertCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Depth Inversion"), translateCommandLiteral("Set global depth inversion in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("invert-depth"), translateCommandLiteral("Changed PSD depth inversion")};
}
SetPsdDepthDialogInvertCommand::SetPsdDepthDialogInvertCommand()
    : SetPsdDepthDialogInvertCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogInvertCommand::SetPsdDepthDialogInvertCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogInvertArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogInvertCommand::update(SettingsState& state) { state.settings.invert = value; }
SetPsdDepthDialogBackDepthCommand::SourceConstructorArguments SetPsdDepthDialogBackDepthCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Back Depth"), translateCommandLiteral("Set the back depth in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("back-depth"), translateCommandLiteral("Changed PSD back depth"), true};
}
SetPsdDepthDialogBackDepthCommand::SetPsdDepthDialogBackDepthCommand()
    : SetPsdDepthDialogBackDepthCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogBackDepthCommand::SetPsdDepthDialogBackDepthCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogBackDepthArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogBackDepthCommand::update(SettingsState& state) { state.settings.backDepth = value; }
SetPsdDepthDialogFrontDepthCommand::SourceConstructorArguments SetPsdDepthDialogFrontDepthCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Front Depth"), translateCommandLiteral("Set the front depth in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("front-depth"), translateCommandLiteral("Changed PSD front depth"), true};
}
SetPsdDepthDialogFrontDepthCommand::SetPsdDepthDialogFrontDepthCommand()
    : SetPsdDepthDialogFrontDepthCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogFrontDepthCommand::SetPsdDepthDialogFrontDepthCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogFrontDepthArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogFrontDepthCommand::update(SettingsState& state) { state.settings.frontDepth = value; }
SetPsdDepthDialogDepthScaleCommand::SourceConstructorArguments SetPsdDepthDialogDepthScaleCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Depth Scale"), translateCommandLiteral("Set the global depth scale in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("depth-scale"), translateCommandLiteral("Changed PSD depth scale"), true};
}
SetPsdDepthDialogDepthScaleCommand::SetPsdDepthDialogDepthScaleCommand()
    : SetPsdDepthDialogDepthScaleCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogDepthScaleCommand::SetPsdDepthDialogDepthScaleCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogDepthScaleArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogDepthScaleCommand::update(SettingsState& state) { state.settings.depthScale = value; }
Text SetPsdDepthDialogDepthScaleCommand::validate(SettingsState& state) {
        return std::isfinite(state.settings.depthScale) && state.settings.depthScale >= 0.0f
            ? Text{} : Text::fromStaticLiteral("PSD depth scale must be finite and non-negative");
}
SetPsdDepthDialogChannelCommand::SourceConstructorArguments SetPsdDepthDialogChannelCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Depth Channel"), translateCommandLiteral("Set the depth channel in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("channel"), translateCommandLiteral("Changed PSD depth channel")};
}
SetPsdDepthDialogChannelCommand::SetPsdDepthDialogChannelCommand()
    : SetPsdDepthDialogChannelCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogChannelCommand::SetPsdDepthDialogChannelCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogChannelArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogChannelCommand::update(SettingsState& state) { state.settings.channel = value; }
SetPsdDepthDialogSamplingCommand::SourceConstructorArguments SetPsdDepthDialogSamplingCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Depth Sampling"), translateCommandLiteral("Set depth sampling in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("sampling"), translateCommandLiteral("Changed PSD depth sampling")};
}
SetPsdDepthDialogSamplingCommand::SetPsdDepthDialogSamplingCommand()
    : SetPsdDepthDialogSamplingCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogSamplingCommand::SetPsdDepthDialogSamplingCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogSamplingArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogSamplingCommand::update(SettingsState& state) { state.settings.convolution = value; }
SetPsdDepthDialogCustomRadiusCommand::SourceConstructorArguments SetPsdDepthDialogCustomRadiusCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Sampling Radius"), translateCommandLiteral("Set the custom sampling radius in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("custom-radius"), translateCommandLiteral("Changed PSD depth sampling radius"), true};
}
SetPsdDepthDialogCustomRadiusCommand::SetPsdDepthDialogCustomRadiusCommand()
    : SetPsdDepthDialogCustomRadiusCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogCustomRadiusCommand::SetPsdDepthDialogCustomRadiusCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogCustomRadiusArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogCustomRadiusCommand::update(SettingsState& state) { state.settings.customRadius = value; }
Text SetPsdDepthDialogCustomRadiusCommand::validate(SettingsState& state) {
        return state.settings.customRadius >= 1 && state.settings.customRadius <= 64
            ? Text{} : Text::fromStaticLiteral("PSD custom sampling radius must be in [1, 64]");
}
SetPsdDepthDialogAlphaThresholdCommand::SourceConstructorArguments SetPsdDepthDialogAlphaThresholdCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Alpha Threshold"), translateCommandLiteral("Set the alpha threshold in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("alpha-threshold"), translateCommandLiteral("Changed PSD depth alpha threshold"), true};
}
SetPsdDepthDialogAlphaThresholdCommand::SetPsdDepthDialogAlphaThresholdCommand()
    : SetPsdDepthDialogAlphaThresholdCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogAlphaThresholdCommand::SetPsdDepthDialogAlphaThresholdCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogAlphaThresholdArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogAlphaThresholdCommand::update(SettingsState& state) { state.settings.alphaThreshold = value; }
Text SetPsdDepthDialogAlphaThresholdCommand::validate(SettingsState& state) {
        return std::isfinite(state.settings.alphaThreshold) &&
            state.settings.alphaThreshold >= 0.0f && state.settings.alphaThreshold <= 1.0f
            ? Text{} : Text::fromStaticLiteral("PSD alpha threshold must be finite and in [0, 1]");
}
SetPsdDepthDialogMissingPolicyCommand::SourceConstructorArguments SetPsdDepthDialogMissingPolicyCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Missing Pixel Policy"), translateCommandLiteral("Set missing-pixel handling in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("missing-policy"), translateCommandLiteral("Changed PSD missing depth policy")};
}
SetPsdDepthDialogMissingPolicyCommand::SetPsdDepthDialogMissingPolicyCommand()
    : SetPsdDepthDialogMissingPolicyCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogMissingPolicyCommand::SetPsdDepthDialogMissingPolicyCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogMissingPolicyArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogMissingPolicyCommand::update(SettingsState& state) { state.settings.missingPolicy = value; }
SetPsdDepthDialogContourRepairCommand::SourceConstructorArguments SetPsdDepthDialogContourRepairCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Contour Repair"), translateCommandLiteral("Set contour repair in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("repair-contour"), translateCommandLiteral("Changed PSD contour repair")};
}
SetPsdDepthDialogContourRepairCommand::SetPsdDepthDialogContourRepairCommand()
    : SetPsdDepthDialogContourRepairCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogContourRepairCommand::SetPsdDepthDialogContourRepairCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogContourRepairArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogContourRepairCommand::update(SettingsState& state) { state.settings.repairContourBand = value; }
SetPsdDepthDialogSurfaceSmoothingCommand::SourceConstructorArguments SetPsdDepthDialogSurfaceSmoothingCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Surface Smoothing"), translateCommandLiteral("Set surface smoothing in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("smooth-surface"), translateCommandLiteral("Changed PSD surface smoothing")};
}
SetPsdDepthDialogSurfaceSmoothingCommand::SetPsdDepthDialogSurfaceSmoothingCommand()
    : SetPsdDepthDialogSurfaceSmoothingCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogSurfaceSmoothingCommand::SetPsdDepthDialogSurfaceSmoothingCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogSurfaceSmoothingArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogSurfaceSmoothingCommand::update(SettingsState& state) { state.settings.smoothWavySurface = value; }
FillPsdDepthDialogAlphaDepthGapsCommand::SourceConstructorArguments FillPsdDepthDialogAlphaDepthGapsCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Fill PSD Depth Dialog Alpha-Depth Gaps"),
            translateCommandLiteral("Fill alpha-depth gaps in the displayed PSD depth import dialog.")};
}
FillPsdDepthDialogAlphaDepthGapsCommand::FillPsdDepthDialogAlphaDepthGapsCommand()
    : FillPsdDepthDialogAlphaDepthGapsCommand(sourceConstructorArguments()) {}
FillPsdDepthDialogAlphaDepthGapsCommand::FillPsdDepthDialogAlphaDepthGapsCommand(SourceConstructorArguments arguments)
    : ExCommand<CommandNoArguments>{arguments.label, arguments.description} {}
bool FillPsdDepthDialogAlphaDepthGapsCommand::runnable(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        return dialog && dialog->dialogCommandsAvailable();
    
}
ResultRef FillPsdDepthDialogAlphaDepthGapsCommand::run(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false,
        Text::fromStaticLiteral("PSD depth import dialog is not displayed"));
    auto dialog = contextDialog(ctx);
    auto oldState = actions::node::detail::requireActionObject(dialog)->captureDialogLayerPixels();
    if (!dialog->applyDialogAlphaDepthGapFill()) return CommandResult::opCall(false,
        Text::fromStaticLiteral("Alpha-depth gap fill failed"));
    auto newState = dialog->captureDialogLayerPixels();
    if (!PSDDepthMapWindow::dialogLayerPixelsEqual(oldState, newState)) {
        incActionPush(PsdDepthDialogPixelsChangeAction::createSource([&] {
            auto actionDialog = dialog;
            auto actionOldState = oldState;
            auto actionNewState = newState;
            return std::tuple{std::move(actionDialog), std::move(actionOldState), std::move(actionNewState)};
        }));
    }
    return CommandResult::opCall(true);
}
SetPsdDepthDialogGpuCompositionCommand::SourceConstructorArguments SetPsdDepthDialogGpuCompositionCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD GPU Composition"), translateCommandLiteral("Set GPU composition in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("gpu-composition"), translateCommandLiteral("Changed PSD GPU composition")};
}
SetPsdDepthDialogGpuCompositionCommand::SetPsdDepthDialogGpuCompositionCommand()
    : SetPsdDepthDialogGpuCompositionCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogGpuCompositionCommand::SetPsdDepthDialogGpuCompositionCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogGpuCompositionArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogGpuCompositionCommand::update(SettingsState& state) { state.settings.useGpuComposition = value; }
SetPsdDepthDialogDirectGridMatchCommand::SourceConstructorArguments SetPsdDepthDialogDirectGridMatchCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Direct Grid Match"), translateCommandLiteral("Set direct grid-name matching in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("direct-grid-match"), translateCommandLiteral("Changed PSD direct grid matching")};
}
SetPsdDepthDialogDirectGridMatchCommand::SetPsdDepthDialogDirectGridMatchCommand()
    : SetPsdDepthDialogDirectGridMatchCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogDirectGridMatchCommand::SetPsdDepthDialogDirectGridMatchCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogDirectGridMatchArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogDirectGridMatchCommand::update(SettingsState& state) { state.settings.matchDirectGridName = value; }
SetPsdDepthDialogProblemFilterCommand::SourceConstructorArguments SetPsdDepthDialogProblemFilterCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Problem Layer Filter"), translateCommandLiteral("Set the problem-layer filter in the displayed PSD depth dialog."),
        Text::fromStaticLiteral("problem-filter"), translateCommandLiteral("Changed PSD problem layer filter")};
}
SetPsdDepthDialogProblemFilterCommand::SetPsdDepthDialogProblemFilterCommand()
    : SetPsdDepthDialogProblemFilterCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogProblemFilterCommand::SetPsdDepthDialogProblemFilterCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogSettingsValueCommand<SetPsdDepthDialogProblemFilterArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.mergeable} {}
void SetPsdDepthDialogProblemFilterCommand::update(SettingsState& state) { state.onlyProblemLayers = value; }
SetPsdDepthDialogLayerMappingAutoCommand::SourceConstructorArguments SetPsdDepthDialogLayerMappingAutoCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Use Automatic PSD Layer Mapping"),
        translateCommandLiteral("Return a Source / Mapping layer to automatic target matching.")};
}
SetPsdDepthDialogLayerMappingAutoCommand::SetPsdDepthDialogLayerMappingAutoCommand()
    : SetPsdDepthDialogLayerMappingAutoCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerMappingAutoCommand::SetPsdDepthDialogLayerMappingAutoCommand(SourceConstructorArguments arguments)
    : ExCommand<SetPsdDepthDialogLayerMappingAutoArguments>{arguments.label, arguments.description} {}
bool SetPsdDepthDialogLayerMappingAutoCommand::runnable(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    auto dialog = contextDialog(ctx);
    return dialog && dialog->dialogCommandsAvailable();
}
ResultRef SetPsdDepthDialogLayerMappingAutoCommand::run(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    return applyMappingChange(ctx, layerPath, {}, false, true);
}
SetPsdDepthDialogLayerMappingIgnoredCommand::SourceConstructorArguments SetPsdDepthDialogLayerMappingIgnoredCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Ignore PSD Source Layer"), translateCommandLiteral("Ignore a layer in Source / Mapping.")};
}
SetPsdDepthDialogLayerMappingIgnoredCommand::SetPsdDepthDialogLayerMappingIgnoredCommand()
    : SetPsdDepthDialogLayerMappingIgnoredCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerMappingIgnoredCommand::SetPsdDepthDialogLayerMappingIgnoredCommand(SourceConstructorArguments arguments)
    : ExCommand<SetPsdDepthDialogLayerMappingIgnoredArguments>{arguments.label, arguments.description} {}
bool SetPsdDepthDialogLayerMappingIgnoredCommand::runnable(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    auto dialog = contextDialog(ctx);
    return dialog && dialog->dialogCommandsAvailable();
}
ResultRef SetPsdDepthDialogLayerMappingIgnoredCommand::run(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    return applyMappingChange(ctx, layerPath, {}, true, false);
}
SetPsdDepthDialogLayerMappingTargetCommand::SourceConstructorArguments SetPsdDepthDialogLayerMappingTargetCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Remap PSD Source Layer"), translateCommandLiteral("Assign a Source / Mapping layer to a specific target.")};
}
SetPsdDepthDialogLayerMappingTargetCommand::SetPsdDepthDialogLayerMappingTargetCommand()
    : SetPsdDepthDialogLayerMappingTargetCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerMappingTargetCommand::SetPsdDepthDialogLayerMappingTargetCommand(SourceConstructorArguments arguments)
    : ExCommand<SetPsdDepthDialogLayerMappingTargetArguments>{arguments.label, arguments.description} {}
bool SetPsdDepthDialogLayerMappingTargetCommand::runnable(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        return dialog && dialog->dialogCommandsAvailable() && dialog->dialogMappingTargetAvailable(target);
    
}
ResultRef SetPsdDepthDialogLayerMappingTargetCommand::run(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    return applyMappingChange(ctx, layerPath, target, false, false);
}
SetPsdDepthDialogTargetEnabledCommand::SourceConstructorArguments SetPsdDepthDialogTargetEnabledCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Target Enabled"),
        translateCommandLiteral("Enable or disable the Context target in Source / Mapping.")};
}
SetPsdDepthDialogTargetEnabledCommand::SetPsdDepthDialogTargetEnabledCommand()
    : SetPsdDepthDialogTargetEnabledCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogTargetEnabledCommand::SetPsdDepthDialogTargetEnabledCommand(SourceConstructorArguments arguments)
    : ExCommand<SetPsdDepthDialogTargetEnabledArguments>{arguments.label, arguments.description} {}
bool SetPsdDepthDialogTargetEnabledCommand::runnable(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        std::uint64_t uuid = 0;
        return dialog && dialog->dialogCommandsAvailable() &&
            dialog->dialogContextTargetUuid(ctx, uuid);
    
}
ResultRef SetPsdDepthDialogTargetEnabledCommand::run(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        std::uint64_t uuid = 0;
        if (!dialog || !dialog->dialogCommandsAvailable() ||
            !dialog->dialogContextTargetUuid(ctx, uuid)) {
            return CommandResult::opCall(false,
                Text::fromStaticLiteral("PSD depth target is not selected in Context"));
        }
        auto oldState = dialog->captureDialogSettingsState();
        auto nextState = dialog->captureDialogSettingsState();
        Text key;
        try {
            key = Text(std::to_string(uuid));
        } catch (const std::bad_alloc&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        } catch (const std::length_error&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        }
        if (value) nextState.settings.disabledGridUuids.remove(key.view());
        else nextState.settings.disabledGridUuids.setRetained(key, true);
        if (!dialog->applyDialogSettingsState(nextState)) return CommandResult::opCall(true);
        incActionPush(PsdDepthDialogSettingsChangeAction::createSource([&] {
            auto actionDialog = dialog;
            auto actionOldState = oldState;
            auto actionNewState = nextState;
            Text actionKey;
            try {
                actionKey = join(Text::fromStaticLiteral("target-enabled:"), key);
            } catch (const std::bad_alloc&) {
                throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                    std::source_location::current());
            } catch (const std::length_error&) {
                throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                    std::source_location::current());
            }
            auto actionLabel = translateCommandLiteral("Changed PSD depth target state");
            const auto actionMergeable = false;
            return std::tuple{std::move(actionDialog), std::move(actionOldState), std::move(actionNewState),
                std::move(actionKey), std::move(actionLabel), actionMergeable};
        }));
        return CommandResult::opCall(true);
    
}
SetPsdDepthDialogLayerEnabledCommand::SourceConstructorArguments SetPsdDepthDialogLayerEnabledCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Layer Enabled"), translateCommandLiteral("Enable or disable the selected composed layer."),
        Text::fromStaticLiteral("layer-enabled"), translateCommandLiteral("Changed PSD depth layer state"), false};
}
SetPsdDepthDialogLayerEnabledCommand::SetPsdDepthDialogLayerEnabledCommand()
    : SetPsdDepthDialogLayerEnabledCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerEnabledCommand::SetPsdDepthDialogLayerEnabledCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerEnabledArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.stageForApply, arguments.mergeable} {}
void SetPsdDepthDialogLayerEnabledCommand::update(LayerState& state) { state.enabled = value; state.visible = value; }
SetPsdDepthDialogLayerVisibleCommand::SourceConstructorArguments SetPsdDepthDialogLayerVisibleCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Layer Visible"), translateCommandLiteral("Show or hide the selected layer in 3D Adjust."),
        Text::fromStaticLiteral("layer-visible"), translateCommandLiteral("Changed PSD depth layer visibility"), false};
}
SetPsdDepthDialogLayerVisibleCommand::SetPsdDepthDialogLayerVisibleCommand()
    : SetPsdDepthDialogLayerVisibleCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerVisibleCommand::SetPsdDepthDialogLayerVisibleCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerVisibleArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.stageForApply, arguments.mergeable} {}
void SetPsdDepthDialogLayerVisibleCommand::update(LayerState& state) { state.visible = value; }
SetPsdDepthDialogLayerDepthEnabledCommand::SourceConstructorArguments SetPsdDepthDialogLayerDepthEnabledCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Layer Depth Enabled"), translateCommandLiteral("Enable or attach depth for the selected layer."),
        Text::fromStaticLiteral("layer-depth-enabled"), translateCommandLiteral("Changed PSD layer depth attachment"), false};
}
SetPsdDepthDialogLayerDepthEnabledCommand::SetPsdDepthDialogLayerDepthEnabledCommand()
    : SetPsdDepthDialogLayerDepthEnabledCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerDepthEnabledCommand::SetPsdDepthDialogLayerDepthEnabledCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthEnabledArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.stageForApply, arguments.mergeable} {}
void SetPsdDepthDialogLayerDepthEnabledCommand::update(LayerState& state) { state.depthEnabled = value; }
SetPsdDepthDialogLayerDepthInvertedCommand::SourceConstructorArguments SetPsdDepthDialogLayerDepthInvertedCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Layer Depth Inversion"), translateCommandLiteral("Invert depth for the selected layer."),
        Text::fromStaticLiteral("layer-invert"), translateCommandLiteral("Changed PSD layer depth inversion"), true};
}
SetPsdDepthDialogLayerDepthInvertedCommand::SetPsdDepthDialogLayerDepthInvertedCommand()
    : SetPsdDepthDialogLayerDepthInvertedCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerDepthInvertedCommand::SetPsdDepthDialogLayerDepthInvertedCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthInvertedArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.stageForApply, arguments.mergeable} {}
void SetPsdDepthDialogLayerDepthInvertedCommand::update(LayerState& state) { state.invert = value; }
SetPsdDepthDialogLayerDepthScaleCommand::SourceConstructorArguments SetPsdDepthDialogLayerDepthScaleCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Layer Z Scale"), translateCommandLiteral("Set Z scale for the selected layer."),
        Text::fromStaticLiteral("layer-z-scale"), translateCommandLiteral("Changed PSD layer depth scale"), true, true};
}
SetPsdDepthDialogLayerDepthScaleCommand::SetPsdDepthDialogLayerDepthScaleCommand()
    : SetPsdDepthDialogLayerDepthScaleCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerDepthScaleCommand::SetPsdDepthDialogLayerDepthScaleCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthScaleArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.stageForApply, arguments.mergeable} {}
void SetPsdDepthDialogLayerDepthScaleCommand::update(LayerState& state) { state.depthScale = value; }
SetPsdDepthDialogLayerDepthOffsetCommand::SourceConstructorArguments SetPsdDepthDialogLayerDepthOffsetCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Set PSD Layer Z Offset"), translateCommandLiteral("Set Z offset for the selected layer."),
        Text::fromStaticLiteral("layer-z-offset"), translateCommandLiteral("Changed PSD layer depth offset"), true, true};
}
SetPsdDepthDialogLayerDepthOffsetCommand::SetPsdDepthDialogLayerDepthOffsetCommand()
    : SetPsdDepthDialogLayerDepthOffsetCommand(sourceConstructorArguments()) {}
SetPsdDepthDialogLayerDepthOffsetCommand::SetPsdDepthDialogLayerDepthOffsetCommand(SourceConstructorArguments arguments)
    : PsdDepthDialogLayerValueCommand<SetPsdDepthDialogLayerDepthOffsetArguments>{arguments.label, arguments.description, arguments.key, arguments.changeLabel, arguments.stageForApply, arguments.mergeable} {}
void SetPsdDepthDialogLayerDepthOffsetCommand::update(LayerState& state) { state.depthOffset = value; }
ResetPsdDepthDialogLayerDepthTransformCommand::SourceConstructorArguments ResetPsdDepthDialogLayerDepthTransformCommand::sourceConstructorArguments() {
    return {translateCommandLiteral("Reset PSD Layer Z Transform"), translateCommandLiteral("Reset inversion, Z scale, and Z offset for the selected layer.")};
}
ResetPsdDepthDialogLayerDepthTransformCommand::ResetPsdDepthDialogLayerDepthTransformCommand()
    : ResetPsdDepthDialogLayerDepthTransformCommand(sourceConstructorArguments()) {}
ResetPsdDepthDialogLayerDepthTransformCommand::ResetPsdDepthDialogLayerDepthTransformCommand(SourceConstructorArguments arguments)
    : ExCommand<ResetPsdDepthDialogLayerDepthTransformArguments>{arguments.label, arguments.description} {}
bool ResetPsdDepthDialogLayerDepthTransformCommand::runnable(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        LayerState state;
        return dialog && dialog->dialogCommandsAvailable() &&
            dialog->captureDialogContextLayerState(ctx, layerPath, state);
    
}
ResultRef ResetPsdDepthDialogLayerDepthTransformCommand::run(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        LayerState oldState;
        if (!dialog || !dialog->captureDialogContextLayerState(ctx, layerPath, oldState))
            return CommandResult::opCall(false,
                Text::fromStaticLiteral("PSD depth dialog layer is not selected in Context"));
        auto nextState = oldState;
        nextState.invert = false;
        nextState.depthScale = 1.0f;
        nextState.depthOffset = 0.0f;
        if (!dialog->applyDialogLayerState(nextState, true)) return CommandResult::opCall(true);
        incActionPush(PsdDepthDialogLayerChangeAction::createSource([&] {
            auto actionDialog = dialog;
            auto actionOldState = oldState;
            auto actionNewState = nextState;
            Text actionKey;
            try {
                actionKey = join(Text::fromStaticLiteral("layer-z-reset:"), layerPath);
            } catch (const std::bad_alloc&) {
                throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                    std::source_location::current());
            } catch (const std::length_error&) {
                throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                    std::source_location::current());
            }
            auto actionLabel = translateCommandLiteral("Reset PSD layer depth transform");
            const auto actionStageForApply = true;
            const auto actionMergeable = false;
            return std::tuple{std::move(actionDialog), std::move(actionOldState), std::move(actionNewState),
                std::move(actionKey), std::move(actionLabel), actionStageForApply, actionMergeable};
        }));
        return CommandResult::opCall(true);
    
}
ApplyPsdDepthDialogCommand::SourcePresentation ApplyPsdDepthDialogCommand::sourceConstructorPresentation() {
    auto label = translateCommandLiteral("Apply PSD Depth Dialog");
    auto description = translateCommandLiteral("Apply the displayed PSD depth import dialog.");
    return {std::move(label), std::move(description)};
}
ApplyPsdDepthDialogCommand::ApplyPsdDepthDialogCommand()
    : ApplyPsdDepthDialogCommand(sourceConstructorPresentation()) {}
ApplyPsdDepthDialogCommand::ApplyPsdDepthDialogCommand(SourcePresentation presentation)
    : ExCommand<CommandNoArguments>{presentation.label, presentation.description} {}
bool ApplyPsdDepthDialogCommand::runnable(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        return dialog && dialog->dialogCommandsAvailable();
    
}
ResultRef ApplyPsdDepthDialogCommand::run(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false,
        Text::fromStaticLiteral("PSD depth import dialog is not displayed"));
    auto dialog = contextDialog(ctx);
    if (!actions::node::detail::requireActionObject(dialog)->applyDialogResult())
        return CommandResult::opCall(false, Text::fromStaticLiteral("PSD depth import dialog apply failed"));
    return CommandResult::opCall(true);
}
CancelPsdDepthDialogCommand::SourcePresentation CancelPsdDepthDialogCommand::sourceConstructorPresentation() {
    auto label = translateCommandLiteral("Cancel PSD Depth Dialog");
    auto description = translateCommandLiteral("Cancel the displayed PSD depth import dialog.");
    return {std::move(label), std::move(description)};
}
CancelPsdDepthDialogCommand::CancelPsdDepthDialogCommand()
    : CancelPsdDepthDialogCommand(sourceConstructorPresentation()) {}
CancelPsdDepthDialogCommand::CancelPsdDepthDialogCommand(SourcePresentation presentation)
    : ExCommand<CommandNoArguments>{presentation.label, presentation.description} {}
bool CancelPsdDepthDialogCommand::runnable(ContextRef ctx) {
        const auto keepAlive = weak_from_this().lock();
        auto dialog = contextDialog(ctx);
        return dialog && dialog->dialogCommandsAvailable();
    
}
ResultRef CancelPsdDepthDialogCommand::run(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false,
        Text::fromStaticLiteral("PSD depth import dialog is not displayed"));
    auto dialog = contextDialog(ctx);
    actions::node::detail::requireActionObject(dialog)->cancelDialog();
    return CommandResult::opCall(true);
}
std::shared_ptr<InspectPsdDepthDialogCommand::Result>
InspectPsdDepthDialogCommand::run(const ContextRef& ctx) {
    return runTyped(ctx);
}
std::shared_ptr<InspectPsdDepthDialogCommand::Result>
InspectPsdDepthDialogCommand::runTyped(ContextRef ctx) {
    const auto keepAlive = weak_from_this().lock();
    using namespace detail;
    using core::settings::SettingsString;
    if (!runnable(ctx)) return ExCommandResult<Json>::opCall(false, Json(nullptr),
        Text::fromStaticLiteral("PSD depth import dialog is not displayed"));
    auto state = compat::ngRequireObject(contextDialog(ctx))->captureDialogSettingsState();
    Object settings;
    settings[literalString("colorSourcePath")] = Json(SettingsString(state.settings.colorSourcePath));
    settings[literalString("invert")] = Json(state.settings.invert);
    settings[literalString("backDepth")] = Json(state.settings.backDepth);
    settings[literalString("frontDepth")] = Json(state.settings.frontDepth);
    settings[literalString("depthScale")] = Json(state.settings.depthScale);
    settings[literalString("channel")] = Json(SettingsString(enumText(state.settings.channel,
        io::depthmap_psd::ngPsdDepthChannelName, "PsdDepthChannel", io::depthmap_psd::PsdDepthChannel::Luminance)));
    settings[literalString("sampling")] = Json(SettingsString(enumText(state.settings.convolution,
        io::depthmap_psd::ngPsdDepthConvolutionName, "PsdDepthConvolution", io::depthmap_psd::PsdDepthConvolution::BackmostCustom)));
    settings[literalString("customRadius")] = Json(state.settings.customRadius);
    settings[literalString("alphaThreshold")] = Json(state.settings.alphaThreshold);
    settings[literalString("missingPolicy")] = Json(SettingsString(enumText(state.settings.missingPolicy,
        io::depthmap_psd::ngPsdDepthMissingPolicyName, "PsdDepthMissingPolicy", io::depthmap_psd::PsdDepthMissingPolicy::SkipGrid)));
    settings[literalString("contourRepair")] = Json(state.settings.repairContourBand);
    settings[literalString("surfaceSmoothing")] = Json(state.settings.smoothWavySurface);
    settings[literalString("gpuComposition")] = Json(state.settings.useGpuComposition);
    settings[literalString("directGridMatch")] = Json(state.settings.matchDirectGridName);
    settings[literalString("problemFilter")] = Json(state.onlyProblemLayers);
    Array layers;
    const auto capturedLayers = state.layers;
    for (auto layer : capturedLayers) {
        Object entry;
        entry[literalString("layerPath")] = Json(SettingsString(layer.layerPath));
        entry[literalString("targetGridUuid")] = Json(layer.targetGridUuid);
        entry[literalString("visible")] = Json(layer.visible);
        entry[literalString("enabled")] = Json(layer.enabled);
        entry[literalString("depthEnabled")] = Json(layer.depthEnabled);
        entry[literalString("depthInverted")] = Json(layer.invert);
        entry[literalString("depthOffset")] = Json(layer.depthOffset);
        entry[literalString("depthScale")] = Json(layer.depthScale);
        entry[literalString("outlierPruneEnabled")] = Json(layer.outlierPruneEnabled);
        entry[literalString("puppetFitEnabled")] = Json(layer.puppetFitEnabled);
        layers.append(Json(entry));
    }
    Object result; result[literalString("settings")] = Json(settings); result[literalString("layers")] = Json(layers);
    return ExCommandResult<Json>::opCall(true, Json(result));
}
thread_local CommandTable commands;
void ngInitPsdDepthDialogCommands() {
    initialization::Program::run<initialization::Table>();
}
}
