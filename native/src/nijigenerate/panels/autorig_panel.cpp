// Actual class bodies from independently authored panels/autorig.d; BSD-2-Clause.
#include "autorig.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <compat/phobos_file_read.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/puppet/tool_handlers.hpp>
#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/panels/resource.hpp>
#include <nijigenerate/autorig/deterministic/native.hpp>
#include <nijigenerate/autorig/deterministic/contracts_json.hpp>
#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nijigenerate/autorig/deterministic/templates.hpp>
#include <nijigenerate/autorig/deterministic/review.hpp>
#include <common/unicode_lowercase.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <initializer_list>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::panels::autorig {
namespace {

// Source callbacks are evaluated outside this primitive storage conversion.
template<class Build> decltype(auto) withStringAllocation(Build&& build) {
    try {
        return std::forward<Build>(build)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

compat::RetainedString translateText(compat::RetainedString text) {
    return core::translation::ngTranslateRetained(std::move(text));
}

template<std::size_t N> compat::RetainedString translateText(const char (&text)[N]) {
    return translateText(compat::RetainedString::fromStaticLiteral(text));
}

template<class Array> decltype(auto) sourceElement(Array& values, std::size_t index, std::size_t line) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), line);
    return values[index];
}

template<class Map, class Key> auto sourceMapValue(const Map& values, const Key& key, std::size_t line) {
    const auto found = values.find(key);
    if (found == values.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), line);
    return found->second;
}

// Keep the real owned copy alive over the C call, as source toStringz requires.
compat::SourceCString cStringCopy(compat::RetainedStringSlice text) {
    auto result = compat::SourceCString(text);
    compat::detail::ngSourceCStringPostconditions(text.view(), result.data());
    return result;
}

compat::SourceCString cStringCopy(std::string text) {
    auto result = compat::SourceCString(std::string_view(text));
    compat::detail::ngSourceCStringPostconditions(text, result.data());
    return result;
}

const char* cStringData(const compat::SourceCString& text) {
    return text.data();
}

std::string valueKindText(::nijigenerate::autorig::framework::AutoRigValueKind kind) {
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    using namespace core::translation;
    FormatEnum value;
    value.base = std::get<FormatInteger>(ngFormatArgument(static_cast<std::int32_t>(kind)));
    value.typeName = compat::RetainedString::fromStaticLiteral("AutoRigValueKind");
    switch (kind) {
    case Kind::FileName: value.memberName = compat::RetainedString::fromStaticLiteral("FileName"); break;
    case Kind::Path: value.memberName = compat::RetainedString::fromStaticLiteral("Path"); break;
    case Kind::Json: value.memberName = compat::RetainedString::fromStaticLiteral("Json"); break;
    case Kind::Blob: value.memberName = compat::RetainedString::fromStaticLiteral("Blob"); break;
    }
    const std::array<TypedFormatArgument, 1> arguments{value};
    return ngFormatTyped("%s", arguments);
}

compat::RetainedString workflowStateText(::nijigenerate::autorig::workflow::AutoRigWorkflowState state) {
    using State = ::nijigenerate::autorig::workflow::AutoRigWorkflowState;
    using namespace core::translation;
    FormatEnum value;
    value.base = std::get<FormatInteger>(ngFormatArgument(static_cast<std::int32_t>(state)));
    value.typeName = compat::RetainedString::fromStaticLiteral("AutoRigWorkflowState");
    switch (state) {
    case State::Pending: value.memberName = compat::RetainedString::fromStaticLiteral("Pending"); break;
    case State::Running: value.memberName = compat::RetainedString::fromStaticLiteral("Running"); break;
    case State::Succeeded: value.memberName = compat::RetainedString::fromStaticLiteral("Succeeded"); break;
    case State::Failed: value.memberName = compat::RetainedString::fromStaticLiteral("Failed"); break;
    case State::Canceled: value.memberName = compat::RetainedString::fromStaticLiteral("Canceled"); break;
    }
    const std::array<TypedFormatArgument, 1> arguments{value};
    return compat::RetainedString(ngFormatTyped("%s", arguments));
}

compat::RetainedString taskStateText(::nijigenerate::autorig::framework::AutoRigTaskState state) {
    using State = ::nijigenerate::autorig::framework::AutoRigTaskState;
    using namespace core::translation;
    FormatEnum value;
    value.base = std::get<FormatInteger>(ngFormatArgument(static_cast<std::int32_t>(state)));
    value.typeName = compat::RetainedString::fromStaticLiteral("AutoRigTaskState");
    switch (state) {
    case State::Pending: value.memberName = compat::RetainedString::fromStaticLiteral("Pending"); break;
    case State::Running: value.memberName = compat::RetainedString::fromStaticLiteral("Running"); break;
    case State::Succeeded: value.memberName = compat::RetainedString::fromStaticLiteral("Succeeded"); break;
    case State::Failed: value.memberName = compat::RetainedString::fromStaticLiteral("Failed"); break;
    case State::Canceled: value.memberName = compat::RetainedString::fromStaticLiteral("Canceled"); break;
    case State::Stale: value.memberName = compat::RetainedString::fromStaticLiteral("Stale"); break;
    }
    const std::array<TypedFormatArgument, 1> arguments{value};
    return compat::RetainedString(ngFormatTyped("%s", arguments));
}

} // namespace

std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowManager> AutoRigPanel::workflowManager() {
    const auto owner = weak_from_this().lock();
    if (!workflows) {
        using Manager = ::nijigenerate::autorig::workflow::AutoRigWorkflowManager;
        std::allocator<Manager> allocator;
        Manager* storage;
        try {
            storage = allocator.allocate(1);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        try {
            std::construct_at(storage, ngAutoRigSessionManager());
        } catch (...) {
            allocator.deallocate(storage, 1);
            throw;
        }
        const auto destroy = [](Manager* value) noexcept {
            std::destroy_at(value);
            std::allocator<Manager>{}.deallocate(value, 1);
        };
        try {
            // shared_ptr invokes this same deleter if its control allocation fails.
            workflows = std::shared_ptr<Manager>(storage, destroy);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    return workflows;
}

compat::RetainedStringSlice AutoRigPanel::presetLabel(
        ::nijigenerate::autorig::workflow::AutoRigWorkflowPreset preset) {
    [[maybe_unused]] const auto self = weak_from_this().lock();
    const auto owner = compat::ngRequireObject(ngAutoRigSessionManager())->processor(preset.providerId);
    const auto title = core::translation::ngTranslateRetained(compat::ngRequireObject(owner)->displayName());
    auto label = withStringAllocation([&] {
        std::string value(title.view());
        value += " / ";
        return value;
    });
    if (!preset.spec.label.empty()) {
        const auto suffix = core::translation::ngTranslateRetained(preset.spec.label);
        withStringAllocation([&] { label += suffix.view(); });
    }
    else withStringAllocation([&] { label += preset.spec.id.view(); });
    return withStringAllocation([&] { return compat::RetainedString(std::move(label)); });
}

compat::RetainedStringSlice AutoRigPanel::draftKey(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId, compat::RetainedStringSlice portId) {
    const auto runId = compat::ngRequireObject(run)->id();
    try {
        std::string key(runId.view());
        key += '/';
        key += taskId.view();
        key += '/';
        key += portId.view();
        return compat::RetainedString(std::move(key));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

AutoRigPanel::AutoRigPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("AutoRig"),
        translateText("AutoRig"), true) {
    supportsReadOnlyInteraction = true;
    activeModes = project::EditMode::ModelEdit;
}

void AutoRigPanel::prepareSourceClassInfo() {
    Panel::prepareSourceClassInfo();
    static const bool registered = [] {
        using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<AutoRigPanel>;
        const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
        nicxlive::core::d_runtime::ngRegisterObjectTypeNames<AutoRigPanel>(name,
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AutoRigPanel), name);
        return true;
    }();
    (void)registered;
}

void AutoRigPanel::restoreDriverState() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto puppet = project::incActivePuppet();
    if (puppet && compat::ngRequireObject(puppet->root)->getUUID() == controlledRoot && !project::incArmedParameter()) {
        puppet->enableDrivers = previousDriversEnabled;
    }
    controlledRoot = 0;
}

void AutoRigPanel::onInit() {
    using namespace commands::puppet::tool;
    using Json = ::nijigenerate::autorig::framework::Json;
    const auto self = shared_from_this();
    AutoRigExecuteHandler execute(
        [self](Json options, Json context) { return self->executeImportedModel(options, context); });
    AutoRigStatusHandler status(
        [self](compat::RetainedStringSlice runId) { return self->runStatus(runId); });
    AutoRigResumeHandler resume(
        [self](compat::RetainedStringSlice runId, compat::RetainedStringSlice stepId) {
            return self->resumeWorkflow(runId, stepId);
        });
    AutoRigRemoveHandler remove(
        [self](compat::RetainedStringSlice runId) { self->removeRun(runId); });
    ngSetAutoRigCommandHandlers(execute, status, resume, remove);
}

void AutoRigPanel::onUpdate() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    finishWorker();
    renderPresetPicker();
    compat::RetainedStringSlice error;
    {
        const std::lock_guard lock(stateMonitor);
        error = lastError;
    }
    if (!error.empty()) {
        const auto text = cStringCopy(error);
        ImGui::TextWrapped("%s", cStringData(text));
    }
    ImGui::Separator();
    compat::RetainedStringSlice removedRun;
    if (ImGui::BeginChild("##AutoRigSessions", ImVec2(0, 0))) {
        const auto currentRuns = runs;
        for (auto run : currentRuns) {
            if (renderRun(run)) removedRun = compat::RetainedString(compat::ngRequireObject(run)->id());
        }
    }
    ImGui::EndChild();
    if (!removedRun.empty()) removeRun(removedRun);
}

void AutoRigPanel::removeRun(compat::RetainedStringSlice runId) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    compat::ngRequireObject(workflowManager())->remove(runId.retainedString());
    ::nijigenerate::autorig::deterministic::native::ngRigForgetNativeSession(runId.retainedString());
    compat::Slice<std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun>> remaining;
    const auto currentRuns = runs;
    for (auto run : currentRuns) {
        if (compat::ngRequireObject(run)->id().view() != runId.view()) remaining.append(run);
    }
    runs = remaining;
    materialDrafts.erase(runId);
    const auto eraseChildren = [&runId](auto& drafts) {
        compat::Slice<compat::RetainedStringSlice> keys;
        for (const auto& entry : drafts) keys.append(entry.first);
        for (auto key : keys) {
            const auto prefix = withStringAllocation([&] {
                std::string value(runId.view());
                value += '/';
                return value;
            });
            if (key.view().starts_with(prefix)) drafts.erase(key);
        }
    };
    eraseChildren(reviewDrafts);
    contextDrafts.erase(runId);
    workflowOptionsDrafts.erase(runId);
    eraseChildren(inputDrafts);
    eraseChildren(inputObserved);
    eraseChildren(inputTextCache);
    reclaimSessionMemory = true;
}

