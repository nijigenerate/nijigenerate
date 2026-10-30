#include "control_commands.hpp"
#include "control.hpp"
#include "control_pair_commands.hpp"
#include "control_table.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/model/onionslice.hpp>
#include <nijigenerate/windows/flipconfig_window.hpp>
#include <nijigenerate/windows/automeshbatch.hpp>
#include <nijigenerate/widgets/modal/base.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <runtime_state.hpp>
#include <math/camera.hpp>
#include <algorithm>
#include <cmath>
#include <utility>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands::viewport::control {
namespace detail {
struct ControlCommandText { compat::RetainedString label; compat::RetainedString description; };
}
namespace view = nijigenerate::viewport::base;
namespace flip = windows::flipconfig;
namespace {
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
detail::ControlCommandText fitCommandText() {
    auto label = tr("Fit viewport to model.");
    auto description = tr("Reset viewport center and zoom so the current puppet bounds fit inside the viewport.");
    return {std::move(label), std::move(description)};
}
using Pairs = compat::Slice<std::shared_ptr<flip::FlipPair>>;
class FlipPairListChangeAction : public actions::Action {
    Pairs oldPairs, newPairs;
public:
    FlipPairListChangeAction(const FlipPairListChangeAction&) = delete;
    FlipPairListChangeAction& operator=(const FlipPairListChangeAction&) = delete;
    FlipPairListChangeAction(FlipPairListChangeAction&&) = delete;
    FlipPairListChangeAction& operator=(FlipPairListChangeAction&&) = delete;
    FlipPairListChangeAction(Pairs before, Pairs after) : oldPairs(before.duplicate()), newPairs(after.duplicate()) {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(FlipPairListChangeAction),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.commands.viewport.control.FlipPairListChangeAction"));
    }
    void rollback() override { flip::ngReplaceFlipPairs(oldPairs.duplicate()); }
    void redo() override { flip::ngReplaceFlipPairs(newPairs.duplicate()); }
    actions::ActionText describe() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Changed flip pairs"));
    }
    actions::ActionText describeUndo() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Reverted flip pairs"));
    }
    actions::ActionText getName() override { return compat::RetainedString::fromStaticLiteral("FlipPairListChangeAction"); }
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
    virtual compat::RetainedString toString() {
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
};
} // namespace
} // namespace nijigenerate::commands::viewport::control

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::viewport::control::FlipPairListChangeAction> {
    inline static constexpr char className[] =
        "nijigenerate.commands.viewport.control.FlipPairListChangeAction";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.viewport.control.FlipPairListChangeAction)";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::viewport::control::FlipPairListChangeAction> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.viewport.control.FlipPairListChangeAction)";
    inline static constexpr char unqualifiedArgument[] = "(FlipPairListChangeAction)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.viewport.control.FlipPairListChangeAction";
    inline static constexpr char unqualifiedElement[] = "FlipPairListChangeAction";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::viewport::control {
namespace {
void pushFlipPairActionIfChanged(Pairs before) {
    const auto after = flip::ngGetFlipPairs().duplicate();
    if (before.size() != after.size()) {
        core::actionstack::incActionPush(std::allocate_shared<FlipPairListChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<FlipPairListChangeAction>{}, before, after));
        return;
    }
    for (std::size_t i = 0; i < before.size(); ++i) {
        // Source foreach copies the current reference. Keep that same object
        // alive through the actual action-stack callback, even if it replaces
        // an entry in the shared input slice while pushing the action.
        const auto pair = before[i];
        if (pair != after[i]) {
            core::actionstack::incActionPush(std::allocate_shared<FlipPairListChangeAction>(
                nicxlive::core::d_runtime::SourceAllocator<FlipPairListChangeAction>{}, before, after));
            return;
        }
    }
}
}
#define NG_CONTROL_PREPARE(Type, Base) \
    void Type::prepareSourceClassInfo() { \
        Base::prepareSourceClassInfo(); \
        ::nijigenerate::commands::detail::prepareCommandResultClass<Type>(); \
    }