compat::RetainedStringSlice AutoRigPanel::resumeWorkflow(
        compat::RetainedStringSlice runId, compat::RetainedStringSlice stepId) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    finishWorker();
    if (worker != nullptr) {
        throw compat::ThrowableException(compat::RetainedString(std::string_view(
            "An AutoRig workflow is already running")),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 916);
    }
    auto run = compat::ngRequireObject(workflowManager())->reopen(runId.retainedString());
    bool present = false;
    const auto currentRuns = runs;
    for (auto existing : currentRuns) {
        const auto existingId = compat::ngRequireObject(existing)->id();
        if (existingId.view() == compat::ngRequireObject(run)->id().view()) present = true;
    }
    if (!present) runs.append(run);
    startRun(run, stepId, false);
    return compat::ngRequireObject(run)->id();
}

compat::RetainedStringSlice AutoRigPanel::executeImportedModel(
        ::nijigenerate::autorig::framework::Json options,
        ::nijigenerate::autorig::framework::Json context) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    namespace framework = ::nijigenerate::autorig::framework;
    finishWorker();
    if (worker != nullptr) {
        throw compat::ThrowableException(compat::RetainedString(std::string_view(
            "An AutoRig workflow is already running")),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 926);
    }
    const auto manager = compat::ngRequireObject(workflowManager());
    framework::Table<framework::AutoRigValue> inputs;
    inputs[compat::RetainedString::fromStaticLiteral("options")] = framework::AutoRigValue::jsonValue(options);
    auto run = manager->createConfigured(
        compat::RetainedString::fromStaticLiteral("anime-front-view-rig"),
        compat::RetainedString::fromStaticLiteral("model-to-rig"), inputs, context);
    runs.append(run);
    startRun(run, {}, false);
    return compat::ngRequireObject(run)->id();
}

::nijigenerate::autorig::framework::Json AutoRigPanel::runStatus(compat::RetainedStringSlice runId) {
    using Json = ::nijigenerate::autorig::framework::Json;
    using JsonObject = core::settings::SettingsJsonObject;
    using JsonArray = core::settings::SettingsJsonArray;
    using JsonString = core::settings::SettingsString;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    finishWorker();
    // The selected C++ backend has no GC heap or free GC pools. These legacy
    // fields describe that absent storage, never process or session memory.
    constexpr std::uint64_t gcUsedBytes = 0;
    constexpr std::uint64_t gcFreeBytes = 0;
    if (runId.empty()) {
        std::uint64_t jsonBytes = 0, blobBytes = 0, cpuBytes = 0;
        bool busy = false;
        const auto currentRuns = runs;
        for (auto run : currentRuns) {
            const auto info = compat::ngRequireObject(compat::ngRequireObject(run)->session())->memoryInfo();
            jsonBytes += info.sourceIndexValue("json_bytes").as_uint64();
            blobBytes += info.sourceIndexValue("blob_bytes").as_uint64();
            cpuBytes += info.sourceIndexValue("cpu_bytes").as_uint64();
            busy = busy || compat::ngRequireObject(run->session())->isBusy();
        }
        JsonObject memory;
        memory["json_bytes"] = Json(jsonBytes);
        memory["blob_bytes"] = Json(blobBytes);
        memory["cpu_bytes"] = Json(cpuBytes);
        memory["gc_used_bytes"] = Json(gcUsedBytes);
        memory["gc_free_bytes"] = Json(gcFreeBytes);
        JsonObject result;
        result["state"] = Json(JsonString(busy ? compat::RetainedString::fromStaticLiteral("Running")
            : compat::RetainedString::fromStaticLiteral("Idle")));
        result["memory"] = Json(memory);
        return Json(result);
    }
    const auto currentRuns = runs;
    for (auto run : currentRuns) if (compat::ngRequireObject(run)->id().view() == runId.view()) {
        const auto snapshot = run->snapshot();
        JsonArray steps;
        const auto ordered = run->orderedSteps();
        for (auto step : ordered) {
            const auto state = compat::ngRequireObject(run->session())->task(run->stepTaskId(step.id.retainedString()));
            JsonObject item;
            item["id"] = Json(JsonString(step.id));
            item["state"] = Json(JsonString(taskStateText(state.state)));
            item["attempt"] = Json(state.attempt);
            item["message"] = Json(JsonString(state.message));
            steps.append(Json(item));
        }
        auto memory = compat::ngRequireObject(run->session())->memoryInfo();
        memory.sourceIndexAssignSlot("gc_used_bytes") = Json(gcUsedBytes);
        memory.sourceIndexAssignSlot("gc_free_bytes") = Json(gcFreeBytes);
        JsonObject members;
        members["run_id"] = Json(JsonString(runId));
        members["state"] = Json(JsonString(workflowStateText(snapshot.state)));
        members["message"] = Json(JsonString(snapshot.message));
        members["steps"] = Json(steps);
        members["memory"] = memory;
        Json result(members);
        const auto puppet = project::incActivePuppet();
        JsonObject editor;
        editor["panel_visible"] = Json(visible);
        editor["worker_active"] = Json(worker != nullptr);
        editor["commands_blocked"] = Json(api::mcp::task::ngMcpExternalCommandsBlocked());
        editor["drivers_enabled"] = Json(puppet && puppet->enableDrivers);
        editor["armed_parameter"] = Json(project::incArmedParameter()
            ? compat::ngRequireObject(project::incArmedParameter())->getUUID() : std::uint32_t{0});
        result.sourceIndexAssignSlot("editor") = Json(editor);
        result.sourceIndexAssignSlot("resource_view") = resource::ngResourcePanelReadback();
        const auto diagnostics = ::nijigenerate::autorig::workflow::ngAutoRigWorkflowDiagnostics(run).sourceObjectRef();
        for (const auto& entry : diagnostics) {
            const auto key = entry.name;
            const auto value = entry.value();
            result.sourceIndexAssignSlot(key) = value;
        }
        return result;
    }
    std::string message("Unknown AutoRig workflow run: ");
    message += runId.view();
    throw compat::ThrowableException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 977);
}

void AutoRigPanel::renderPresetPicker() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto presets = compat::ngRequireObject(workflowManager())->listPresets();
    if (presets.size() > 1) {
        std::sort(presets.begin(), presets.end(), [](const auto& a, const auto& b) {
            if (a.spec.preferred != b.spec.preferred) return a.spec.preferred;
            return a.providerId.view() < b.providerId.view() ||
                (a.providerId.view() == b.providerId.view() && a.spec.id.view() < b.spec.id.view());
        });
    }
    bool selectionFound = false;
    for (const auto preset : presets) {
        if (preset.providerId.view() == selectedProviderId.view() &&
                preset.spec.id.view() == selectedWorkflowId.view()) selectionFound = true;
    }
    if (!selectionFound) {
        selectedProviderId = presets.empty() ? compat::RetainedStringSlice{} : sourceElement(presets, 0, 191).providerId;
        selectedWorkflowId = presets.empty() ? compat::RetainedStringSlice{} : sourceElement(presets, 0, 192).spec.id;
    }
    compat::RetainedStringSlice selectedLabel = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("No workflows registered"));
    compat::RetainedStringSlice description;
    for (const auto preset : presets) {
        if (preset.providerId.view() == selectedProviderId.view() &&
                preset.spec.id.view() == selectedWorkflowId.view()) {
            selectedLabel = presetLabel(preset);
            description = core::translation::ngTranslateRetained(preset.spec.description);
        }
    }
    ImGui::BeginDisabled(presets.empty() || worker != nullptr);
    if (widgets::ngButtonColored("+", ImVec2(24, 24))) {
        try {
            auto run = compat::ngRequireObject(workflowManager())->create(
                selectedProviderId.retainedString(), selectedWorkflowId.retainedString());
            runs.append(run);
            const std::lock_guard lock(stateMonitor);
            lastError.reset();
        } catch (const compat::ThrowableException& error) {
            const std::lock_guard lock(stateMonitor);
            lastError = error.msg;
        }
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        const auto tooltip = cStringCopy(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Add workflow session"))));
        ImGui::SetTooltip("%s", cStringData(tooltip));
    }
    ImGui::SameLine();
    const auto space = ImGui::GetContentRegionAvail();
    ImGui::SetNextItemWidth(space.x > 40 ? space.x - 34 : 1);
    const auto preview = cStringCopy(selectedLabel);
    if (ImGui::BeginCombo("##AutoRigWorkflow", cStringData(preview))) {
        for (const auto preset : presets) {
            const auto presetText = presetLabel(preset);
            auto label = withStringAllocation([&] {
                std::string value(presetText.view());
                value += "##";
                value += preset.providerId.view();
                value += '/';
                value += preset.spec.id.view();
                return value;
            });
            const bool selected = preset.providerId.view() == selectedProviderId.view() &&
                preset.spec.id.view() == selectedWorkflowId.view();
            const auto text = cStringCopy(std::move(label));
            if (ImGui::Selectable(cStringData(text), selected)) {
                selectedProviderId = preset.providerId;
                selectedWorkflowId = preset.spec.id;
            }
        }
        ImGui::EndCombo();
    }
    if (!description.empty()) {
        const auto text = cStringCopy(description);
        ImGui::TextWrapped("%s", cStringData(text));
    }
}

void AutoRigPanel::renderTask(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        ::nijigenerate::autorig::framework::AutoRigWorkflowStep step) {
    using State = ::nijigenerate::autorig::framework::AutoRigTaskState;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto taskId = compat::ngRequireObject(run)->stepTaskId(step.id);
    const auto task = compat::ngRequireObject(compat::ngRequireObject(run)->session())->task(taskId, false);
    const auto state = task.state;
    const auto stepId = cStringCopy(step.id);
    ImGui::PushID(cStringData(stepId));
    const auto spec = compat::ngRequireObject(compat::ngRequireObject(run)->session())->taskSpec(taskId);
    const compat::RetainedStringSlice title = !spec.label.empty()
        ? core::translation::ngTranslateRetained(spec.label) : step.id;
    // The original Thread carrier is required here; a separate busy flag would
    // not preserve the source's worker-reference lifetime or null semantics.
    ImGui::BeginDisabled(worker != nullptr);
    if (widgets::ngButtonColored("\xee\x80\xb7", ImVec2(24, 24))) startRun(run, step.id);
    ImGui::EndDisabled();
    ImGui::SameLine();
    core::translation::FormatEnum formattedState;
    formattedState.base = std::get<core::translation::FormatInteger>(
        core::translation::ngFormatArgument(static_cast<std::int32_t>(state)));
    formattedState.typeName = compat::RetainedString::fromStaticLiteral("AutoRigTaskState");
    switch (state) {
    case State::Pending: formattedState.memberName = compat::RetainedString::fromStaticLiteral("Pending"); break;
    case State::Running: formattedState.memberName = compat::RetainedString::fromStaticLiteral("Running"); break;
    case State::Succeeded: formattedState.memberName = compat::RetainedString::fromStaticLiteral("Succeeded"); break;
    case State::Failed: formattedState.memberName = compat::RetainedString::fromStaticLiteral("Failed"); break;
    case State::Canceled: formattedState.memberName = compat::RetainedString::fromStaticLiteral("Canceled"); break;
    case State::Stale: formattedState.memberName = compat::RetainedString::fromStaticLiteral("Stale"); break;
    }
    const std::array<core::translation::TypedFormatArgument, 1> arguments{formattedState};
    const auto stateText = compat::RetainedString(core::translation::ngFormatTyped("%s", arguments));
    if (renderStateTree(title, stateText, compat::RetainedString::fromStaticLiteral("task"),
            ImGuiTreeNodeFlags_None)) {
        // Source __() already returns a C-string; unlike _.toStringz(), this
        // boundary must not make a second SourceCString allocation.
        const auto inputLabel = core::translation::ngTranslateCString("Input");
        if (ImGui::TreeNodeEx(inputLabel.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginDisabled(worker != nullptr);
            if (step.taskId.view() == "compile-rig") renderMaterialRoles(run, taskId);
            else if (!compat::ngRequireObject(compat::ngRequireObject(run)->session())->renderInputUI(taskId)) {
                renderDefaultInputs(run, taskId);
            }
            ImGui::EndDisabled();
            ImGui::TreePop();
        }
        const auto outputLabel = core::translation::ngTranslateCString("Output");
        if (ImGui::TreeNodeEx(outputLabel.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!renderRigOutput(run, taskId) &&
                    !compat::ngRequireObject(compat::ngRequireObject(run)->session())->renderOutputUI(taskId)) {
                renderDefaultOutputs(run, taskId);
            }
            ImGui::TreePop();
        }
        const auto message = task.message;
        if (!message.empty()) {
            const auto text = cStringCopy(message);
            ImGui::TextWrapped("%s", cStringData(text));
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void AutoRigPanel::renderDefaultInputs(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId) {
    using namespace ::nijigenerate::autorig;
    using Kind = framework::AutoRigValueKind;
    using core::translation::ngTranslateCString;
    const auto owner = weak_from_this().lock();
    auto context = compat::ngRequireObject(compat::ngRequireObject(run)->session())->inputContext(
        taskId.retainedString());
    const auto ports = compat::ngRequireObject(context)->ports();
    for (const auto port : ports) {
        const bool builtIn = run->workflowId().view() == "model-to-rig";
        if (builtIn && port.id.view() == "options") {
            const auto text = cStringCopy(translateText(
                "Source capture is configured in workflow arguments; classification is edited in the compile task."));
            ImGui::TextWrapped("%s", cStringData(text));
            continue;
        }
        if (port.id.view() == "review") {
            const auto text = cStringCopy(translateText(
                "Operation settings are edited beside the task result below."));
            ImGui::TextWrapped("%s", cStringData(text));
            continue;
        }
        const auto portId = cStringCopy(port.id);
        ImGui::PushID(cStringData(portId));
        compat::RetainedStringSlice label;
        if (!builtIn) {
            auto bytes = withStringAllocation([&] {
                std::string value(port.id.view());
                value += " (";
                return value;
            });
            const auto kindLabel = valueKindLabel(port.kind);
            label = withStringAllocation([&] {
                bytes += kindLabel.view();
                bytes += ')';
                return compat::RetainedString(std::move(bytes));
            });
        } else if (port.id.view() == "state") label = translateText("Upstream processing result");
        else if (port.id.view() == "program") label = translateText("Rig plan");
        else if (port.id.view() == "model") label = translateText("Model checkpoint");
        else if (port.id.view() == "materials") label = translateText("Material classification");
        else label = port.id;
        const auto labelText = cStringCopy(label);
        ImGui::TextUnformatted(cStringData(labelText));
        if (!port.description.empty()) {
            const auto text = cStringCopy(translateText(port.description));
            ImGui::TextUnformatted(cStringData(text));
        }
        if (context->isConnected(port.id.retainedString())) {
            compat::RetainedStringSlice value;
            if (!context->hasValue(port.id.retainedString()))
                value = translateText("Waiting for dependency");
            else if (port.kind == Kind::Json) value = translateText(builtIn
                ? compat::RetainedString::fromStaticLiteral("Input from preceding task")
                : compat::RetainedString::fromStaticLiteral("JSON artifact ready"));
            else if (port.kind == Kind::Blob) value = translateText("Binary artifact");
            else value = inputText(context->value(port.id.retainedString()));
            const auto text = cStringCopy(value);
            ImGui::TextUnformatted(cStringData(text));
            const auto connections = compat::ngRequireObject(run->session())->taskSpec(
                taskId.retainedString()).connections;
            for (const auto connection : connections) {
                if (connection.input != port.id) continue;
                const auto source = compat::ngRequireObject(run->session())->task(
                    connection.sourceTask.retainedString(), false);
                const auto sourceSpec = compat::ngRequireObject(run->session())->taskSpec(
                    connection.sourceTask.retainedString());
                const auto pattern = translateText("From %s / %s (attempt %s)");
                const std::array<core::translation::TypedFormatArgument, 3> arguments{
                    core::translation::ngFormatArgument(translateText(sourceSpec.label)),
                    core::translation::ngFormatArgument(connection.sourceOutput),
                    core::translation::ngFormatArgument(source.attempt)};
                const auto from = cStringCopy(core::translation::ngFormatTyped(pattern.view(), arguments));
                ImGui::TextWrapped("%s", cStringData(from));
                bool hasReview = false;
                {
                    const auto outputs = sourceSpec.outputs;
                    for (const auto output : outputs) if (output.id.view() == "review") hasReview = true;
                }
                if (hasReview) {
                    const auto preceding = ngTranslateCString("Preceding stage result");
                    if (ImGui::TreeNode(preceding.data())) {
                        const auto cacheKey = draftKey(run, connection.sourceTask,
                            compat::RetainedString::fromStaticLiteral("input-review"));
                        auto found = reviewDrafts.find(cacheKey);
                        auto* cached = found == reviewDrafts.end() ? nullptr : &found->second;
                        if (worker == nullptr && (!cached || cached->attempt != source.attempt)) {
                            ReviewDraft draft;
                            draft.attempt = source.attempt;
                            const auto artifacts = compat::ngRequireObject(run->session())->task(
                                connection.sourceTask.retainedString()).artifacts;
                            for (const auto artifact : artifacts) {
                                if (artifact.portId.view() == "review" || artifact.portId.view() == "review-layout") {
                                    const auto report = framework::ngCopyAutoRigValue(artifact.value()).json;
                                    draft.tables.appendRange(deterministic::presentation::ngRigReviewTables(report, {},
                                        [](compat::RetainedStringSlice message) -> compat::RetainedStringSlice {
                                            return translateText(message.retainedString());
                                        }));
                                }
                            }
                            reviewDrafts[cacheKey] = draft;
                            found = reviewDrafts.find(cacheKey);
                            cached = found == reviewDrafts.end() ? nullptr : &found->second;
                        }
                        if (cached && !cached->tables.empty()) renderReviewTables(cached->tables);
                        else {
                            const auto waiting = cStringCopy(translateText("Waiting for task result"));
                            ImGui::TextUnformatted(cStringData(waiting));
                        }
                        ImGui::TreePop();
                    }
                }
            }
        } else {
            const auto key = draftKey(run, taskId, port.id);
            const auto revision = compat::ngRequireObject(run->session())->inputRevision(taskId.retainedString());
            compat::SharedDelegate<compat::RetainedStringSlice()> load(
                [this, &context, &port]() -> compat::RetainedStringSlice {
                    return context->hasValue(port.id.retainedString())
                        ? inputText(context->value(port.id.retainedString()))
                        : compat::RetainedStringSlice(compat::RetainedString::emptyLiteral());
                });
            const auto current = cachedInputText(key, revision, load);
            if (!inputDrafts.contains(key) || (inputObserved.contains(key) &&
                    sourceMapValue(inputObserved, key, 445) != current &&
                    sourceMapValue(inputDrafts, key, 446) == sourceMapValue(inputObserved, key, 446))) {
                inputDrafts[key] = current;
            }
            inputObserved[key] = current;
            auto draft = sourceMapValue(inputDrafts, key, 449);
            ImGui::BeginDisabled(!context->canEdit(port.id.retainedString()));
            widgets::ngInputText("##value", draft);
            inputDrafts[key] = draft;
            const auto apply = ngTranslateCString("Apply");
            if (widgets::ngButtonColored(apply.data())) {
                try {
                    applyInput(context, port, draft);
                    const std::lock_guard lock(stateMonitor);
                    lastError.reset();
                } catch (const compat::ThrowableException& error) {
                    const std::lock_guard lock(stateMonitor);
                    lastError = error.msg;
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::PopID();
    }
}

bool AutoRigPanel::renderRigOutput(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId) {
    using namespace ::nijigenerate::autorig;
    using namespace deterministic::presentation;
    using namespace deterministic::contracts;
    using core::translation::ngTranslateCString;
    using Json = framework::Json;
    using JsonArray = core::settings::SettingsJsonArray;
    using JsonObject = core::settings::SettingsJsonObject;
    using JsonString = core::settings::SettingsString;
    const auto owner = weak_from_this().lock();
    const auto spec = compat::ngRequireObject(compat::ngRequireObject(run)->session())->taskSpec(
        taskId.retainedString());
    bool supported = false;
    {
        const auto outputs = spec.outputs;
        for (const auto port : outputs) if (port.id.view() == "review") supported = true;
    }
    if (!supported) return false;
    const auto task = compat::ngRequireObject(run->session())->task(taskId.retainedString(), false);
    const auto key = draftKey(run, taskId, compat::RetainedString::fromStaticLiteral("review"));
    auto found = reviewDrafts.find(key);
    auto* cached = found == reviewDrafts.end() ? nullptr : &found->second;
    if (worker == nullptr && (!cached || cached->attempt != task.attempt)) {
        ReviewDraft draft;
        draft.attempt = task.attempt;
        RigReviewMaterialNames materialNames;
        if (run->workflowId().view() == "model-to-rig") {
            const auto sourceId = run->stepTaskId(compat::RetainedString::fromStaticLiteral("source"));
            if (compat::ngRequireObject(run->session())->hasOutput(
                    sourceId, compat::RetainedString::fromStaticLiteral("review"))) {
                materialNames = ngRigReviewMaterialNames(compat::ngRequireObject(run->session())->output(
                    sourceId, compat::RetainedString::fromStaticLiteral("review")).json);
            }
        }
        const auto artifacts = compat::ngRequireObject(run->session())->task(taskId.retainedString()).artifacts;
        bool hasReport = false;
        for (const auto artifact : artifacts) if (artifact.portId.view() == "report") hasReport = true;
        for (const auto artifact : artifacts) {
            const auto port = artifact.portId.view();
            if (port == "report" || port == "review-layout" || (port == "review" && !hasReport)) {
                const auto report = framework::ngCopyAutoRigValue(artifact.value()).json;
                using Names = RigReviewMaterialNames::element_type;
                const auto names = materialNames && !materialNames->empty()
                    ? std::allocate_shared<Names>(nicxlive::core::d_runtime::SourceAllocator<Names>{},
                        *materialNames) : RigReviewMaterialNames{};
                draft.tables.appendRange(ngRigReviewTables(report, names,
                    [](compat::RetainedStringSlice message) -> compat::RetainedStringSlice {
                        return core::translation::ngTranslateRetained(message.retainedString());
                    }));
            }
            if (port == "review-operations") {
                const auto operations = framework::ngCopyAutoRigValue(artifact.value()).json.sourceArrayValue();
                for (auto operation : operations) {
                    draft.operationIds.append(operation.sourceIndexValue("id").as_string());
                    auto plan = operation.sourceIndexValue("plan");
                    std::string targets;
                    const std::array<compat::RetainedStringSlice, 3> fields{
                        compat::RetainedString::fromStaticLiteral("part"),
                        compat::RetainedString::fromStaticLiteral("source"),
                        compat::RetainedString::fromStaticLiteral("target")};
                    for (const auto field : fields) {
                        auto& members = plan.sourceObjectRef();
                        if (const auto* target = members.if_contains(field.view())) {
                            const auto id = target->is_uint64() ? target->as_uint64()
                                : static_cast<std::uint64_t>(target->as_int64());
                            const compat::RetainedStringSlice* name = nullptr;
                            if (materialNames) {
                                const auto named = materialNames->find(id);
                                if (named != materialNames->end()) name = &named->second;
                            }
                            const auto separator = targets.empty() ? std::string_view{} : std::string_view(" → ");
                            const compat::RetainedStringSlice targetName = name ? *name : translateText("Unresolved target");
                            std::string addition(separator);
                            addition += targetName.view();
                            targets += addition;
                        }
                    }
                    const auto operationId = operation.sourceIndexValue("id").as_string();
                    const auto parameter = ngRigString(plan,
                        compat::RetainedString::fromStaticLiteral("parameter"), compat::RetainedString::emptyLiteral());
                    const auto id = operationId.view();
                    const auto kind = translateText(id.starts_with("mechanism:")
                        ? compat::RetainedString::fromStaticLiteral("Parameter")
                        : id.starts_with("control:") ? compat::RetainedString::fromStaticLiteral("Parameter binding")
                        : id.starts_with("weld:") ? compat::RetainedString::fromStaticLiteral("Shoulder welding")
                        : id.starts_with("mask:") ? compat::RetainedString::fromStaticLiteral("Clipping mask")
                        : compat::RetainedString::fromStaticLiteral("Draw order"));
                    std::string description(kind.view());
                    description += ": ";
                    description += parameter.view();
                    if (!parameter.empty() && !targets.empty()) description += " / ";
                    description += targets;
                    auto& keyMembers = plan.sourceObjectRef();
                    if (const auto* keys = keyMembers.if_contains("keys")) {
                        const auto pattern = translateText(" (%s keys)");
                        const std::array<core::translation::TypedFormatArgument, 1> arguments{
                            core::translation::ngFormatArgument(ngRigUnsigned(*keys))};
                        description += core::translation::ngFormatTyped(pattern.view(), arguments);
                    }
                    auto& sortMembers = plan.sourceObjectRef();
                    if (const auto* zsort = sortMembers.if_contains("relative_zsort")) {
                        const std::array<core::translation::TypedFormatArgument, 1> arguments{
                            core::translation::ngFormatArgument(ngRigNumber(*zsort))};
                        description += core::translation::ngFormatTyped(" (%.2f)", arguments);
                    }
                    draft.operationLabels.append(compat::RetainedString(std::move(description)));
                    draft.enabled.append(operation.sourceIndexValue("enabled").as_bool());
                }
            }
        }
        reviewDrafts[key] = draft;
        found = reviewDrafts.find(key);
        cached = found == reviewDrafts.end() ? nullptr : &found->second;
    }
    if (!cached) {
        const auto text = cStringCopy(translateText("Waiting for task result"));
        ImGui::TextUnformatted(cStringData(text));
        return true;
    }
    if (!cached->operationIds.empty()) {
        const auto text = cStringCopy(translateText(
            "Enable or disable individual operations, then rerun this task and its dependents."));
        ImGui::TextWrapped("%s", cStringData(text));
        ImGui::BeginDisabled(worker != nullptr);
        if (ImGui::BeginChild("##Operations", ImVec2(0, 200))) {
            const auto ids = cached->operationIds;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                const auto id = ids[i];
                const auto idText = cStringCopy(id);
                ImGui::PushID(cStringData(idText));
                {
                    auto enabled = cached->enabled;
                    ImGui::Checkbox("##enabled", &sourceElement(enabled, i, 566));
                }
                ImGui::SameLine();
                const auto label = cStringCopy(sourceElement(cached->operationLabels, i, 567));
                ImGui::TextWrapped("%s", cStringData(label));
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        const auto apply = ngTranslateCString("Apply and rerun downstream");
        if (widgets::ngButtonColored(apply.data())) {
            JsonArray disabled;
            const auto ids = cached->operationIds;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                const auto id = ids[i];
                if (!sourceElement(cached->enabled, i, 572)) disabled.append(Json(JsonString(id)));
            }
            try {
                const auto context = compat::ngRequireObject(run->session())->inputContext(taskId.retainedString());
                auto settings = compat::ngRequireObject(context)->hasValue(
                    compat::RetainedString::fromStaticLiteral("review"))
                    ? context->value(compat::RetainedString::fromStaticLiteral("review")).json : Json(JsonObject{});
                settings.sourceIndexAssignSlot("disabled") = Json(disabled);
                context->setValue(compat::RetainedString::fromStaticLiteral("review"),
                    framework::AutoRigValue::jsonValue(settings));
                startRun(run, {}, false);
            } catch (const compat::ThrowableException& error) {
                const std::lock_guard lock(stateMonitor);
                lastError = error.msg;
            }
        }
        ImGui::EndDisabled();
    }
    if (task.state == framework::AutoRigTaskState::Stale) {
        const auto text = cStringCopy(translateText("Previous result. Settings changed; rerun to update it."));
        ImGui::TextWrapped("%s", cStringData(text));
    }
    if (!cached->tables.empty()) renderReviewTables(cached->tables);
    else {
        const auto text = cStringCopy(translateText("Waiting for task result"));
        ImGui::TextUnformatted(cStringData(text));
    }
    return true;
}

compat::RetainedStringSlice AutoRigPanel::inputText(
        ::nijigenerate::autorig::framework::AutoRigValue value) {
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    switch (value.kind) {
    case Kind::FileName:
    case Kind::Path:
    case Kind::Blob:
        return compat::RetainedStringSlice(value.text);
    case Kind::Json:
        return compat::ngJsonStringify(value.json);
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 237);
}

compat::RetainedStringSlice AutoRigPanel::valueKindLabel(
        ::nijigenerate::autorig::framework::AutoRigValueKind kind) {
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    using core::translation::ngTranslateRetained;
    switch (kind) {
    case Kind::FileName: return ngTranslateRetained(compat::RetainedString::fromStaticLiteral("File name"));
    case Kind::Path: return ngTranslateRetained(compat::RetainedString::fromStaticLiteral("File path"));
    case Kind::Json: return ngTranslateRetained(compat::RetainedString::fromStaticLiteral("JSON"));
    case Kind::Blob: return ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Binary data"));
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 254);
}

bool AutoRigPanel::renderStateTree(compat::RetainedStringSlice title,
        compat::RetainedStringSlice state, compat::RetainedStringSlice id, ImGuiTreeNodeFlags flags) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto translate = []<std::size_t N>(const char (&value)[N]) {
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
    };
    ImVec4 color;
    compat::RetainedString icon;
    compat::RetainedString status;
    if (state.view() == "Running") {
        color = {0, 0.4f, 0.8f, 1};
        icon = compat::RetainedString::fromStaticLiteral("\xEE\x87\x84"); // U+E1C4
        status = translate("Running");
    } else if (state.view() == "Succeeded") {
        color = {0, 0.9f, 0, 1};
        icon = compat::RetainedString::fromStaticLiteral("\xEE\xA4\xAF"); // U+E92F
        status = translate("Completed");
    } else if (state.view() == "Failed") {
        color = {0.9f, 0, 0, 1};
        icon = compat::RetainedString::fromStaticLiteral("\xEF\xA2\xB6"); // U+F8B6
        status = translate("Failed");
    } else if (state.view() == "Canceled") {
        color = {0.8f, 0.4f, 0, 1};
        icon = compat::RetainedString::fromStaticLiteral("\xEE\x97\x89"); // U+E5C9
        status = translate("Canceled");
    } else {
        color = {0.8f, 0.4f, 0, 1};
        icon = compat::RetainedString::fromStaticLiteral("\xEE\xBD\x8A"); // U+EF4A
        status = state.view() == "Stale" ? translate("Needs rerun") : translate("Pending");
    }
    const auto iconText = cStringCopy(icon);
    ImGui::TextColored(color, "%s", cStringData(iconText));
    if (ImGui::IsItemHovered()) {
        const auto statusText = cStringCopy(compat::RetainedStringSlice(status));
        ImGui::SetTooltip("%s", cStringData(statusText));
    }
    ImGui::SameLine(0, 0);
    std::string label(title.data() ? title.data() : "", title.size());
    label += "###";
    label.append(id.data() ? id.data() : "", id.size());
    const auto labelText = cStringCopy(std::move(label));
    return ImGui::TreeNodeEx(cStringData(labelText), flags);
}

::nijigenerate::autorig::framework::AutoRigValue AutoRigPanel::draftValue(
        ::nijigenerate::autorig::framework::AutoRigValueKind kind, compat::RetainedStringSlice draft) {
    using Value = ::nijigenerate::autorig::framework::AutoRigValue;
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    switch (kind) {
    case Kind::FileName: return Value::fileName(draft.retainedString());
    case Kind::Path: return Value::path(draft.retainedString());
    case Kind::Json:
        return Value::jsonValue(::nijigenerate::autorig::json::detail::parseJSON(draft));
    case Kind::Blob: {
        auto bytes = nicxlive::core::d_runtime::ngReadBinaryFile(draft.retainedString());
        return Value::blob(compat::Slice<std::uint8_t>::retainedView(
            bytes.retentionOwner(), bytes.data(), bytes.size()));
    }
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 263);
}

void AutoRigPanel::applyInput(
        std::shared_ptr<::nijigenerate::autorig::framework::IAutoRigInputContext> context,
        ::nijigenerate::autorig::framework::AutoRigPortSpec port, compat::RetainedStringSlice draft) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto receiver = compat::ngRequireObject(context);
    const auto id = port.id;
    auto value = draftValue(port.kind, draft);
    receiver->setValue(id.retainedString(), value);
}

compat::RetainedStringSlice AutoRigPanel::cachedInputText(compat::RetainedStringSlice key,
        std::uint64_t revision, compat::SharedDelegate<compat::RetainedStringSlice()> load) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto found = inputTextCache.find(key);
    const auto* cached = found == inputTextCache.end() ? nullptr : &found->second;
    if (!cached || cached->revision != revision) {
        // Selected Source lowering retains the AA field address before key/RHS.
        auto& table = inputTextCache;
        const auto insertionKey = key;
        const InputTextCache value{revision, load()};
        table[insertionKey] = value;
    }
    return sourceMapValue(inputTextCache, key, 250).text;
}

void AutoRigPanel::renderSessionValue(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice name, ::nijigenerate::autorig::framework::AutoRigValueKind kind,
        compat::RetainedStringSlice current, bool contextValue) {
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto key = draftKey(run, contextValue ?
        compat::RetainedString::fromStaticLiteral("session-context") :
        compat::RetainedString::fromStaticLiteral("workflow-input"), name);
    if (!inputDrafts.contains(key) || (inputObserved.contains(key) &&
            sourceMapValue(inputObserved, key, 283) != current &&
            sourceMapValue(inputDrafts, key, 283) == sourceMapValue(inputObserved, key, 283))) {
        inputDrafts[key] = current;
    }
    inputObserved[key] = current;
    const auto keyText = cStringCopy(key);
    ImGui::PushID(cStringData(keyText));
    auto label = withStringAllocation([&] {
        std::string value(name.view());
        value += " (";
        return value;
    });
    const auto kindText = valueKindLabel(kind);
    withStringAllocation([&] { label += kindText.view(); label += ')'; });
    const auto labelText = cStringCopy(std::move(label));
    ImGui::TextUnformatted(cStringData(labelText));
    auto draft = sourceMapValue(inputDrafts, key, 287);
    if (kind == Kind::Json) widgets::ngInputTextMultiline("##value", draft, ImVec2(0, 80));
    else widgets::ngInputText("##value", draft);
    inputDrafts[key] = draft;
    const auto apply = core::translation::ngTranslateCString("Apply");
    if (widgets::ngButtonColored(apply.data())) {
        try {
            auto value = draftValue(kind, draft);
            if (contextValue) compat::ngRequireObject(run)->setContextValue(name.retainedString(), value);
            else compat::ngRequireObject(run)->setInput(name.retainedString(), value);
            const std::lock_guard lock(stateMonitor);
            lastError.reset();
        } catch (const compat::ThrowableException& error) {
            const std::lock_guard lock(stateMonitor);
            lastError = error.msg;
        }
    }
    ImGui::PopID();
}

void AutoRigPanel::renderSessionConfiguration(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run) {
    using namespace ::nijigenerate::autorig;
    using Kind = framework::AutoRigValueKind;
    using Json = framework::Json;
    using core::translation::ngTranslateCString;
    using core::translation::ngTranslateRetained;
    [[maybe_unused]] const auto owner = shared_from_this();
    ImGui::BeginDisabled(worker != nullptr);
    compat::ngRunWithScopeExit([&] {
        if (compat::ngRequireObject(run)->renderInputUI()) return;
        const auto arguments = ngTranslateCString("Workflow arguments");
        if (ImGui::TreeNodeEx(arguments.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto ports = compat::ngRequireObject(run)->inputPorts();
            for (auto port : ports) {
                if (compat::ngRequireObject(run)->workflowId().view() == "model-to-rig" &&
                        port.id.view() == "options") {
                    auto found = workflowOptionsDrafts.find(run->id());
                    auto* cached = found == workflowOptionsDrafts.end() ? nullptr : &found->second;
                    if (!cached || cached->revision != run->inputRevision()) {
                        auto options = run->input(compat::RetainedString::fromStaticLiteral("options")).json;
                        auto& insertionTable = workflowOptionsDrafts;
                        const auto insertionKey = run->id();
                        WorkflowOptionsDraft draft{run->inputRevision(), options,
                            deterministic::contracts::ngRigGet(options,
                                compat::RetainedString::fromStaticLiteral("render"), Json(false)).as_bool()};
                        insertionTable[insertionKey] = draft;
                        found = workflowOptionsDrafts.find(run->id());
                        cached = found == workflowOptionsDrafts.end() ? nullptr : &found->second;
                    }
                    const auto capture = cStringCopy(compat::RetainedStringSlice(ngTranslateRetained(
                        compat::RetainedString::fromStaticLiteral("Capture source appearance for verification"))));
                    if (ImGui::Checkbox(capture.data(), &compat::ngRequireObject(cached)->previews)) {
                        cached->options = compat::ngRequireObject(
                            compat::ngRequireObject(compat::ngRequireObject(run)->session())->inputContext(
                                run->stepTaskId(compat::RetainedString::fromStaticLiteral("compile"))))->value(
                                    compat::RetainedString::fromStaticLiteral("options")).json;
                        cached->options.sourceIndexAssignSlot("render") = Json(cached->previews);
                        run->setInput(compat::RetainedString::fromStaticLiteral("options"),
                            framework::AutoRigValue::jsonValue(cached->options));
                    }
                    const auto description = cStringCopy(compat::RetainedStringSlice(ngTranslateRetained(
                        compat::RetainedString::fromStaticLiteral(
                            "Material classification and landmark positions are edited in the compile task."))));
                    ImGui::TextWrapped("%s", description.data());
                    continue;
                }
                if (!port.description.empty()) {
                    const auto description = cStringCopy(compat::RetainedStringSlice(
                        ngTranslateRetained(port.description.retainedString())));
                    ImGui::TextWrapped("%s", description.data());
                }
                const auto key = draftKey(run, compat::RetainedString::fromStaticLiteral("workflow-input"), port.id);
                const auto revision = run->inputRevision();
                const auto current = cachedInputText(key, revision,
                    compat::SharedDelegate<compat::RetainedStringSlice()>([this, run, &port] {
                        return run->hasInput(port.id.retainedString()) ? inputText(run->input(port.id.retainedString()))
                            : compat::RetainedStringSlice(compat::RetainedString::emptyLiteral());
                    }));
                renderSessionValue(run, port.id, port.kind, current, false);
            }
            ImGui::TreePop();
        }
        const auto contextLabel = ngTranslateCString("Session context / artifacts");
        if (ImGui::TreeNode(contextLabel.data())) {
            auto names = compat::ngRequireObject(run->session())->contextNames();
            if (names.size() > 1)
                std::sort(names.begin(), names.end(), [](const auto& left, const auto& right) {
                    return left.view() < right.view();
                });
            for (auto name : names) {
                auto info = compat::ngRequireObject(run->session())->contextInfo(name);
                const auto key = draftKey(run, compat::RetainedString::fromStaticLiteral("session-context"), name);
                const auto current = cachedInputText(key, info.revision,
                    compat::SharedDelegate<compat::RetainedStringSlice()>([this, run, &name, &info] {
                        return info.kind == Kind::Json ? inputText(run->contextValue(name))
                            : compat::RetainedStringSlice(info.text);
                    }));
                renderSessionValue(run, name, info.kind, current, true);
                if (info.kind == Kind::Blob) {
                    const auto pattern = ngTranslateRetained(compat::RetainedString::fromStaticLiteral("%s bytes"));
                    const std::array<core::translation::TypedFormatArgument, 1> values{
                        core::translation::ngFormatArgument(info.byteLength)};
                    const auto text = cStringCopy(core::translation::ngFormatTyped(pattern.view(), values));
                    ImGui::TextUnformatted(text.data());
                }
            }
            auto found = contextDrafts.find(run->id());
            if (found == contextDrafts.end()) {
                contextDrafts[run->id()] = ContextDraft{};
                found = contextDrafts.find(run->id());
            }
            auto* draft = found == contextDrafts.end() ? nullptr : &found->second;
            widgets::ngInputText(compat::RetainedStringSlice(ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Name"))), compat::ngRequireObject(draft)->name);
            const auto type = ngTranslateCString("Type");
            const auto kindLabel = cStringCopy(valueKindLabel(draft->kind));
            if (ImGui::BeginCombo(type.data(), kindLabel.data())) {
                for (const auto kind : {Kind::Path, Kind::Json, Kind::Blob, Kind::FileName}) {
                    const auto label = cStringCopy(valueKindLabel(kind));
                    if (ImGui::Selectable(label.data(), kind == draft->kind)) draft->kind = kind;
                }
                ImGui::EndCombo();
            }
            widgets::ngInputText(compat::RetainedStringSlice(ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Value / artifact path"))), draft->value);
            const auto hint = cStringCopy(compat::RetainedStringSlice(ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral(
                    "Path references an artifact; Blob imports file contents; Json stores structured data."))));
            ImGui::TextWrapped("%s", hint.data());
            const auto add = ngTranslateCString("Add context value");
            if (widgets::ngButtonColored(add.data())) {
                try {
                    const auto name = draft->name;
                    auto value = draftValue(draft->kind, draft->value);
                    run->setContextValue(name.retainedString(), value);
                    *draft = ContextDraft{};
                    const std::lock_guard lock(stateMonitor);
                    lastError.reset();
                } catch (const compat::ThrowableException& error) {
                    const std::lock_guard lock(stateMonitor);
                    lastError = error.msg;
                }
            }
            ImGui::TreePop();
        }
    }, [] { ImGui::EndDisabled(); });
}

bool AutoRigPanel::renderRun(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run) {
    using State = ::nijigenerate::autorig::workflow::AutoRigWorkflowState;
    using core::translation::ngTranslateRetained;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto id = cStringCopy(compat::RetainedStringSlice(compat::ngRequireObject(run)->id()));
    ImGui::PushID(id.data());
    return compat::ngRunWithScopeExit([&] {
        const auto snapshot = run->snapshot(false);
        auto label = ngTranslateRetained(run->displayName());
        std::string labelBytes(label.view());
        labelBytes += " #";
        {
            const compat::RetainedStringSlice currentId(run->id());
            if (currentId.size() < 8) throw compat::ngStaticError<compat::ArraySliceError>(
                0, 8, currentId.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 850);
            labelBytes += currentId.slice(0, 8).view();
        }
        ImGui::BeginDisabled(activeRun == run || compat::ngRequireObject(run->session())->isBusy());
        const bool remove = widgets::ngButtonColored("\xee\xa1\xb2##delete", ImVec2(24, 24));
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            const auto text = cStringCopy(compat::RetainedStringSlice(ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Delete session"))));
            ImGui::SetTooltip("%s", text.data());
        }
        if (remove) return true;
        ImGui::SameLine();
        const bool running = worker != nullptr && activeRun == run;
        ImGui::BeginDisabled((worker != nullptr && !running) ||
            (running && compat::ngRequireObject(run->session())->isCanceled()));
        if (widgets::ngButtonColored(running ? "\xee\x97\x89" : "\xee\x80\xb7", ImVec2(24, 24))) {
            if (running) run->cancel();
            else startRun(run);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            const auto text = cStringCopy(compat::RetainedStringSlice(ngTranslateRetained(running
                ? compat::RetainedString::fromStaticLiteral("Cancel")
                : compat::RetainedString::fromStaticLiteral("Run batch"))));
            ImGui::SetTooltip("%s", text.data());
        }
        ImGui::SameLine();
        core::translation::FormatEnum state;
        state.base = std::get<core::translation::FormatInteger>(
            core::translation::ngFormatArgument(static_cast<std::int32_t>(snapshot.state)));
        state.typeName = compat::RetainedString::fromStaticLiteral("AutoRigWorkflowState");
        switch (snapshot.state) {
        case State::Pending: state.memberName = compat::RetainedString::fromStaticLiteral("Pending"); break;
        case State::Running: state.memberName = compat::RetainedString::fromStaticLiteral("Running"); break;
        case State::Succeeded: state.memberName = compat::RetainedString::fromStaticLiteral("Succeeded"); break;
        case State::Failed: state.memberName = compat::RetainedString::fromStaticLiteral("Failed"); break;
        case State::Canceled: state.memberName = compat::RetainedString::fromStaticLiteral("Canceled"); break;
        }
        const std::array<core::translation::TypedFormatArgument, 1> values{state};
        const auto stateText = compat::RetainedString(core::translation::ngFormatTyped("%s", values));
        if (renderStateTree(compat::RetainedString(std::move(labelBytes)), stateText,
                compat::RetainedString::fromStaticLiteral("run"), ImGuiTreeNodeFlags_DefaultOpen)) {
            renderSessionConfiguration(run);
            renderSessionOutput(run);
            const auto steps = run->orderedSteps();
            for (auto step : steps) renderTask(run, step);
            ImGui::TreePop();
        }
        return false;
    }, [] { ImGui::PopID(); });
}

void AutoRigPanel::renderMaterialRoles(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId) {
    using namespace ::nijigenerate::autorig;
    using namespace deterministic::contracts;
    using namespace deterministic::review;
    using namespace deterministic::presentation;
    using Json = framework::Json;
    using JsonObject = core::settings::SettingsJsonObject;
    using JsonString = core::settings::SettingsString;
    using core::translation::ngTranslateRetained;
    using core::translation::ngTranslateCString;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto literal = []<std::size_t N>(const char (&value)[N]) -> compat::RetainedStringSlice {
        return compat::RetainedString::fromStaticLiteral(value);
    };
    const auto translate = [](compat::RetainedStringSlice value) -> compat::RetainedStringSlice {
        return ngTranslateRetained(value.retainedString());
    };
    const auto lower = [](compat::RetainedStringSlice value) {
        return nicxlive::core::common::unicode::toLower(value.retainedString());
    };
    const auto context = compat::ngRequireObject(compat::ngRequireObject(run)->session())->inputContext(
        taskId.retainedString());
    if (!compat::ngRequireObject(context)->hasValue(literal("materials").retainedString())) {
        const auto text = cStringCopy(translate(literal("Observe the model first to configure material roles.")));
        ImGui::TextUnformatted(text.data());
        return;
    }
    const auto source = compat::ngRequireObject(run->session())->task(run->stepTaskId(
        literal("source").retainedString()), false);
    const auto compiled = compat::ngRequireObject(run->session())->task(taskId.retainedString(), false);
    const auto revision = compat::ngRequireObject(run->session())->inputRevision(taskId.retainedString());
    auto found = materialDrafts.find(run->id());
    auto* cached = found == materialDrafts.end() ? nullptr : &found->second;
    if (!cached || cached->attempt != source.attempt || cached->compiledAttempt != compiled.attempt ||
            cached->revision != revision) {
        MaterialDraft draft;
        draft.attempt = source.attempt;
        draft.compiledAttempt = compiled.attempt;
        draft.revision = revision;
        auto materials = compat::ngRequireObject(context)->value(literal("materials").retainedString()).json;
        const auto reviewArtifacts = compat::ngRequireObject(run->session())->task(taskId.retainedString()).artifacts;
        for (auto artifact : reviewArtifacts) if (artifact.portId.view() == "review") {
            auto report = framework::ngCopyAutoRigValue(artifact.value()).json;
            if (const auto* classification = report.sourceObjectRef().if_contains("classification"))
                materials = *classification;
        }
        auto options = context->value(literal("options").retainedString()).json;
        const auto evidenceArtifacts = compat::ngRequireObject(run->session())->task(taskId.retainedString()).artifacts;
        for (auto artifact : evidenceArtifacts) if (artifact.portId.view() == "evidence") {
            auto evidence = framework::ngCopyAutoRigValue(artifact.value()).json;
            if (const auto* landmarks = evidence.sourceObjectRef().if_contains("landmarks")) {
                compat::Slice<JsonString> names;
                const auto members = landmarks->sourceObjectRef();
                for (const auto& member : members) names.append(member.name);
                if (names.size() > 1)
                    std::sort(names.begin(), names.end(), [](const auto& left, const auto& right) {
                        return left.view() < right.view();
                    });
                for (auto name : names) {
                    auto item = landmarks->sourceIndexValue(name.view());
                    auto point = ngRigPoint(item.sourceIndexValue("xy"));
                    if (const auto* overrides = options.sourceObjectRef().if_contains("landmarks"))
                        if (const auto* overridePoint = overrides->sourceObjectRef().if_contains(name.view()))
                            point = ngRigPoint(*overridePoint);
                    draft.landmarks.append(name);
                    draft.landmarkX.append(point[0]);
                    draft.landmarkY.append(point[1]);
                    draft.landmarkReasons.append(ngRigString(item, literal("method"),
                        ngRigString(item, literal("provenance"), literal(""))));
                    draft.landmarkChanged.append(false);
                    draft.landmarkReset.append(false);
                }
            }
        }
        const auto materialValues = materials.sourceArrayValue();
        for (auto material : materialValues) {
            const auto name = material.sourceIndexValue("name").as_string();
            const auto path = material.sourceIndexValue("path").as_string();
            const auto candidates = deterministic::evidence::ngRigMaterialRoleCandidates(name);
            auto role = ngRigString(material, literal("role"), candidates.size() == 1 ? candidates[0] : literal(""));
            auto feature = ngRigString(material, literal("feature"), literal(""));
            auto side = ngRigString(material, literal("side_override"), literal(""));
            bool disabled = ngRigMaterialStaticPreference(material);
            if (const auto* overrides = options.sourceObjectRef().if_contains("materials"))
                if (const auto* entry = overrides->sourceObjectRef().if_contains(path.view())) {
                    if (const auto* stationary = entry->sourceObjectRef().if_contains("static"))
                        disabled = stationary->as_bool();
                    if (const auto* chosen = entry->sourceObjectRef().if_contains("role")) role = chosen->as_string();
                    feature = ngRigString(*entry, literal("feature"), feature);
                    side = ngRigString(*entry, literal("side"), side);
                }
            const auto semanticSource = ngRigString(material, literal("semantic_source"));
            const bool forcedStatic = ngRigMaterialForcedStatic(
                material.sourceIndexValue("active").as_bool(), role, semanticSource);
            draft.requestedStatic.append(disabled);
            draft.semanticSources.append(semanticSource);
            disabled = disabled || forcedStatic;
            draft.names.append(name);
            draft.paths.append(path);
            draft.roles.append(role);
            draft.features.append(feature);
            draft.sides.append(side);
            draft.disabled.append(disabled);
            draft.active.append(material.sourceIndexValue("active").as_bool());
            draft.forcedStatic.append(forcedStatic);
            const auto reason = ngRigReviewEvidenceLabel(
                ngRigString(material, literal("semantic_source"), literal("")), translate);
            std::string reasonBytes(reason.view());
            reasonBytes += " / ";
            reasonBytes += ngRigString(material, literal("owner"), literal("")).view();
            reasonBytes += " / ";
            reasonBytes += ngRigString(material, literal("chart"), literal("")).view();
            draft.reasons.append(compat::RetainedString(std::move(reasonBytes)));
            draft.changed.append(false);
            draft.reset.append(false);
            draft.editedFields.append(0);
        }
        materialDrafts[run->id()] = draft;
        found = materialDrafts.find(run->id());
        cached = found == materialDrafts.end() ? nullptr : &found->second;
    }
    if (materialRoleChoices.empty()) {
        materialRoleChoices = {literal("")};
        const auto rules = deterministic::templates::ngRigMaterialRoles().sourceIndexValue("rules").sourceArrayValue();
        for (auto rule : rules) materialRoleChoices.append(rule.sourceIndexValue("id").as_string());
    }
    const auto introduction = cStringCopy(translate(literal(
        "Inspect the actual classification and its evidence. Disabling rigging keeps the artwork static. Only edited rows become overrides.")));
    ImGui::TextWrapped("%s", introduction.data());
    const auto unresolved = cStringCopy(translate(literal("Show unresolved only")));
    ImGui::Checkbox(unresolved.data(), &compat::ngRequireObject(cached)->unresolvedOnly);
    widgets::ngInputText(translate(literal("Filter materials")), cached->filter);
    if (ImGui::BeginChild("##MaterialRoles", ImVec2(0, 240))) {
        if (ImGui::BeginTable("##MaterialClassification", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit)) {
            const std::array<compat::RetainedStringSlice, 6> headers{
                translate(literal("Material")), translate(literal("Role")), translate(literal("Static artwork")),
                translate(literal("Feature")), translate(literal("Model side")), translate(literal("Classification evidence"))};
            for (auto header : headers) {
                const auto text = cStringCopy(header);
                ImGui::TableSetupColumn(text.data());
            }
            ImGui::TableHeadersRow();
            const auto paths = cached->paths;
            for (std::size_t i = 0; i < paths.size(); ++i) {
                const auto path = paths[i];
                if (cached->unresolvedOnly && !sourceElement(cached->roles, i, 670).empty()) continue;
                if (!cached->filter.empty() && lower(path).view().find(lower(cached->filter).view()) == std::string_view::npos &&
                        lower(sourceElement(cached->names, i, 672)).view().find(lower(cached->filter).view()) == std::string_view::npos) continue;
                const auto pathText = cStringCopy(path);
                ImGui::PushID(pathText.data());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const auto nameText = cStringCopy(sourceElement(cached->names, i, 675));
                ImGui::TextUnformatted(nameText.data());
                if (ImGui::IsItemHovered()) {
                    const auto text = cStringCopy(path);
                    ImGui::SetTooltip("%s", text.data());
                }
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(150);
                const auto label = cStringCopy(!sourceElement(cached->roles, i, 678).empty()
                    ? ngRigReviewClassLabel(sourceElement(cached->roles, i, 678), translate) : translate(literal("Choose role")));
                if (ImGui::BeginCombo("##role", label.data())) {
                    const auto choices = materialRoleChoices;
                    for (auto choice : choices) {
                        const auto display = cStringCopy(!choice.empty()
                            ? ngRigReviewClassLabel(choice, translate) : translate(literal("Choose role")));
                        if (ImGui::Selectable(display.data(), choice == sourceElement(cached->roles, i, 682))) {
                            sourceElement(cached->roles, i, 683) = choice;
                            sourceElement(cached->changed, i, 683) = true;
                            sourceElement(cached->reset, i, 683) = false;
                            sourceElement(cached->editedFields, i, 684) |= static_cast<std::uint8_t>(RigMaterialField::role);
                            auto forcedValues = cached->forcedStatic;
                            auto& forced = sourceElement(forcedValues, i, 685);
                            const auto active = sourceElement(cached->active, i, 685);
                            const auto semanticSource = sourceElement(cached->semanticSources, i, 686);
                            forced = ngRigMaterialForcedStatic(active, choice, semanticSource);
                            auto disabledValues = cached->disabled;
                            auto& disabled = sourceElement(disabledValues, i, 687);
                            disabled = sourceElement(cached->forcedStatic, i, 687) ||
                                sourceElement(cached->requestedStatic, i, 687);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(sourceElement(cached->forcedStatic, i, 693));
                const bool staticChanged = [&] {
                    auto disabled = cached->disabled;
                    return ImGui::Checkbox("##static", &sourceElement(disabled, i, 694));
                }();
                if (staticChanged) {
                    auto requestedValues = cached->requestedStatic;
                    auto& requested = sourceElement(requestedValues, i, 695);
                    requested = sourceElement(cached->disabled, i, 695);
                    sourceElement(cached->changed, i, 696) = true;
                    sourceElement(cached->reset, i, 696) = false;
                    sourceElement(cached->editedFields, i, 697) |= static_cast<std::uint8_t>(RigMaterialField::stationary);
                }
                ImGui::EndDisabled();
                const auto featureLabel = ngRigReviewClassLabel(sourceElement(cached->features, i, 700), translate);
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(150);
                const auto featureText = cStringCopy(featureLabel);
                if (ImGui::BeginCombo("##feature", featureText.data())) {
                    const std::array features{literal(""), literal("mouth"), literal("mouth_tongue"),
                        literal("mouth_upper_teeth"), literal("mouth_lower_teeth"), literal("mouth_outline"),
                        literal("mouth_upper_lip"), literal("mouth_lower_lip"), literal("brow"), literal("sclera"),
                        literal("iris"), literal("corner"), literal("upper"), literal("lower"), literal("fold"), literal("nose")};
                    for (auto feature : features) {
                        const auto display = cStringCopy(ngRigReviewClassLabel(feature, translate));
                        if (ImGui::Selectable(display.data(), feature == sourceElement(cached->features, i, 707))) {
                            sourceElement(cached->features, i, 708) = feature;
                            sourceElement(cached->changed, i, 708) = true;
                            sourceElement(cached->reset, i, 708) = false;
                            sourceElement(cached->editedFields, i, 709) |= static_cast<std::uint8_t>(RigMaterialField::feature);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(110);
                const auto sideLabel = cStringCopy(!sourceElement(cached->sides, i, 714).empty()
                    ? ngRigReviewClassLabel(sourceElement(cached->sides, i, 714), translate) : translate(literal("Automatic")));
                if (ImGui::BeginCombo("##side", sideLabel.data())) {
                    for (auto side : {literal(""), literal("L"), literal("R")}) {
                        const auto display = cStringCopy(!side.empty()
                            ? ngRigReviewClassLabel(side, translate) : translate(literal("Automatic")));
                        if (ImGui::Selectable(display.data(), side == sourceElement(cached->sides, i, 717))) {
                            sourceElement(cached->sides, i, 718) = side;
                            sourceElement(cached->changed, i, 718) = true;
                            sourceElement(cached->reset, i, 718) = false;
                            sourceElement(cached->editedFields, i, 719) |= static_cast<std::uint8_t>(RigMaterialField::side);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::TableNextColumn();
                const auto reason = cStringCopy(sourceElement(cached->reasons, i, 723));
                ImGui::TextUnformatted(reason.data());
                const auto reset = ngTranslateCString("Reset to automatic");
                if (widgets::ngButtonColored(reset.data())) {
                    sourceElement(cached->changed, i, 724) = true;
                    sourceElement(cached->reset, i, 724) = true;
                }
                if (sourceElement(cached->changed, i, 725)) {
                    const auto text = cStringCopy(translate(literal("Unapplied changes")));
                    ImGui::TextUnformatted(text.data());
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    if (!cached->landmarks.empty() && ImGui::TreeNode(ngTranslateCString("Anatomical landmarks").data())) {
        const auto hint = cStringCopy(translate(literal(
            "Edit measured landmark positions in model coordinates. The scaffold is solved again from these inputs.")));
        ImGui::TextWrapped("%s", hint.data());
        const auto names = cached->landmarks;
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto name = names[i];
            const auto id = cStringCopy(name);
            ImGui::PushID(id.data());
            const auto text = cStringCopy(name);
            ImGui::TextUnformatted(text.data());
            const bool xChanged = [&] {
                auto values = cached->landmarkX;
                return ImGui::InputDouble("X", &sourceElement(values, i, 737));
            }();
            if (xChanged) {
                sourceElement(cached->landmarkChanged, i, 738) = true;
                sourceElement(cached->landmarkReset, i, 738) = false;
            }
            const bool yChanged = [&] {
                auto values = cached->landmarkY;
                return ImGui::InputDouble("Y", &sourceElement(values, i, 740));
            }();
            if (yChanged) {
                sourceElement(cached->landmarkChanged, i, 741) = true;
                sourceElement(cached->landmarkReset, i, 741) = false;
            }
            const auto reason = cStringCopy(sourceElement(cached->landmarkReasons, i, 743));
            ImGui::TextWrapped("%s", reason.data());
            if (widgets::ngButtonColored(ngTranslateCString("Reset to automatic").data())) {
                sourceElement(cached->landmarkChanged, i, 745) = true;
                sourceElement(cached->landmarkReset, i, 745) = true;
            }
            if (sourceElement(cached->landmarkChanged, i, 747)) {
                const auto changed = cStringCopy(translate(literal("Unapplied changes")));
                ImGui::TextUnformatted(changed.data());
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    const auto applyLabel = cStringCopy(translate(literal("Apply review settings")));
    const bool apply = widgets::ngButtonColored(applyLabel.data());
    ImGui::SameLine();
    const bool rerun = widgets::ngButtonColored(ngTranslateCString("Apply and rerun downstream").data());
    if (apply || rerun) try {
        auto options = compat::ngRequireObject(context)->value(literal("options").retainedString()).json;
        JsonObject overrides;
        if (const auto* previous = options.sourceObjectRef().if_contains("materials"))
            overrides = previous->sourceObjectRef();
        const auto paths = cached->paths;
        for (std::size_t i = 0; i < paths.size(); ++i) {
            const auto path = paths[i];
            if (!sourceElement(cached->changed, i, 759)) continue;
            if (sourceElement(cached->reset, i, 760)) {
                overrides.erase(path.view());
                continue;
            }
            const auto* entry = overrides.if_contains(path.view());
            const auto original = entry ? *entry : Json{};
            const auto editedFields = sourceElement(cached->editedFields, i, 763);
            const auto role = sourceElement(cached->roles, i, 763);
            const auto disabled = sourceElement(cached->disabled, i, 763);
            const auto feature = sourceElement(cached->features, i, 763);
            const auto side = sourceElement(cached->sides, i, 763);
            overrides[JsonString(path)] = ngRigEditedMaterialOverride(original, editedFields, role, disabled, feature, side);
        }
        options.sourceIndexAssignSlot("materials") = Json(overrides);
        JsonObject landmarks;
        if (const auto* previous = options.sourceObjectRef().if_contains("landmarks"))
            landmarks = previous->sourceObjectRef().duplicate();
        const auto names = cached->landmarks;
        for (std::size_t i = 0; i < names.size(); ++i) if (sourceElement(cached->landmarkChanged, i, 768)) {
            const auto name = names[i];
            if (sourceElement(cached->landmarkReset, i, 769)) landmarks.erase(name.view());
            else landmarks[JsonString(name)] = Json(core::settings::SettingsJsonArray{
                Json(sourceElement(cached->landmarkX, i, 770)), Json(sourceElement(cached->landmarkY, i, 770))});
        }
        options.sourceIndexAssignSlot("landmarks") = Json(landmarks);
        context->setValue(literal("options").retainedString(), framework::AutoRigValue::jsonValue(options));
        if (rerun) startRun(run, {}, false);
    } catch (const compat::ThrowableException& error) {
        const std::lock_guard lock(stateMonitor);
        lastError = error.msg;
    }
}

void AutoRigPanel::renderSessionOutput(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run) {
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto title = core::translation::ngTranslateCString("Workflow output");
    if (!ImGui::TreeNode(title.data())) return;
    compat::ngRunWithScopeExit([&] {
        if (compat::ngRequireObject(run)->renderOutputUI()) return;
        if (compat::ngRequireObject(run)->workflowId() == "model-to-rig") {
            const auto message = cStringCopy(translateText(
                "The resulting rig is applied to the editor model. Inspect task changes and verification below."));
            ImGui::TextWrapped("%s", cStringData(message));
            return;
        }
        const auto ports = compat::ngRequireObject(run)->outputPorts();
        for (auto port : ports) {
            const auto name = cStringCopy(port.id);
            ImGui::TextUnformatted(cStringData(name));
            if (!compat::ngRequireObject(run)->hasOutput(port.id.retainedString())) {
                const auto waiting = cStringCopy(translateText("Waiting for output"));
                ImGui::TextUnformatted(cStringData(waiting));
            } else {
                auto artifact = compat::ngRequireObject(run)->outputArtifact(port.id.retainedString());
                const auto label = port.kind == Kind::Json ?
                    translateText("JSON artifact ready") :
                    port.kind == Kind::Blob ? translateText("Binary artifact") :
                    artifact.value().text;
                const auto text = cStringCopy(label);
                ImGui::TextUnformatted(cStringData(text));
                if (port.kind == Kind::Blob) {
                    const auto pattern = translateText("%s bytes");
                    const std::array<core::translation::TypedFormatArgument, 1> arguments{
                        core::translation::ngFormatArgument(artifact.byteLength())};
                    const auto bytes = cStringCopy(core::translation::ngFormatTyped(pattern.view(), arguments));
                    ImGui::TextUnformatted(cStringData(bytes));
                }
            }
        }
    }, [] { ImGui::TreePop(); });
}

void AutoRigPanel::renderDefaultOutputs(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId) {
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto context = compat::ngRequireObject(compat::ngRequireObject(run)->session())->outputContext(taskId.retainedString());
    const auto ports = compat::ngRequireObject(context)->ports();
    for (auto port : ports) {
        std::string label(port.id.view());
        label += " (";
        label += valueKindText(port.kind);
        label += ')';
        const auto text = cStringCopy(std::move(label));
        ImGui::TextUnformatted(cStringData(text));
        if (compat::ngRequireObject(context)->hasValue(port.id.retainedString())) {
            const auto value = port.kind == Kind::Json ?
                compat::RetainedStringSlice(translateText("JSON artifact ready")) :
                port.kind == Kind::Blob ?
                compat::RetainedStringSlice(translateText("Binary artifact")) :
                inputText(compat::ngRequireObject(context)->value(port.id.retainedString()));
            const auto valueText = cStringCopy(value);
            ImGui::TextUnformatted(cStringData(valueText));
        }
    }
    const auto artifacts = compat::ngRequireObject(context)->snapshot().artifacts;
    for (auto artifact : artifacts) {
        std::string label((artifact.preview ? translateText("Preview: ") : translateText("Output")).view());
        label += ' ';
        label += artifact.portId.view();
        const auto text = cStringCopy(std::move(label));
        ImGui::TextUnformatted(cStringData(text));
    }
}

void AutoRigPanel::renderReviewTables(
        compat::Slice<::nijigenerate::autorig::deterministic::presentation::RigReviewTable>& tables) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto currentTables = tables;
    for (std::size_t index = 0; index < currentTables.size(); ++index) {
        auto& table = currentTables[index];
        ImGui::PushID(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index)));
        const auto title = cStringCopy(table.title);
        ImGui::TextUnformatted(cStringData(title));
        const auto pages = (table.rows.size() + 31) / 32;
        if (table.page >= pages) table.page = 0;
        if (pages > 1) {
            const auto previous = core::translation::ngTranslateCString("Previous");
            if (widgets::ngButtonColored(previous.data()) && table.page) --table.page;
            ImGui::SameLine();
            const auto next = core::translation::ngTranslateCString("Next");
            if (widgets::ngButtonColored(next.data()) && table.page + 1 < pages) ++table.page;
            ImGui::SameLine();
            ImGui::Text("%u / %u", static_cast<std::uint32_t>(table.page) + 1u,
                static_cast<std::uint32_t>(pages));
        }
        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit;
        const auto columnsCount = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(table.columns.size()));
        if (ImGui::BeginTable("##Review", columnsCount, flags)) {
            const auto columns = table.columns;
            for (auto column : columns) {
                const auto text = cStringCopy(column);
                ImGui::TableSetupColumn(cStringData(text));
            }
            ImGui::TableHeadersRow();
            const auto firstRow = table.page * 32;
            const auto lastRow = (std::min)(table.rows.size(), (table.page + 1) * 32);
            for (auto row = firstRow; row < lastRow; ++row) {
                ImGui::TableNextRow();
                const auto cells = sourceElement(table.rows, row, 502);
                for (auto cell : cells) {
                    ImGui::TableNextColumn();
                    const auto text = cStringCopy(cell);
                    ImGui::TextUnformatted(cStringData(text));
                }
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::PopID();
    }
}

} // namespace nijigenerate::panels::autorig