NG_CONTROL_PREPARE(ToggleMirrorViewCommand, ExCommand<>)
NG_CONTROL_PREPARE(ToggleOnionSliceCommand, ExCommand<>)
NG_CONTROL_PREPARE(TogglePhysicsCommand, ExCommand<>)
NG_CONTROL_PREPARE(TogglePostProcessCommand, ExCommand<>)
NG_CONTROL_PREPARE(ResetPhysicsCommand, ExCommand<>)
NG_CONTROL_PREPARE(ResetParametersCommand, ExCommand<>)
NG_CONTROL_PREPARE(OpenFlipPairWindowCommand, ExCommand<>)
NG_CONTROL_PREPARE(OpenAutomeshBatchingCommand, ExCommand<>)
NG_CONTROL_PREPARE(ResetViewportZoomCommand, ExCommand<>)
NG_CONTROL_PREPARE(ResetViewportPositionCommand, ExCommand<>)
NG_CONTROL_PREPARE(FitViewportToModelCommand, ExCommand<>)
NG_CONTROL_PREPARE(ListFlipPairsCommand, NativeBase)
NG_CONTROL_PREPARE(AddFlipPairCommand, ExCommand<AddFlipPairArguments>)
NG_CONTROL_PREPARE(AutoAddFlipPairsCommand, ExCommand<AutoAddFlipPairsArguments>)
NG_CONTROL_PREPARE(RemoveFlipPairCommand, ExCommand<RemoveFlipPairArguments>)
#undef NG_CONTROL_PREPARE
#define NG_REGISTER_SOURCE_CLASS(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control." #Type))
#define NG_CTOR(Type, Text) \
    compat::RetainedString Type::sourceConstructorDescription() { return tr(Text); } \
    Type::Type() : Type(sourceConstructorDescription()) {} \
    Type::Type(compat::RetainedString description) : ExCommand(std::move(description)) { \
        NG_REGISTER_SOURCE_CLASS(Type); \
    }
NG_CTOR(ToggleMirrorViewCommand, "Toggle mirror view.")
NG_CTOR(ToggleOnionSliceCommand, "Toggle onion slice overlay.")
NG_CTOR(TogglePhysicsCommand, "Toggle physics drivers.")
NG_CTOR(TogglePostProcessCommand, "Toggle post processing.")
NG_CTOR(ResetPhysicsCommand, "Reset physics.")
NG_CTOR(ResetParametersCommand, "Reset parameters to defaults.")
NG_CTOR(OpenFlipPairWindowCommand, "Open Flip Pair configuration window.")
NG_CTOR(OpenAutomeshBatchingCommand, "Open Automesh Batching modal.")
NG_CTOR(ResetViewportZoomCommand, "Reset viewport zoom to 1.0.")
NG_CTOR(ResetViewportPositionCommand, "Reset viewport position to origin.")
#undef NG_CTOR
compat::RetainedString ListFlipPairsCommand::sourceConstructorDescription() {
    return tr("List registered flip pairs.");
}
ListFlipPairsCommand::ListFlipPairsCommand() : ListFlipPairsCommand(sourceConstructorDescription()) {}
ListFlipPairsCommand::ListFlipPairsCommand(compat::RetainedString description) : NativeBase(std::move(description)) {
    NG_REGISTER_SOURCE_CLASS(ListFlipPairsCommand);
}
compat::RetainedString AddFlipPairCommand::sourceConstructorDescription() { return tr("Register a flip pair."); }
AddFlipPairCommand::AddFlipPairCommand() : AddFlipPairCommand(sourceConstructorDescription()) {}
AddFlipPairCommand::AddFlipPairCommand(compat::RetainedString description) : ExCommand({}, std::move(description)) {
    NG_REGISTER_SOURCE_CLASS(AddFlipPairCommand);
}
compat::RetainedString AutoAddFlipPairsCommand::sourceConstructorDescription() {
    return tr("Register flip pairs by name pattern.");
}
AutoAddFlipPairsCommand::AutoAddFlipPairsCommand() : AutoAddFlipPairsCommand(sourceConstructorDescription()) {}
AutoAddFlipPairsCommand::AutoAddFlipPairsCommand(compat::RetainedString description)
    : ExCommand({}, std::move(description)) {
    NG_REGISTER_SOURCE_CLASS(AutoAddFlipPairsCommand);
}
compat::RetainedString RemoveFlipPairCommand::sourceConstructorDescription() { return tr("Remove a flip pair."); }
RemoveFlipPairCommand::RemoveFlipPairCommand() : RemoveFlipPairCommand(sourceConstructorDescription()) {}
RemoveFlipPairCommand::RemoveFlipPairCommand(compat::RetainedString description) : ExCommand({}, std::move(description)) {
    NG_REGISTER_SOURCE_CLASS(RemoveFlipPairCommand);
}
detail::ControlCommandText FitViewportToModelCommand::sourceConstructorArguments() { return fitCommandText(); }
FitViewportToModelCommand::FitViewportToModelCommand() : FitViewportToModelCommand(sourceConstructorArguments()) {}
FitViewportToModelCommand::FitViewportToModelCommand(detail::ControlCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) {
    NG_REGISTER_SOURCE_CLASS(FitViewportToModelCommand);
}
#undef NG_REGISTER_SOURCE_CLASS
std::shared_ptr<CommandResult> ToggleMirrorViewCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    view::shouldMirrorViewport = !view::shouldMirrorViewport; return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ToggleOnionSliceCommand::run(std::shared_ptr<Context>) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    auto onion = nijigenerate::viewport::model::onionslice::OnionSlice::singleton();
    compat::ngRequireObject(onion)->toggle(); return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> TogglePhysicsCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet() || !ctx->puppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    const auto destination = ctx->puppet();
    const auto source = ctx->puppet();
    const bool enabled = !compat::ngRequireObject(source)->enableDrivers;
    compat::ngRequireObject(destination)->enableDrivers = enabled;
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> TogglePostProcessCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    view::shouldPostProcess = !view::shouldPostProcess; return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ResetPhysicsCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet() || !ctx->puppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    compat::ngRequireObject(ctx->puppet())->resetDrivers(); return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ResetParametersCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet() || !ctx->puppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    const auto puppet = compat::ngRequireObject(ctx->puppet());
    auto parameters = puppet->parameters;
    for (auto& parameter : parameters) {
        const auto receiver = compat::ngRequireObject(parameter);
        receiver->value = receiver->defaults;
    }
    return CommandResult::opCall(true);
}
std::shared_ptr<ListFlipPairsCommand::Result> ListFlipPairsCommand::runTyped(std::shared_ptr<Context> ctx) {
    const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet()) return Result::opCall(false, {},
        compat::RetainedString::fromStaticLiteral("No puppet"));
    return Result::opCall(true, core::settings::SettingsJsonValue(flip::ngListFlipPairsJson()));
}
std::shared_ptr<CommandResult> AddFlipPairCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    const auto before = flip::ngGetFlipPairs().duplicate(); const auto result = flip::ngAddFlipPair(left, right);
    if (result.succeeded) pushFlipPairActionIfChanged(before);
    return CommandResult::opCall(result.succeeded, result.message.retainedString());
}
std::shared_ptr<CommandResult> AutoAddFlipPairsCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet() || !ctx->puppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    const auto before = flip::ngGetFlipPairs().duplicate();
    const auto puppet = ctx->puppet();
    const auto left = leftPattern;
    const auto right = rightPattern;
    const auto result = flip::ngAutoAddFlipPairs(puppet, left, right);
    if (result.succeeded && result.added > 0) pushFlipPairActionIfChanged(before);
    const auto message = [&] {
        try {
            return compat::RetainedString(std::string(result.message.view()) + " (" +
                std::to_string(result.added) + " added)");
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    return CommandResult::opCall(result.succeeded, message);
}
std::shared_ptr<CommandResult> RemoveFlipPairCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    const auto before = flip::ngGetFlipPairs().duplicate(); const auto result = flip::ngRemoveFlipPair(left, right);
    if (result.succeeded) pushFlipPairActionIfChanged(before);
    return CommandResult::opCall(result.succeeded, result.message.retainedString());
}
std::shared_ptr<CommandResult> OpenFlipPairWindowCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    windows::base::incPushWindow(windows::base::ngCreateWindow<flip::FlipPairWindow>());
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> OpenAutomeshBatchingCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    if (!ctx->hasPuppet()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No puppet"));
    widgets::modal::ngModalAdd(std::allocate_shared<windows::automeshbatch::AutoMeshBatchWindow>(
        nicxlive::core::d_runtime::SourceAllocator<windows::automeshbatch::AutoMeshBatchWindow>{}));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ResetViewportZoomCommand::run(std::shared_ptr<Context>) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    view::viewportTargetZoom = 1; return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ResetViewportPositionCommand::run(std::shared_ptr<Context>) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    view::viewportTargetPosition = {0, 0}; return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> FitViewportToModelCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = weak_from_this().lock();
    ctx = compat::ngRequireObject(ctx);
    const auto puppet = ctx->hasPuppet() && ctx->puppet() ? ctx->puppet() : project::incActivePuppet();
    if (!puppet) return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No puppet"));
    int width, height; nicxlive::core::inGetViewport(width, height);
    if (width <= 0 || height <= 0) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("Viewport is empty"));
    puppet->update();
    const auto bounds = puppet->getCombinedBounds<true>();
    const float boundsWidth = bounds[2] - bounds[0], boundsHeight = bounds[3] - bounds[1];
    if (!std::isfinite(boundsWidth) || !std::isfinite(boundsHeight) || boundsWidth <= 0 || boundsHeight <= 0)
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("Puppet bounds are empty"));
    const float centerX = bounds[0] + (bounds[2] - bounds[0]) * 0.5f;
    const float centerY = bounds[1] + (bounds[3] - bounds[1]) * 0.5f;
    float zoom = std::min(static_cast<float>(width) / boundsWidth, static_cast<float>(height) / boundsHeight);
    zoom = std::clamp(zoom, static_cast<float>(view::viewportZoomMin), static_cast<float>(view::viewportZoomMax));
    const nicxlive::core::math::Vec2 position{-centerX, -centerY};
    const auto camera = nicxlive::core::inGetCameraHandle();
    compat::ngRequireObject(camera)->position = position;
    camera->scale = {zoom, zoom}; camera->rotation = 0;
    view::viewportTargetPosition = position; view::viewportTargetZoom = zoom; view::viewportZoom = zoom;
    return CommandResult::opCall(true);
}
void ngInitViewportControlCommands() {
    initialization::Program::run<initialization::Table>();
}
} // namespace nijigenerate::commands::viewport::control
