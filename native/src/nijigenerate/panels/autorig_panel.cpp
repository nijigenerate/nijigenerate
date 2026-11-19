// Actual class bodies from independently authored panels/autorig.d; BSD-2-Clause.
#include "autorig.hpp"
#include "detail/autorig_resource_store.hpp"
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
#include <nijigenerate/autorig/deterministic/cheek_options.hpp>
#include <nijigenerate/autorig/deterministic/workflow_options.hpp>
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
#include <nijigenerate/widgets/dragdrop.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <nijigenerate/widgets/output_tree.hpp>
#include <nijigenerate/widgets/controller.hpp>
#include <nodes/part.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/autorig/deterministic/native_stage.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <map>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_set>

namespace nijigenerate::panels::autorig {
using detail::EvidenceResourceGroup;

struct TaskResourceSummary {
    std::vector<std::uint32_t> detected;
    std::vector<EvidenceResourceGroup> tags;
    std::map<std::uint32_t, std::vector<::nijigenerate::autorig::framework::Text>> changes;
    std::map<std::uint32_t, ::nijigenerate::autorig::framework::Text> names;
    std::vector<::nijigenerate::autorig::framework::Text> incompleteStages;
};

struct AutoRigPanel::HandoffPreview {
    std::vector<std::uint32_t> nodes;
    std::vector<EvidenceResourceGroup> groups;
    std::size_t landmarks{};
};

struct AutoRigPanel::EvidenceTree {
    std::shared_ptr<detail::EvidenceResourceStore> store = std::make_shared<detail::EvidenceResourceStore>();
    std::shared_ptr<detail::EvidenceResourceOutput> output = std::make_shared<detail::EvidenceResourceOutput>(store);
    std::vector<EvidenceResourceGroup> groups;
    std::uint64_t indexRevision{};
};

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
    case Kind::Object: value.memberName = compat::RetainedString::fromStaticLiteral("Object"); break;
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
    case State::PartiallySucceeded: value.memberName = compat::RetainedString::fromStaticLiteral("PartiallySucceeded"); break;
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
    case State::Blocked: value.memberName = compat::RetainedString::fromStaticLiteral("Blocked"); break;
    case State::PartiallySucceeded: value.memberName = compat::RetainedString::fromStaticLiteral("PartiallySucceeded"); break;
    case State::NotApplicable: value.memberName = compat::RetainedString::fromStaticLiteral("NotApplicable"); break;
    }
    const std::array<TypedFormatArgument, 1> arguments{value};
    return compat::RetainedString(ngFormatTyped("%s", arguments));
}

compat::RetainedString taskStatusLabel(::nijigenerate::autorig::framework::AutoRigTaskState state) {
    using State = ::nijigenerate::autorig::framework::AutoRigTaskState;
    switch (state) {
    case State::Succeeded: return translateText("Processing complete");
    case State::Failed: return translateText("Failed");
    case State::Running: return translateText("Running");
    case State::Pending: return translateText("Pending");
    case State::Canceled: return translateText("Canceled");
    case State::Stale: return translateText("Needs rerun");
    case State::Blocked: return translateText("Blocked");
    case State::PartiallySucceeded: return translateText("Partially completed");
    case State::NotApplicable: return translateText("Not applicable");
    }
    return {};
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
    renderActiveProgress();
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
    eraseChildren(evidenceTrees);
    eraseChildren(resourceSummaries);
    reviewTaskByRun.erase(runId);
    reviewHandoffByRun.erase(runId);
    eraseChildren(handoffTargets);
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

namespace {
using ::nijigenerate::autorig::framework::Text;
using ::nijigenerate::autorig::workflow::AutoRigWorkflowRun;

std::vector<Text> workflowTasks(const std::shared_ptr<AutoRigWorkflowRun>& run) {
    std::vector<Text> result;
    std::unordered_set<Text, ::nijigenerate::autorig::framework::TextHash,
        ::nijigenerate::autorig::framework::TextEqual> seen;
    const auto visit = [&](auto&& self, Text id) -> void {
        if (!seen.insert(id).second) return;
        const auto spec = run->session()->taskSpec(id);
        for (const auto& dep : spec.dependencies) self(self, dep.retainedString());
        for (const auto& connection : spec.connections) self(self, connection.sourceTask.retainedString());
        result.push_back(std::move(id));
    };
    for (const auto& step : run->orderedSteps()) visit(visit, run->stepTaskId(step.id.retainedString()));
    return result;
}
using TaskState = ::nijigenerate::autorig::framework::AutoRigTaskState;
using TaskPhase = ::nijigenerate::autorig::framework::AutoRigTaskPhase;

bool needsAttention(TaskState state) {
    return state == TaskState::Failed || state == TaskState::Blocked || state == TaskState::PartiallySucceeded ||
        state == TaskState::Stale || state == TaskState::Canceled;
}

Text missingRequiredInput(const std::shared_ptr<AutoRigWorkflowRun>& run, const Text& task,
        bool externalOnly = false) {
    const auto session = run->session();
    const auto spec = session->taskSpec(task);
    for (const auto& port : spec.inputs) {
        if (externalOnly && std::ranges::any_of(spec.connections, [&](const auto& connection) {
                return connection.input == port.id;
            })) continue;
        if (port.required && session->inputEvidence(task, port.id.retainedString()).empty())
            return port.id.retainedString();
    }
    return {};
}

// Material Symbols Outlined, merged into the application's UI font.
const char* taskStatusIcon(TaskState state) {
    if (state == TaskState::Succeeded) return "\ue92f"; // check_circle
    if (state == TaskState::Running) return "\ue1c4"; // timelapse
    if (state == TaskState::Failed) return "\uf8b6"; // error
    if (state == TaskState::Canceled) return "\ue5c9"; // cancel
    if (state == TaskState::NotApplicable) return "\ue15b"; // remove
    return "\uef4a"; // pending
}

ImVec4 taskStatusColor(TaskState state) {
    switch (state) {
    case TaskState::Running: return {0.f, .4f, .8f, 1.f};
    case TaskState::Succeeded: return {0.f, .9f, 0.f, 1.f};
    case TaskState::Failed: return {.9f, 0.f, 0.f, 1.f};
    case TaskState::NotApplicable: return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    default: return {.8f, .4f, 0.f, 1.f};
    }
}

constexpr auto changeArrow = " \ue5c8 "; // arrow_forward

template<std::size_t N>
void reviewLine(const char (&label)[N]) {
    const auto text = translateText(label);
    ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
}

void reviewGuidance(std::string_view step, TaskPhase phase) {
    if (step == "source") reviewLine("Check that the artwork needed for the rig is present, including hidden layers.");
    else if (step == "exclusions" || step == "exclusion-check")
        reviewLine("Check that excluded parts are meant to stay still and that moving parts are still included.");
    else if (step == "measure" || step == "compile")
        reviewLine("Check that the eyes, mouth, neck and joints line up with the artwork. Correct wrong roles or landmarks in the settings.");
    else if (step == "shoulders" || step == "ear-attachments" || step == "hip-attachments" || step == "weld")
        reviewLine("Check that each pair meets at the intended seam. After welding, turn or tilt the model and look for gaps or pulling.");
    else if (step == "mesh" || step == "uv")
        reviewLine("Select a part and check that the mesh covers the artwork and the texture sits in the right place.");
    else if (step == "groups" || step == "composites" || step == "layout")
        reviewLine("Check that parts which move together are grouped and that clipping and draw order keep the eyes and mouth visible.");
    else if (step == "build" || step == "depth")
        reviewLine("Check that the bones meet the joints and that the face, neck and clothing are placed at the right depth.");
    else if (step == "bake")
        reviewLine("Turn and tilt the face and body in both directions. Check that the head stays attached to the neck and the arms, waist and legs follow.");
    else if (step == "controls")
        reviewLine("Blink and open the mouth. Check that the eyes and mouth close cleanly and that highlights and clipping follow.");
    else if (step == "foot-plan")
        reviewLine("Check which foot and leg grids are eligible before applying the correction.");
    else if (step == "feet")
        reviewLine("Tilt the body to both sides. Check that the feet hold their position and the legs do not pull away.");
    else if (step == "rear-plan" || step == "cheek-plan")
        reviewLine("Check that the selected parts belong to the back hair or skin outline this correction is meant to follow.");
    else if (step == "rear")
        reviewLine("Turn the head left, right, up and down. Check that the back hair follows the head without bulging or stepping.");
    else if (step == "corrections")
        reviewLine("Compare both sides of the face. Follow the outline from chin to temple and check that the cheek and hair meet without a dent or gap.");
    else if (step == "physics")
        reviewLine("Move and stop the model. Check that the intended parts sway together and settle.");
    else if (step == "verify" || phase == TaskPhase::Verify)
        reviewLine("Review any item marked for attention, then move the model in the viewport to check its appearance.");
    else if (phase == TaskPhase::Collect || phase == TaskPhase::Plan)
        reviewLine("Check that the selected parts have the roles needed for this step.");
    else
        reviewLine("Move the affected parameter and check that the selected parts follow as intended.");
}

Text handoffLabel(compat::RetainedStringSlice port) {
    if (port.view() == "state") return translateText("Model state");
    if (port.view() == "model") return translateText("Model checkpoint");
    if (port.view() == "program") return translateText("Rig plan");
    if (port.view() == "plan") return translateText("Correction targets and settings");
    if (port.view() == "pairs") return translateText("Part pairs");
    if (port.view() == "shoulder-pairs") return translateText("Shoulder and arm pairs");
    if (port.view() == "ear-pairs") return translateText("Front and back ear pairs");
    if (port.view() == "hip-pairs") return translateText("Waist and leg pairs");
    if (port.view() == "options") return translateText("Workflow settings");
    if (port.view() == "targets" || port.view() == "target-associations") return translateText("Target nodes");
    if (port.view() == "measurements") return translateText("Body measurements");
    if (port.view() == "materials") return translateText("Material classification");
    return translateText(port.retainedString());
}

struct ReviewStep {
    Text task;
    compat::RetainedStringSlice step;
    compat::RetainedStringSlice label;
    TaskState state;
};

struct TaskHandoff {
    std::size_t from, to;
    std::vector<::nijigenerate::autorig::framework::AutoRigConnection> ports;
};

std::vector<TaskHandoff> workflowHandoffs(const std::shared_ptr<AutoRigWorkflowRun>& run,
        const std::vector<ReviewStep>& rows) {
    std::vector<TaskHandoff> edges;
    for (std::size_t to = 0; to < rows.size(); ++to) {
        const auto spec = run->session()->taskSpec(rows[to].task);
        std::map<std::size_t, std::vector<::nijigenerate::autorig::framework::AutoRigConnection>> inputs;
        const auto sourceIndex = [&](std::string_view id) {
            return static_cast<std::size_t>(std::ranges::find_if(rows,
                [&](const auto& row) { return row.task.view() == id; }) - rows.begin());
        };
        for (const auto& dep : spec.dependencies) {
            const auto from = sourceIndex(dep.view());
            if (from < to) inputs.try_emplace(from);
        }
        for (const auto& port : spec.connections) {
            const auto from = sourceIndex(port.sourceTask.view());
            if (from < to) inputs[from].push_back(port);
        }
        for (auto& [from, ports] : inputs) edges.push_back({from, to, std::move(ports)});
    }
    return edges;
}

std::optional<std::size_t> renderWorkflowFlow(const std::vector<ReviewStep>& rows,
        const std::vector<TaskHandoff>& edges, std::optional<std::size_t> task,
        std::optional<std::size_t> handoff) {
    std::optional<std::size_t> selected;
    const float line = ImGui::GetTextLineHeight();
    constexpr float railWidth = 22.f;
    const auto ink = ImGui::GetColorU32(ImGuiCol_Text);
    const auto muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const auto border = ImGui::GetColorU32(ImGuiCol_Text, .28f);
    auto* panelState = ImGui::GetStateStorage();
    const auto fractionKey = ImGui::GetID("task-flow-fraction");
    const float fraction = panelState->GetFloat(fractionKey, .45f);
    const float rowHeight = ImGui::GetFrameHeightWithSpacing();
    const float available = std::max(ImGui::GetContentRegionAvail().y, rowHeight * 10.f);
    const float height = available * fraction;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.f, 8.f});
    if (ImGui::BeginChild("task-flow", {0, height}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) {
        auto* draw = ImGui::GetWindowDrawList();
        std::vector<ImVec2> anchors;
        anchors.reserve(rows.size());
        const auto focusKey = ImGui::GetID("last-focused-task");
        auto* storage = ImGui::GetStateStorage();
        const int focus = task ? static_cast<int>(*task) : -1;
        const bool reveal = focus != storage->GetInt(focusKey, -1);
        storage->SetInt(focusKey, focus);
        ImGui::Indent(railWidth);
        const float width = std::max(60.f, ImGui::GetContentRegionAvail().x);
        for (std::size_t index = 0; index < rows.size(); ++index) {
            const auto& row = rows[index];
            ImGui::PushID(cStringCopy(row.task).data());
            const auto status = cStringCopy(taskStatusLabel(row.state));
            const float statusWidth = ImGui::CalcTextSize(status.data()).x;
            const bool inlineStatus = width >= statusWidth + 155.f;
            const float titleWidth = std::max(24.f, width-35.f-(inlineStatus ? statusWidth+8.f : 0.f));
            const auto titleSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, titleWidth,
                row.label.data(), row.label.data()+row.label.size());
            const float titleHeight = std::max(line, titleSize.y);
            const float cardHeight = titleHeight + (inlineStatus ? 0.f : line+2.f) + 10.f;
            if (ImGui::InvisibleButton("task", {width, cardHeight}, ImGuiButtonFlags_EnableNav)) selected = index;
            const auto p = ImGui::GetItemRectMin(), q = ImGui::GetItemRectMax();
            anchors.push_back({p.x, (p.y+q.y)*.5f});
            const bool active = task == index;
            if (active && reveal) ImGui::SetScrollHereY(.4f);
            if (active || ImGui::IsItemHovered() || ImGui::IsItemFocused())
                draw->AddRectFilled(p, q, ImGui::GetColorU32(ImGuiCol_Text, active ? .11f : .06f), 4.f);
            draw->AddRect(p, q, active ? ink : border, 4.f, 0, active ? 1.5f : 1.f);
            draw->PushClipRect(p, q, true);
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {p.x+7.f, p.y+5.f},
                ImGui::GetColorU32(taskStatusColor(row.state)), taskStatusIcon(row.state));
            draw->AddText(nullptr, 0, {p.x+28.f, p.y+5.f}, ink, row.label.data(),
                row.label.data()+row.label.size(), titleWidth);
            const ImVec2 statusPosition = inlineStatus ? ImVec2{q.x-statusWidth-6.f, p.y+5.f} :
                ImVec2{p.x+28.f, p.y+titleHeight+7.f};
            draw->AddText(statusPosition, needsAttention(row.state) || active ? ink : muted, status.data());
            draw->PopClipRect();
            ImGui::PopID();
        }
        ImGui::Unindent(railWidth);
        if (task) for (std::size_t index = 0; index < edges.size(); ++index) {
            const auto& edge = edges[index];
            if (edge.from != *task && edge.to != *task) continue;
            const auto from = anchors[edge.from], to = anchors[edge.to];
            const float rail = to.x-(edge.to == *task ? 16.f : 9.f);
            const ImVec2 path[]{from, {rail, from.y}, {rail, to.y}, to};
            const auto color = handoff == index ? ink : border;
            draw->AddPolyline(path, 4, color, 0, handoff == index ? 2.f : 1.f);
            draw->AddTriangleFilled(to, {to.x-5.f, to.y-3.f}, {to.x-5.f, to.y+3.f}, color);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::InvisibleButton("resize-task-flow", {ImGui::GetContentRegionAvail().x, line * .55f});
    const bool resizing = ImGui::IsItemActive();
    const bool hover = ImGui::IsItemHovered();
    if (resizing || hover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (resizing) {
        const float change = ImGui::GetIO().MouseDelta.y / available;
        panelState->SetFloat(fractionKey, std::clamp(fraction + change, .25f, .85f));
    } else if (hover) {
        ImGui::SetTooltip("%s", cStringCopy(translateText("Drag to resize the task list")).data());
    }
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const float middle = (min.y + max.y) * .5f;
    const auto handleColor = resizing ? ImGuiCol_SeparatorActive : hover ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator;
    ImGui::GetWindowDrawList()->AddLine({min.x, middle}, {max.x, middle}, ImGui::GetColorU32(handleColor));
    return selected;
}

void reviewMotion(const std::shared_ptr<AutoRigWorkflowRun>& run, std::string_view step) {
    struct Control { const char* name; Text label; };
    using Controls = std::initializer_list<Control>;
    const Controls angles{{"Face::Yaw-Pitch", translateText("Face turn and tilt")},
        {"Body::Yaw-Pitch", translateText("Body turn and lean")}, {"Body::Roll", translateText("Body side tilt")}};
    const Controls face{{"Face::Yaw-Pitch", translateText("Face turn and tilt")}};
    const Controls feet{{"Body::Roll", translateText("Body side tilt")}};
    const Controls expressions{{"Eye::L::Blink", translateText("Left blink")}, {"Eye::R::Blink", translateText("Right blink")},
        {"Mouth::Open", translateText("Mouth opening")}};
    const Controls* controls = step == "bake" || step == "verify" || step == "weld" ? &angles :
        step == "rear" || step == "corrections" ? &face : step == "feet" ? &feet :
        step == "controls" ? &expressions : nullptr;
    if (!controls || run->workflowId().view() != "model-to-rig") return;
    if (!ImGui::TreeNodeEx("motion-preview", ImGuiTreeNodeFlags_DefaultOpen, "%s",
            cStringCopy(translateText("Try the movement")).data())) return;
    const auto puppet = project::incActivePuppet();
    const auto session = run->session();
    const auto source = run->stepTaskId("source");
    if (puppet && puppet->root && session->hasOutput(source, "state")) {
        const auto state = session->output(source, "state").json;
        const auto root = state.is_object() ? state.sourceObjectRef().if_contains("rootId") : nullptr;
        std::uint64_t node = 0;
        if (root && root->is_uint64()) node = root->as_uint64();
        else if (root && root->is_int64()) node = static_cast<std::uint64_t>(root->as_int64());
        if (node == puppet->root->getUUID()) {
            std::vector<std::shared_ptr<nicxlive::core::param::Parameter>> parameters;
            const auto count = static_cast<float>(controls->size());
            const float available = ImGui::GetContentRegionAvail().x;
            const float column = (available - ImGui::GetStyle().ItemSpacing.x * (count - 1)) / count;
            const bool sideBySide = column >= 76;
            const float width = std::min(sideBySide ? column : available, 150.0f);
            for (const auto& control : *controls) {
                const auto found = std::ranges::find_if(puppet->parameters, [&](const auto& parameter) {
                    return parameter->getName().view() == control.name;
                });
                if (found == puppet->parameters.end()) continue;
                auto parameter = *found;
                if (sideBySide && !parameters.empty()) ImGui::SameLine();
                parameters.push_back(parameter);
                ImGui::PushID(control.name);
                ImGui::BeginGroup();
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
                ImGui::TextUnformatted(control.label.data(), control.label.data() + control.label.size());
                ImGui::PopTextWrapPos();
                if (width >= 30)
                    widgets::ngController<4.0f, 4.0f, 3.0f, 1.0f>(std::string_view("##pose"), parameter,
                        {width, parameter->isVec2 ? 62.0f : 22.0f});
                ImGui::EndGroup();
                ImGui::PopID();
            }
            if (!parameters.empty() && ImGui::SmallButton(cStringCopy(translateText("Reset these pose controls")).data()))
                for (const auto& parameter : parameters) parameter->value = parameter->defaults;
        } else reviewLine("Open the model used by this run to try its movement.");
    } else reviewLine("Open the model used by this run to try its movement.");
    ImGui::TreePop();
}

} // namespace

void AutoRigPanel::renderTask(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        ::nijigenerate::autorig::framework::Text taskId, compat::RetainedStringSlice stepId,
        bool running, const std::function<void()>& renderInputs, const std::function<void()>& renderNext) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto task = compat::ngRequireObject(compat::ngRequireObject(run)->session())->task(taskId, false);
    const auto state = task.state;
    ImGui::PushID(cStringCopy(taskId).data());
    const auto spec = compat::ngRequireObject(compat::ngRequireObject(run)->session())->taskSpec(taskId);
    const compat::RetainedStringSlice title = !spec.label.empty()
        ? core::translation::ngTranslateRetained(spec.label.retainedString()) : compat::RetainedStringSlice(taskId);
    enum View { Review, Inputs, Next, Evidence };
    auto* ui = ImGui::GetStateStorage();
    const auto initial = state == TaskState::Pending || state == TaskState::Blocked ? Inputs : Review;
    const auto selectedTabKey = ImGui::GetID("requested-step-tab");
    const auto evidenceTabKey = ImGui::GetID("evidence-tab-open");
    const auto displayState = running ? TaskState::Running : state;
    const auto heading = taskStatusLabel(displayState);
    ImGui::TextWrapped("%.*s", static_cast<int>(title.size()), title.data());
    bool openMenu = !running && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right);
    ImGui::TextColored(taskStatusColor(displayState), "%s", taskStatusIcon(displayState));
    ImGui::SameLine();
    ImGui::TextUnformatted(heading.data(), heading.data() + heading.size());

    if (!running) {
        const auto expanded = run->session()->task(taskId);
        const bool hasComparison = std::ranges::any_of(expanded.artifacts, [](const auto& artifact) {
            const auto port = artifact.portId.view();
            return port == "review" || port == "report" || port == "review-layout";
        });
        const bool hasEvidence = !expanded.artifacts.empty() || !expanded.inputEvidenceIds.empty();
        bool evidenceOpen = hasEvidence && ui->GetBool(evidenceTabKey, false);
        const auto selectTab = [&](View destination) { ui->SetInt(selectedTabKey, destination); };
        const auto runCurrent = [&] {
            const auto missing = missingRequiredInput(run, taskId);
            const bool canPrepare = !missing.empty() && missingRequiredInput(run, taskId, true).empty();
            ImGui::BeginDisabled(worker != nullptr || (!missing.empty() && !canPrepare));
            const auto label = canPrepare ? translateText("Prepare inputs and run") :
                state == TaskState::Pending ? translateText("Run this step") :
                translateText("Rerun this step");
            if (ImGui::Button(cStringCopy(label).data())) {
                startRun(run, stepId, true, {}, taskId);
                selectTab(Review);
            }
            ImGui::EndDisabled();
            if (!missing.empty()) {
                const auto format = cStringCopy(translateText("Waiting for %s"));
                ImGui::TextWrapped(format.data(), cStringCopy(handoffLabel(missing)).data());
            }
        };
        const auto showResult = [&] {
            if (state == TaskState::Stale)
                reviewLine("Settings changed. Run the step again to check the new result.");
            else if (state == TaskState::Blocked)
                reviewLine("Check the preceding step that provides the missing result.");
            renderTaskEvidence(run, expanded);
            if (state == TaskState::Blocked || state == TaskState::Stale || state == TaskState::Failed) {
                if (ImGui::Button(cStringCopy(translateText("Check step inputs")).data())) selectTab(Inputs);
            }
            if (!worker && (state == TaskState::Succeeded || state == TaskState::PartiallySucceeded)) {
                reviewGuidance(stepId.view(), spec.phase);
                reviewMotion(run, stepId.view());
            }
            if (hasComparison && ImGui::TreeNode(cStringCopy(translateText("Changes tab")).data())) {
                if (!renderRigOutput(run, taskId)) run->session()->renderOutputUI(taskId);
                ImGui::TreePop();
            }
        };
        const auto showInputs = [&] {
            runCurrent();
            ImGui::BeginDisabled(worker != nullptr);
            renderTargetSettings(run, taskId);
            renderInputs();
            if (ImGui::TreeNodeEx("input-options", stepId.view() == "compile" ? ImGuiTreeNodeFlags_DefaultOpen : 0,
                    "%s", cStringCopy(translateText("Settings")).data())) {
                if (stepId.view() == "compile") renderMaterialRoles(run, taskId);
                else if (!run->session()->renderInputUI(taskId)) renderDefaultInputs(run, taskId);
                ImGui::TreePop();
            }
            ImGui::EndDisabled();
        };
        if (ImGui::BeginTabBar("step-views", ImGuiTabBarFlags_FittingPolicyScroll)) {
            const int requested = ui->GetInt(selectedTabKey, initial);
            ui->SetInt(selectedTabKey, -1);
            const auto tab = [&](View destination, const Text& label, const auto& body, bool* open = nullptr) {
                const auto flags = requested == destination ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(cStringCopy(label).data(), open, flags)) {
                    ImGui::Spacing();
                    body();
                    ImGui::EndTabItem();
                }
            };
            tab(Review, translateText("Step result tab"), showResult);
            tab(Inputs, translateText("Step inputs tab"), showInputs);
            tab(Next, translateText("Continue to dependent steps"), renderNext);
            if (evidenceOpen)
                tab(Evidence, translateText("Evidence tab"), [&] { renderTaskEvidence(run, expanded, true); },
                    &evidenceOpen);
            ImGui::BeginDisabled(!hasEvidence);
            if (ImGui::TabItemButton("\ue5d3##step-options", ImGuiTabItemFlags_Trailing)) openMenu = true;
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", cStringCopy(translateText("Step options")).data());
            ImGui::EndTabBar();
        }
        if (openMenu) ImGui::OpenPopup("step-menu");
        if (ImGui::BeginPopup("step-menu")) {
            const auto label = cStringCopy(translateText("Advanced evidence"));
            if (ImGui::MenuItem(label.data(), nullptr, false, hasEvidence)) {
                evidenceOpen = true;
                selectTab(Evidence);
            }
            ImGui::EndPopup();
        }
        ui->SetBool(evidenceTabKey, evidenceOpen);
    }
    ImGui::PopID();
}

namespace {
using namespace ::nijigenerate::autorig::framework;

void evidenceText(const Text& text) {
    if (!text.empty()) ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void evidenceNode(const detail::EvidenceNodeIndex& index, std::uint32_t id) {
    const auto node = index.find(id);
    ImGui::PushID(static_cast<int>(id));
    if (node) {
        const auto icon = utils::incTypeIdToIcon(node->typeId());
        const std::string label = std::string(icon) + " " + std::string(node->getName().view()) + "###node";
        const bool selected = project::incNodeInSelection(node);
        if (const auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node);
                part && part->textures[0]) {
            widgets::ngTextureSlotUntitled(std::string_view("part"), part->textures[0],
                {24, 24}, 1, ImGuiWindowFlags_NoInputs, selected);
            ImGui::SameLine();
        }
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None,
                ImVec2(ImGui::CalcTextSize(label.c_str(), nullptr, true).x + 4, 0))) {
            if (!ImGui::GetIO().KeyCtrl) project::incSelectNode(node);
            else if (selected) project::incRemoveSelectNode(node);
            else project::incAddSelectNode(node);
        }
        if (node->parentRef() && ImGui::BeginDragDropSource()) {
            widgets::ngSetNodeDragDropPayload(node);
            const auto selection = project::incSelectedNodes();
            if (selected && selection.size() > 1) widgets::ngDragdropNodeList(selection);
            else widgets::ngDragdropNodeList(node);
            ImGui::EndDragDropSource();
        }
    } else {
        ImGui::TextDisabled("%s", cStringCopy(translateText("Part unavailable in the current model")).data());
    }
    ImGui::PopID();
}

std::uint32_t evidenceNodeId(const Json& row, const char* field) {
    if (!row.is_object()) return 0;
    const auto fieldValue = row.sourceObjectRef().if_contains(field);
    if (!fieldValue) return 0;
    if (fieldValue->is_uint64()) return static_cast<std::uint32_t>(fieldValue->as_uint64());
    if (fieldValue->is_int64()) return static_cast<std::uint32_t>(fieldValue->as_int64());
    return 0;
}

std::string_view reportText(const Json& row, const char* field) {
    if (!row.is_object()) return {};
    const auto value = row.sourceObjectRef().if_contains(field);
    return value && value->is_string() ? value->as_string().view() : std::string_view{};
}

std::string changeValue(const Json& value) {
    if (value.is_bool()) return std::string(translateText(value.as_bool() ? Text("Enabled") : Text("Disabled")).view());
    if (value.is_string()) return std::string(value.as_string().view());
    const auto number = [](const Json& entry) { return entry.is_int64() || entry.is_uint64() || entry.is_double(); };
    if (number(value)) return std::string(compat::ngJsonStringify(value).view());
    if (value.is_array() && value.as_array().size() <= 4 &&
            std::ranges::all_of(value.as_array(), number))
        return std::string(compat::ngJsonStringify(value).view());
    return {};
}

Text reviewRole(std::string_view tag) {
    const bool left = tag.ends_with("/left");
    const bool right = tag.ends_with("/right");
    if (left || right) tag.remove_suffix(left ? 5 : 6);
    const auto label = [&]() -> Text {
        if (tag == "rig/material") return translateText("Rig materials");
        if (tag == "face/eye") return translateText("Eye area");
        if (tag == "face/mouth") return translateText("Mouth");
        if (tag == "face/skin") return translateText("Face skin");
        if (tag == "face/cheek") return translateText("Cheek outline");
        if (tag == "body/neck") return translateText("Neck");
        if (tag == "body/torso") return translateText("Torso");
        if (tag == "body/arm") return translateText("Arm");
        if (tag == "body/arm/hand") return translateText("Hand");
        if (tag == "body/leg") return translateText("Leg");
        if (tag == "body/leg/foot") return translateText("Foot");
        if (tag == "body/shoulder") return translateText("Shoulder");
        if (tag == "body/hip") return translateText("Waist joint");
        if (tag == "head/ear") return translateText("Ear");
        if (tag == "head/hair/back") return translateText("Back hair");
        return {};
    }();
    if (label.empty() || (!left && !right)) return label;
    return (left ? translateText("Model left") : translateText("Model right")) + " · " + label;
}

std::vector<EvidenceResourceGroup> tagGroups(const std::vector<AutoRigTargetAssociation>& members) {
    std::map<std::string_view, std::vector<std::uint32_t>> byTag;
    for (const auto& member : members)
        if (member.node && !member.tag.empty()) byTag[member.tag.view()].push_back(member.node);
    std::vector<EvidenceResourceGroup> groups;
    groups.reserve(byTag.size());
    for (auto& [tag, nodes] : byTag) {
        auto label = reviewRole(tag);
        if (label.empty()) label = Text(tag);
        groups.push_back({std::string(tag), std::move(label), std::move(nodes)});
    }
    return groups;
}

TaskResourceSummary resourceSummary(const AutoRigTaskSnapshot& task) {
    using namespace ::nijigenerate::autorig::deterministic::native_detail;
    TaskResourceSummary summary;
    const auto detect = [&](std::uint32_t node) {
        if (node) summary.detected.push_back(node);
    };
    const auto operations = [&](const Json& plan, const char* field) {
        if (!plan.is_object()) return;
        const auto rows = plan.sourceObjectRef().if_contains("operations");
        if (rows && rows->is_array())
            for (const auto& row : rows->as_array()) detect(evidenceNodeId(row, field));
    };
    const auto description = [&](std::uint32_t node, Text label) {
        auto& lines = summary.changes[node];
        if (std::ranges::find(lines, label) == lines.end()) lines.push_back(std::move(label));
    };
    bool hasDetections = false;
    for (const auto& artifact : task.artifacts) {
        if (artifact.preview || artifact.evidenceId.empty()) continue;
        const auto port = artifact.portId.view();
        if (port == "target-associations") {
            hasDetections = true;
            const auto associations = artifact.value().object<AutoRigTargetAssociations>();
            summary.tags = tagGroups(associations->members);
            for (const auto& member : associations->members) detect(member.node);
        } else if (port == "plan") {
            hasDetections = true;
            const auto value = artifact.value();
            if (value.holdsObject<FixedFootPlan>()) operations(value.object<FixedFootPlan>()->report, "grid");
            if (value.holdsObject<CheekPlan>()) operations(value.object<CheekPlan>()->report, "part");
            if (value.holdsObject<RearCapPlan>())
                for (const auto& operation : value.object<RearCapPlan>()->operations) detect(operation.part);
        } else if (port == "report" && artifact.kind == AutoRigValueKind::Json) {
            const auto report = artifact.value().json;
            const auto stages = report.is_object() ? report.sourceObjectRef().if_contains("finish_stages") : nullptr;
            if (stages && stages->is_array()) for (const auto& stage : stages->as_array()) {
                if (!stage.is_object()) continue;
                const auto ok = stage.sourceObjectRef().if_contains("succeeded");
                if (ok && ok->is_bool() && !ok->as_bool())
                    summary.incompleteStages.emplace_back(std::string(reportText(stage, "stage")));
            }
        } else if (port == "targets" && artifact.kind == AutoRigValueKind::Json) {
            hasDetections = true;
            const auto value = artifact.value().json;
            for (const auto field : {"parts", "nodes"}) if (value.is_object()) {
                const auto rows = value.sourceObjectRef().if_contains(field);
                if (rows && rows->is_array()) for (const auto& row : rows->as_array()) {
                    if (row.is_uint64()) detect(static_cast<std::uint32_t>(row.as_uint64()));
                    else if (row.is_int64()) detect(static_cast<std::uint32_t>(row.as_int64()));
                }
            }
        }
    }
    for (const auto& artifact : task.artifacts) {
        if (artifact.preview || artifact.portId.view() != "review" || artifact.kind != AutoRigValueKind::Json) continue;
        const auto report = artifact.value().json;
        if (!report.is_object()) continue;
        if (!hasDetections) if (const auto rows = report.sourceObjectRef().if_contains("classification");
                rows && rows->is_array()) {
            for (const auto& row : rows->as_array()) detect(evidenceNodeId(row, "uuid"));
        }
        const auto rows = report.sourceObjectRef().if_contains("changes");
        if (!rows || !rows->is_array()) continue;
        const auto bindingRows = [](const Json& item) {
            if (!item.is_object()) return core::settings::SettingsJsonArray{};
            const auto values = item.sourceObjectRef().if_contains("bindings");
            return values && values->is_array() ? values->sourceArrayValue() : core::settings::SettingsJsonArray{};
        };
        for (const auto& row : rows->as_array()) {
            const auto id = reportText(row, "id");
            const auto action = reportText(row, "change");
            const auto before = row.sourceIndexValue("before");
            const auto after = row.sourceIndexValue("after");
            const auto name = reportText(action == "removed" ? before : after, "name");
            if (id.starts_with("node:")) {
                std::uint32_t node{};
                const auto parsed = std::from_chars(id.data() + 5, id.data() + id.size(), node);
                if (parsed.ec != std::errc{} || parsed.ptr != id.data() + id.size() || !node) continue;
                summary.names[node] = Text(std::string(name));
                if (action == "added") description(node, translateText("Added"));
                else if (action == "removed") description(node, translateText("Removed"));
                else {
                    const auto fields = row.sourceIndexValue("fields");
                    if (fields.is_object()) for (const auto& field : fields.sourceObjectRef()) {
                        const auto label = ::nijigenerate::autorig::deterministic::presentation::ngRigReviewFieldLabel(
                            field.name.retainedString(), [](compat::RetainedStringSlice label) -> compat::RetainedStringSlice {
                                return translateText(label.retainedString());
                            });
                        if (label.view() == field.name.view()) continue;
                        const auto oldValue = changeValue(field.item.sourceIndexValue("before"));
                        const auto newValue = changeValue(field.item.sourceIndexValue("after"));
                        if (!oldValue.empty() && !newValue.empty())
                            description(node, Text(std::string(label.view()) + "  " + oldValue + changeArrow + newValue));
                        else description(node, label.retainedString());
                    }
                    if (!summary.changes.contains(node)) description(node, translateText("Modified"));
                }
            } else if (id.starts_with("parameter:")) {
                const auto oldBindings = bindingRows(before), newBindings = bindingRows(after);
                const auto describe = [&](const Json& binding, Text action, Text property,
                        std::uint32_t oldCount, std::uint32_t newCount) {
                    const auto node = evidenceNodeId(binding, "target");
                    if (!node) return;
                    summary.names[node] = Text(std::string(reportText(binding, "target_name")));
                    const auto label = translateText("Parameter binding");
                    std::string line = std::string(action.view()) + " · " + std::string(name) +
                        " · " + std::string(label.view()) + " · " + std::string(property.view());
                    if (oldCount != newCount) {
                        line += " (" + std::string(translateText("Keys").view()) + " " +
                            std::to_string(oldCount) + changeArrow + std::to_string(newCount) + ")";
                    }
                    description(node, Text(std::move(line)));
                };
                const auto matches = [](const Json& a, const Json& b) {
                    return evidenceNodeId(a, "target") == evidenceNodeId(b, "target") &&
                        reportText(a, "property") == reportText(b, "property");
                };
                const auto propertyName = [](const Json& binding) {
                    const auto property = reportText(binding, "property");
                    return property == "deform" ? translateText("Deformation") : Text(std::string(property));
                };
                for (const auto& binding : newBindings) {
                    auto previous = std::ranges::find_if(oldBindings, [&](const Json& a) { return matches(a, binding); });
                    const auto newCount = evidenceNodeId(binding, "authored_keys");
                    if (previous == oldBindings.end())
                        describe(binding, translateText("Added"), propertyName(binding), 0, newCount);
                    else if (*previous != binding)
                        describe(binding, translateText("Modified"), propertyName(binding),
                            evidenceNodeId(*previous, "authored_keys"), newCount);
                }
                for (const auto& binding : oldBindings)
                    if (std::ranges::none_of(newBindings, [&](const Json& a) { return matches(a, binding); }))
                        describe(binding, translateText("Removed"), propertyName(binding),
                            evidenceNodeId(binding, "authored_keys"), 0);
            }
        }
    }
    std::ranges::sort(summary.detected);
    summary.detected.erase(std::unique(summary.detected.begin(), summary.detected.end()), summary.detected.end());
    return summary;
}

template<class Source, class Edited, class NodeIds>
bool evidenceGroups(const detail::EvidenceNodeIndex& index, const Source& source, const Source& current,
        Edited& edited, NodeIds ids) {
    bool changed = false;
    using Keys = std::remove_cvref_t<decltype(ids(source[0]))>;
    std::map<Keys, bool> groups;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto nodes = ids(source[i]);
        auto [group, inserted] = groups.try_emplace(nodes, false);
        if (!inserted) {
            if (group->second) edited.push_back(source[i]);
            continue;
        }
        bool& include = group->second;
        include = std::ranges::any_of(current, [&](const auto& row) { return ids(row) == nodes; });
        ImGui::PushID(static_cast<int>(i));
        changed |= ImGui::Checkbox("##include", &include);
        for (const auto node : nodes) if (node) {
            ImGui::SameLine();
            evidenceNode(index, node);
        }
        ImGui::PopID();
        if (include) edited.push_back(source[i]);
    }
    return changed;
}

std::optional<AutoRigValue> editPlan(const detail::EvidenceNodeIndex& index,
        const AutoRigValue& source, const AutoRigValue& current) {
    using namespace ::nijigenerate::autorig::deterministic::native_detail;
    if (source.holdsObject<RearCapPlan>() && current.holdsObject<RearCapPlan>()) {
        auto edited = *current.object<RearCapPlan>();
        edited.operations.clear();
        if (evidenceGroups(index, source.object<RearCapPlan>()->operations, current.object<RearCapPlan>()->operations,
                edited.operations, [](const RearCapOperation& operation) { return std::array{operation.part}; }))
            return AutoRigValue::objectValue(std::move(edited));
    } else if (source.holdsObject<FixedFootPlan>() && current.holdsObject<FixedFootPlan>()) {
        auto edited = *current.object<FixedFootPlan>();
        const auto sourceRows = source.object<FixedFootPlan>()->report.sourceIndexValue("operations").sourceArrayValue();
        const auto currentRows = edited.report.sourceIndexValue("operations").sourceArrayValue();
        std::vector<Json> rows;
        if (evidenceGroups(index, sourceRows, currentRows, rows,
                [](const Json& row) { return std::array{evidenceNodeId(row, "grid")}; })) {
            core::settings::SettingsJsonArray operations;
            for (const auto& row : rows) operations.append(row);
            auto report = edited.report.sourceObjectNoRef().duplicate();
            report["operations"] = operations;
            edited.report = report;
            return AutoRigValue::objectValue(std::move(edited));
        }
    } else if (source.holdsObject<Json>() && current.holdsObject<Json>() && source.object<Json>()->is_array() &&
            current.object<Json>()->is_array()) {
        const auto sourceRows = source.object<Json>()->sourceArrayValue();
        const auto currentRows = current.object<Json>()->sourceArrayValue();
        const auto pairNodes = [](const Json& row) {
            return std::array{evidenceNodeId(row, "source"), evidenceNodeId(row, "target")};
        };
        if (!sourceRows.empty() && std::ranges::all_of(sourceRows, [&](const auto& row) {
            const auto nodes = pairNodes(row);
            return nodes[0] && nodes[1];
        })) {
            std::vector<Json> rows;
            if (evidenceGroups(index, sourceRows, currentRows, rows, pairNodes)) {
                core::settings::SettingsJsonArray pairs;
                for (const auto& row : rows) pairs.append(row);
                return AutoRigValue::objectValue(Json(pairs));
            }
        }
    }
    return std::nullopt;
}
} // namespace

detail::EvidenceNodeIndex& AutoRigPanel::evidenceIndex() {
    if (!evidenceNodeIndex) evidenceNodeIndex = std::make_shared<detail::EvidenceNodeIndex>();
    const auto puppet = project::incActivePuppet();
    evidenceNodeIndex->refresh(puppet ? puppet->root : nullptr);
    return *evidenceNodeIndex;
}

void AutoRigPanel::renderEvidenceTree(compat::RetainedStringSlice key, const std::vector<std::uint32_t>& ids) {
    if (!ids.empty()) renderEvidenceGroups(key, {{ {}, {}, ids }});
}

void AutoRigPanel::renderEvidenceGroups(compat::RetainedStringSlice key,
        const std::vector<EvidenceResourceGroup>& groups, const std::function<void(std::uint32_t)>& editNode) {
    if (groups.empty()) return;
    const auto& index = evidenceIndex();
    if (!evidenceTrees.contains(key)) evidenceTrees[key] = std::make_shared<EvidenceTree>();
    auto& view = *evidenceTrees.at(key);
    if (view.indexRevision != index.revision() || view.groups != groups) {
        view.store->assign(groups, index, translateText("Part unavailable in the current model"));
        view.groups = groups;
        view.indexRevision = index.revision();
    }
    if (view.store->roots.empty()) {
        reviewLine("Part unavailable in the current model");
        return;
    }
    const auto rows = view.store->nodes.size() + (groups.front().key.empty() ? 0 : view.store->roots.size());
    const auto displayHeight = static_cast<float>(std::clamp<std::size_t>(rows, 2, 6) * 34 + 24);
    if (ImGui::BeginChild(cStringCopy(key).data(), {0, displayHeight}, ImGuiChildFlags_Borders))
        view.output->render(editNode);
    ImGui::EndChild();
}

void AutoRigPanel::renderTargetSettings(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        ::nijigenerate::autorig::framework::Text task,
        ::nijigenerate::autorig::framework::Text producerEvidence) {
    using namespace ::nijigenerate::autorig::framework;
    const auto session = run->session();
    const auto spec = session->taskSpec(task);
    const auto puppet = project::incActivePuppet();
    if (spec.tagCondition.empty() || !puppet || !puppet->root || worker) return;
    const Text model(std::to_string(puppet->root->getUUID()));
    const auto eligible = session->resolveTargets(spec.tagCondition.retainedString(), model);
    if (!producerEvidence.empty() && std::ranges::find(eligible.evidenceIds, producerEvidence) ==
            eligible.evidenceIds.end()) return;
    const auto title = producerEvidence.empty() ? translateText("Target nodes") :
        translateText(spec.label.retainedString());
    ImGui::PushID(cStringCopy(task).data());
    if (ImGui::TreeNodeEx("targets", producerEvidence.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0,
            "%s", cStringCopy(title).data())) {
        const auto restriction = session->targetRestriction(task);
        const auto selected = restriction.empty() ? nullptr :
            session->readEvidence(restriction).object<AutoRigTargetAssociations>();
        std::vector<std::uint32_t> nodes;
        const auto isSelected = [&](std::uint32_t node) {
            return !selected || (selected->modelIdentity == model &&
                std::ranges::any_of(selected->members, [&](const auto& member) { return node == member.node; }));
        };
        std::vector<std::uint32_t> candidates;
        for (const auto& member : eligible.targets)
            if (std::ranges::find(candidates, member.node) == candidates.end()) candidates.push_back(member.node);
        std::vector<AutoRigTargetAssociation> selectedMembers;
        for (const auto& member : eligible.targets) if (isSelected(member.node)) selectedMembers.push_back(member);
        for (const auto node : candidates) if (isSelected(node)) nodes.push_back(node);
        if (nodes.empty()) evidenceText(translateText("No confirmed targets"));
        else {
            const auto count = cStringCopy(translateText("Parts selected for this step: %zu"));
            ImGui::Text(count.data(), nodes.size());
            renderEvidenceGroups(draftKey(run, task, Text("picker")), tagGroups(selectedMembers));
        }
        bool changed = false;
        const bool showChecklist = ImGui::TreeNode(cStringCopy(translateText("Include or exclude individual targets")).data());
        if (showChecklist) {
            renderEvidenceGroups(draftKey(run, task, Text("picker-edit")), tagGroups(eligible.targets),
                [&](std::uint32_t id) {
                    auto found = std::ranges::find(nodes, id);
                    bool included = found != nodes.end();
                    if (!ImGui::Checkbox("##target", &included)) return;
                    changed = true;
                    if (included) nodes.push_back(id);
                    else nodes.erase(found);
                });
            ImGui::TreePop();
        }
        const auto useSelection = [&] {
            nodes.clear();
            for (const auto& node : project::incSelectedNodes())
                if (eligible.contains(node->getUUID())) nodes.push_back(node->getUUID());
            changed = !nodes.empty();
        };
        if (ImGui::Button(cStringCopy(translateText("Use selected nodes")).data())) useSelection();
        if (ImGui::BeginDragDropTarget()) {
            if (const auto payload = ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
                const auto node = widgets::ngNodeFromDragDropPayload(payload);
                if (node && evidenceIndex().find(node->getUUID()) == node && eligible.contains(node->getUUID())) {
                    if (project::incNodeInSelection(node)) useSelection();
                    else { nodes = {node->getUUID()}; changed = true; }
                }
            }
            ImGui::EndDragDropTarget();
        }
        try {
            if (changed) session->setTargetRestriction(task, model, std::move(nodes));
            if (selected) {
                ImGui::SameLine();
                if (ImGui::Button(cStringCopy(translateText("Follow detected targets")).data()))
                    session->setTargetRestriction(task, model, std::nullopt);
            }
        } catch (const compat::ThrowableException& error) {
            const std::lock_guard lock(stateMonitor);
            lastError = error.msg;
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void AutoRigPanel::renderHandoff(
        const std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun>& run,
        ::nijigenerate::autorig::framework::Text sourceId, ::nijigenerate::autorig::framework::Text taskId,
        const std::vector<::nijigenerate::autorig::framework::AutoRigConnection>& ports) {
    using namespace ::nijigenerate::autorig::framework;
    using namespace ::nijigenerate::autorig::deterministic::native_detail;
    const auto session = run->session();
    const auto receiver = session->task(taskId, false);
    const auto taskTitle = [&](const Text& id) {
        return translateText(session->taskSpec(id).label.retainedString());
    };
    ImGui::PushID(cStringCopy(sourceId).data());
    ImGui::PushID(cStringCopy(taskId).data());
    if (ImGui::SmallButton(cStringCopy(translateText("Inspect sender")).data())) {
        reviewTaskByRun[run->id()] = sourceId;
        reviewHandoffByRun.erase(run->id());
    }
    for (const auto& port : ports) {
        ImGui::PushID(cStringCopy(port.input).data());
        const auto name = handoffLabel(port.sourceOutput);
        const auto planned = session->inputEvidence(taskId, port.input.retainedString());
        const auto used = receiver.inputEvidenceIds.find(port.input);
        const bool recorded = used != receiver.inputEvidenceIds.end();
        const auto evidence = planned;
        const auto status = evidence.empty() ? translateText("Waiting for output") :
            recorded && used->second == evidence ? translateText("Received by task") :
            translateText("Ready for next run");
        ImGui::Separator();
        ImGui::TextWrapped("%.*s  ·  %.*s", static_cast<int>(name.size()), name.data(),
            static_cast<int>(status.size()), status.data());
        if (recorded && used->second != planned) reviewLine("The last run used a different input.");
        if (!evidence.empty()) {
            const auto origin = session->evidenceInfo(evidence);
            if (origin.task != sourceId) {
                if (origin.task == taskId) reviewLine("Uses an edited input.");
                else if (!origin.task.empty()) {
                    const auto originLabel = taskTitle(origin.task);
                    const auto originFormat = cStringCopy(translateText("Passed from %s"));
                    ImGui::TextWrapped(originFormat.data(), cStringCopy(originLabel).data());
                }
            } else if (evidence != session->outputEvidence(sourceId, port.sourceOutput.retainedString())) {
                reviewLine("Uses an earlier result.");
            }
            const auto key = draftKey(run, taskId, evidence);
            const auto sourceSpec = session->taskSpec(sourceId);
            const auto output = std::ranges::find_if(sourceSpec.outputs, [&](const auto& entry) {
                return entry.id == port.sourceOutput;
            });
            if (output != sourceSpec.outputs.end() && output->kind != AutoRigValueKind::Blob &&
                    !handoffTargets.contains(key)) {
                auto preview = std::make_shared<HandoffPreview>();
                auto& nodes = preview->nodes;
                const auto add = [&](std::uint32_t id) {
                    if (id && std::ranges::find(nodes, id) == nodes.end()) nodes.push_back(id);
                };
                const auto value = session->readEvidence(evidence);
                const auto rows = [&](const Json& object, const char* field, const char* node) {
                    if (!object.is_object()) return;
                    const auto values = object.sourceObjectRef().if_contains(field);
                    if (values && values->is_array())
                        for (const auto& row : values->as_array()) add(evidenceNodeId(row, node));
                };
                if (value.holdsObject<AutoRigTargetAssociations>()) {
                    const auto associations = value.object<AutoRigTargetAssociations>();
                    preview->groups = tagGroups(associations->members);
                    for (const auto& member : associations->members) add(member.node);
                } else if (value.holdsObject<FixedFootPlan>()) {
                    rows(value.object<FixedFootPlan>()->report, "operations", "grid");
                } else if (value.holdsObject<CheekPlan>()) {
                    rows(value.object<CheekPlan>()->report, "operations", "part");
                } else if (value.holdsObject<RearCapPlan>()) {
                    for (const auto& operation : value.object<RearCapPlan>()->operations) add(operation.part);
                } else if (value.kind == AutoRigValueKind::Json) {
                    rows(value.json, "carriers", "part");
                    rows(value.json, "targets", "part");
                    rows(value.json, "classification", "uuid");
                    if (port.sourceOutput.view() == "state") rows(value.json, "materials", "uuid");
                }
                if (value.kind == AutoRigValueKind::Json && origin.task == sourceId &&
                        origin.port.view() == "materials" && origin.attempt != 0) {
                    const auto review = session->outputEvidence(origin.task, "review", origin.attempt);
                    if (!review.empty()) rows(session->readEvidence(review).json, "classification", "uuid");
                }
                if (value.holdsObject<Json>()) {
                    const auto data = value.object<Json>();
                    if (data->is_array()) for (const auto& pair : data->as_array()) {
                        const auto source = evidenceNodeId(pair, "source");
                        const auto target = evidenceNodeId(pair, "target");
                        add(source);
                        add(target);
                        if (!source && !target) continue;
                        const auto number = preview->groups.size() + 1;
                        const auto name = cStringCopy(translateText("Part pair %zu"));
                        char label[96];
                        std::snprintf(label, sizeof(label), name.data(), number);
                        std::vector<std::uint32_t> members;
                        if (source) members.push_back(source);
                        if (target) members.push_back(target);
                        preview->groups.push_back({std::to_string(number), Text(std::string(label)), std::move(members)});
                    }
                    if (data->is_object()) {
                        const auto landmarks = data->sourceObjectRef().if_contains("landmarks");
                        if (landmarks && landmarks->is_object())
                            preview->landmarks = landmarks->as_object().size();
                    }
                }
                handoffTargets[key] = std::move(preview);
            }
            const auto found = handoffTargets.find(key);
            if (found != handoffTargets.end() && found->second->landmarks) {
                const auto count = cStringCopy(translateText("Landmarks measured: %d"));
                ImGui::Text(count.data(), static_cast<int>(found->second->landmarks));
            }
            if (found != handoffTargets.end() && !found->second->nodes.empty()) {
                const auto& nodes = found->second->nodes;
                const auto label = cStringCopy(translateText("Handoff parts and nodes"));
                if (ImGui::TreeNodeEx("parts", ImGuiTreeNodeFlags_DefaultOpen, "%s (%zu)", label.data(), nodes.size())) {
                    if (found->second->groups.empty()) renderEvidenceTree(key, nodes);
                    else renderEvidenceGroups(key, found->second->groups);
                    ImGui::TreePop();
                }
            }
            if (!worker) try {
                const auto input = port.input.retainedString();
                const auto original = session->outputEvidence(sourceId, port.sourceOutput.retainedString());
                const auto outputPort = port.sourceOutput.view();
                const bool editable = outputPort == "plan" || outputPort == "pairs" || outputPort.ends_with("-pairs");
                if (editable && !original.empty() && (evidence == original || origin.task == taskId)) {
                    if (auto edited = editPlan(evidenceIndex(), session->readEvidence(original),
                            session->readEvidence(evidence)))
                        session->bindInputEvidence(taskId, input,
                            session->registerInputEvidence(taskId, input, std::move(*edited)));
                }
                if (!session->boundInputEvidence(taskId, input).empty() &&
                        ImGui::SmallButton(cStringCopy(translateText("Follow task output")).data()))
                    session->bindInputEvidence(taskId, input, {});
            } catch (const compat::ThrowableException& error) {
                const std::lock_guard lock(stateMonitor);
                lastError = error.msg;
            }
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    ImGui::PopID();
}

void AutoRigPanel::renderTaskEvidence(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        const ::nijigenerate::autorig::framework::AutoRigTaskSnapshot& task, bool details) {
    using namespace ::nijigenerate::autorig::framework;
    const auto session = run->session();
    using core::translation::ngTranslateCString;
    const auto steps = workflowTasks(run);
    if (!details) {
        const auto summaryKey = draftKey(run, task.taskId, Text("resources-" + std::to_string(task.attempt)));
        std::shared_ptr<const TaskResourceSummary> available;
        if (task.state == AutoRigTaskState::Running) available = std::make_shared<TaskResourceSummary>();
        else {
            if (!resourceSummaries.contains(summaryKey))
                resourceSummaries[summaryKey] = std::make_shared<TaskResourceSummary>(resourceSummary(task));
            available = resourceSummaries.at(summaryKey);
        }
        const auto& summary = *available;
        const auto section = [&](const char* id, const Text& title, const std::vector<std::uint32_t>& nodes,
                bool open, const std::vector<EvidenceResourceGroup>* groups = nullptr) {
            if (nodes.empty()) return;
            if (ImGui::TreeNodeEx(id, open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s (%zu)",
                    cStringCopy(title).data(), nodes.size())) {
                const auto key = draftKey(run, task.taskId, Text(std::string(id) + "-" + std::to_string(task.attempt)));
                if (groups && !groups->empty()) renderEvidenceGroups(key, *groups);
                else renderEvidenceTree(key, nodes);
                ImGui::TreePop();
            }
        };
        std::vector<std::uint32_t> flagged, applied, changed;
        std::vector<EvidenceResourceGroup> issueGroups;
        std::map<std::string, std::size_t> issueByReason;
        std::vector<Text> issuesWithoutParts;
        std::size_t attention = 0;
        for (const auto& unit : task.units) {
            auto* destination = unit.state == TaskState::Succeeded ? &applied :
                needsAttention(unit.state) ? &flagged : nullptr;
            if (destination == &flagged) {
                ++attention;
                const auto reason = unit.message.empty() ? taskStatusLabel(unit.state) : translateText(unit.message);
                if (unit.nodes.empty()) issuesWithoutParts.push_back(reason);
                else {
                    const auto key = std::string(reason.view());
                    const auto [found, first] = issueByReason.try_emplace(key, issueGroups.size());
                    if (first) issueGroups.push_back({key, reason, {}});
                    auto& members = issueGroups[found->second].nodes;
                    members.insert(members.end(), unit.nodes.begin(), unit.nodes.end());
                }
            }
            if (destination) for (const auto node : unit.nodes)
                if (std::ranges::find(*destination, node) == destination->end()) destination->push_back(node);
        }
        for (const auto& change : summary.changes) changed.push_back(change.first);
        if (!attention && needsAttention(task.state) && task.state != TaskState::Stale && !task.message.empty()) {
            const auto message = translateText(task.message);
            ImGui::TextWrapped("%.*s", static_cast<int>(message.size()), message.data());
        }
        if (!summary.incompleteStages.empty()) {
            reviewLine("Steps that did not finish");
            const auto steps = run->orderedSteps();
            for (const auto& incomplete : summary.incompleteStages) {
                const auto step = std::ranges::find_if(steps,
                    [&](const auto& item) { return item.taskId.view() == incomplete.view(); });
                if (step == steps.end()) continue;
                const auto title = translateText(session->taskSpec(run->stepTaskId(step->id.retainedString())).label.retainedString());
                const auto id = run->stepTaskId(step->id.retainedString());
                ImGui::PushID(cStringCopy(id).data());
                if (ImGui::Selectable(cStringCopy(title).data())) reviewTaskByRun[run->id()] = id;
                ImGui::PopID();
            }
        }
        if (attention) {
            const auto heading = ngTranslateCString("Groups to check: %d");
            ImGui::Text(heading.data(), static_cast<int>(attention));
            for (const auto& reason : issuesWithoutParts)
                ImGui::TextWrapped("%.*s", static_cast<int>(reason.size()), reason.data());
            section("flagged", translateText("Find these parts in the tree"), flagged, true, &issueGroups);
        }

        // Choose the list that supports the step's decision. Keep the other views available on demand.
        const bool detection = changed.empty() && !summary.detected.empty();
        const auto& mainTargets = detection ? summary.detected : !changed.empty() ? changed : applied;
        if (mainTargets != flagged || (detection && !summary.tags.empty())) {
            const auto title = detection ? translateText("Check the detected targets") : !changed.empty() ?
                translateText("Changed parts and nodes") : translateText("Processed parts and nodes");
            section("review-targets", title, mainTargets, flagged.empty(), detection ? &summary.tags : nullptr);
        }
        if (!changed.empty()) {
            bool foundSelection = false;
            for (const auto& node : project::incSelectedNodes()) {
                const auto found = summary.changes.find(node->getUUID());
                if (found == summary.changes.end()) continue;
                foundSelection = true;
                evidenceText(Text(node->getName()));
                for (const auto& change : found->second)
                    ImGui::BulletText("%.*s", static_cast<int>(change.size()), change.data());
            }
            if (!foundSelection) reviewLine("Select a changed part to see what was edited");
            const auto& index = evidenceIndex();
            for (const auto node : changed) if (!index.find(node)) {
                const auto name = summary.names.at(node);
                ImGui::Text("%s %.*s", ngTranslateCString("Removed").data(), static_cast<int>(name.size()), name.data());
            }
        }
        if (task.state == TaskState::NotApplicable) reviewLine("No applicable parts were found.");
        const bool otherParts = (!detection && !summary.detected.empty()) ||
            (!applied.empty() && applied != mainTargets && applied != changed) ||
            (task.scope && !task.scope->targets.empty());
        if (otherParts && ImGui::TreeNode(ngTranslateCString("Part details").data())) {
            if (!detection) section("detected", translateText("Check the detected targets"), summary.detected,
                false, &summary.tags);
            if (applied != mainTargets && applied != changed)
                section("applied", translateText("Processed parts and nodes"), applied, false);
            if (task.scope && !task.scope->targets.empty()) {
                std::vector<std::uint32_t> candidates;
                for (const auto& member : task.scope->targets)
                    if (std::ranges::find(candidates, member.node) == candidates.end()) candidates.push_back(member.node);
                const auto groups = tagGroups(task.scope->targets);
                if (candidates != mainTargets || groups != summary.tags)
                    section("eligible", translateText("Eligible parts and nodes"), candidates, false, &groups);
            }
            ImGui::TreePop();
        }
        return;
    }
    if (task.artifacts.empty() && task.inputEvidenceIds.empty()) return;
    if (!task.inputEvidenceIds.empty() && ImGui::TreeNode(cStringCopy(translateText("Input evidence")).data())) {
        for (const auto& [port, id] : task.inputEvidenceIds) {
            const auto origin = session->evidenceInfo(id);
            const auto name = handoffLabel(origin.port.empty() ? port : origin.port);
            if (origin.task.empty() || origin.task == task.taskId) {
                evidenceText(name);
            } else {
                const auto sender = translateText(session->taskSpec(origin.task).label.retainedString());
                ImGui::TextWrapped("%.*s · %.*s", static_cast<int>(name.size()), name.data(),
                    static_cast<int>(sender.size()), sender.data());
            }
        }
        ImGui::TreePop();
    }
    const auto renderArtifacts = [&](const AutoRigTaskSnapshot& snapshot) {
        for (const auto& artifact : snapshot.artifacts) {
            if (artifact.evidenceId.empty() || artifact.preview) continue;
            ImGui::PushID(cStringCopy(artifact.evidenceId).data());
            const auto name = handoffLabel(artifact.portId);
            if (ImGui::TreeNode("evidence", "%.*s", static_cast<int>(name.size()), name.data())) {
                if (artifact.portId.view() == "target-associations") {
                    const auto associations = artifact.value().object<AutoRigTargetAssociations>();
                    renderEvidenceGroups(draftKey(run, task.taskId, artifact.evidenceId),
                        tagGroups(associations->members));
                }
                if (artifact.kind == AutoRigValueKind::Json && artifact.portId.view() == "report" &&
                        ImGui::TreeNode(cStringCopy(translateText("Report data")).data())) {
                    const auto report = inputText(artifact.value());
                    ImGui::TextWrapped("%.*s", static_cast<int>(report.size()), report.data());
                    ImGui::TreePop();
                }
                ImGui::BeginDisabled(worker != nullptr);
                for (const auto& consumer : steps) {
                    const auto spec = session->taskSpec(consumer);
                    if (artifact.portId.view() == "target-associations")
                        renderTargetSettings(run, consumer, artifact.evidenceId);
                    for (const auto& connection : spec.connections) {
                        if (connection.sourceTask.view() != task.taskId.view() ||
                                connection.sourceOutput.view() != artifact.portId.view()) continue;
                        ImGui::PushID(cStringCopy(consumer).data());
                        ImGui::PushID(cStringCopy(connection.input).data());
                        const auto label = cStringCopy(translateText(spec.label.retainedString()));
                        if (ImGui::TreeNode("consumer", "%s", label.data())) {
                            try {
                                const auto port = connection.input.retainedString();
                                const auto input = session->inputEvidence(consumer, port);
                                const auto pinned = session->boundInputEvidence(consumer, port);
                                if (input == artifact.evidenceId) reviewLine("Ready for next run");
                                else if (input.empty()) reviewLine("Waiting for output");
                                else reviewLine("Uses a different result.");
                                if (pinned != artifact.evidenceId &&
                                        ImGui::Button(cStringCopy(translateText("Use this evidence")).data()))
                                    session->bindInputEvidence(consumer, port, artifact.evidenceId);
                                if (!pinned.empty() &&
                                        ImGui::Button(cStringCopy(translateText("Follow task output")).data()))
                                    session->bindInputEvidence(consumer, port, {});
                                const auto latest = session->outputEvidence(snapshot.taskId, artifact.portId);
                                if (input == artifact.evidenceId || (!pinned.empty() && artifact.evidenceId == latest &&
                                        session->evidenceInfo(pinned).task == consumer)) {
                                    if (artifact.kind == AutoRigValueKind::Object) {
                                        if (auto edited = editPlan(evidenceIndex(), artifact.value(), session->readEvidence(input)))
                                            session->bindInputEvidence(consumer, port,
                                                session->registerInputEvidence(consumer, port, std::move(*edited)));
                                    } else if (artifact.kind == AutoRigValueKind::Json &&
                                            artifact.portId.view() != "state" && artifact.portId.view() != "program" &&
                                            ImGui::TreeNode(cStringCopy(translateText("Edit evidence for this input")).data())) {
                                        const auto key = draftKey(run, consumer, port);
                                        if (!inputObserved.contains(key) || inputObserved.at(key) != input) {
                                            inputDrafts[key] = inputText(session->readEvidence(input));
                                            inputObserved[key] = input;
                                        }
                                        auto draft = inputDrafts.at(key);
                                        widgets::ngInputTextMultiline(std::string_view("##evidence"), draft, {-1, 100});
                                        inputDrafts[key] = draft;
                                        const bool apply = ImGui::Button(cStringCopy(translateText("Apply")).data());
                                        ImGui::TreePop();
                                        if (apply) session->bindInputEvidence(consumer, port, session->registerInputEvidence(
                                            consumer, port, draftValue(artifact.kind, draft)));
                                    }
                                }
                            } catch (const compat::ThrowableException& error) {
                                const std::lock_guard lock(stateMonitor);
                                lastError = error.msg;
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                        ImGui::PopID();
                    }
                }
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    };
    renderArtifacts(task);
    if (task.attempt && ImGui::TreeNode(cStringCopy(translateText("Earlier attempts")).data())) {
        for (const auto& attempt : session->taskAttempts(task.taskId)) {
            if (attempt.attempt == task.attempt) continue;
            ImGui::PushID(static_cast<int>(attempt.attempt));
            if (ImGui::TreeNode("attempt", "#%u · %s", attempt.attempt,
                    cStringCopy(taskStatusLabel(attempt.state)).data())) {
                renderArtifacts(attempt);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

void AutoRigPanel::renderDefaultInputs(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId) {
    using namespace ::nijigenerate::autorig;
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
        } else if (port.id.view() == "plan") label = translateText("Correction targets and settings");
        else if (port.id.view() == "measurements") label = translateText("Body measurements");
        else if (port.id.view() == "shoulder-pairs") label = translateText("Shoulder and arm pairs");
        else if (port.id.view() == "ear-pairs") label = translateText("Front and back ear pairs");
        else if (port.id.view() == "hip-pairs") label = translateText("Waist and leg pairs");
        else if (port.id.view() == "state") label = translateText("Upstream processing result");
        else if (port.id.view() == "program") label = translateText("Rig plan");
        else if (port.id.view() == "model") label = translateText("Model checkpoint");
        else if (port.id.view() == "materials") label = translateText("Material classification");
        else label = port.id;
        const auto labelText = cStringCopy(label);
        ImGui::TextUnformatted(cStringData(labelText));
        if (!port.description.empty()) {
            const auto text = cStringCopy(translateText(port.description.retainedString()));
            ImGui::TextUnformatted(cStringData(text));
        }
        if (context->isConnected(port.id.retainedString())) {
            const auto session = run->session();
            const auto name = port.id.retainedString();
            const auto text = !session->boundInputEvidence(taskId.retainedString(), name).empty()
                ? translateText("Uses the selected past result")
                : context->hasValue(name) ? translateText("Uses the latest result")
                : translateText("Waiting for a previous step");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", cStringCopy(text).data());
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
                            const auto separator = targets.empty() ? std::string_view{} : std::string_view(changeArrow);
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
    case Kind::Object: return {};
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
    case Kind::Object: return ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Evidence object"));
    case Kind::Blob: return ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Binary data"));
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/autorig.d"), 254);
}

bool AutoRigPanel::renderStateTree(compat::RetainedStringSlice title,
        TaskState state, compat::RetainedStringSlice id, ImGuiTreeNodeFlags flags, bool reveal) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    ImGui::TextColored(taskStatusColor(state), "%s", taskStatusIcon(state));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", cStringCopy(taskStatusLabel(state)).data());
    ImGui::SameLine(0, 2.f);
    std::string label(title.data() ? title.data() : "", title.size());
    label += "###";
    label.append(id.data() ? id.data() : "", id.size());
    const auto labelText = cStringCopy(std::move(label));
    if (reveal) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
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
    case Kind::Object: break;
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
                    const bool captureChanged = ImGui::Checkbox(capture.data(), &compat::ngRequireObject(cached)->previews);
                    const auto ratio = deterministic::contracts::ngRigGet(cached->options,
                        compat::RetainedString::fromStaticLiteral(deterministic::upwardJawRidgeBlendOption),
                        deterministic::contracts::ngRigGet(cached->options,
                            compat::RetainedString::fromStaticLiteral(deterministic::jawRidgeBlendOption),
                            Json(deterministic::defaultUpwardJawRidgeBlend)));
                    float blend = static_cast<float>(deterministic::defaultUpwardJawRidgeBlend);
                    if (ratio.is_double() || ratio.is_int64() || ratio.is_uint64())
                        blend = static_cast<float>(deterministic::contracts::ngRigNumber(ratio));
                    const auto blendLabel = ngTranslateCString("Upward jaw contour correction blend");
                    const bool blendChanged = ImGui::SliderFloat(blendLabel.data(), &blend, 0.f, 1.f, "%.2f",
                        ImGuiSliderFlags_AlwaysClamp);
                    bool feminineChestDepth = deterministic::contracts::ngRigGet(cached->options,
                        compat::RetainedString::fromStaticLiteral(deterministic::feminineChestDepthOption),
                        Json(false)).as_bool();
                    const auto chestLabel = ngTranslateCString("Add feminine chest volume");
                    const bool chestChanged = ImGui::Checkbox(chestLabel.data(), &feminineChestDepth);
                    if (captureChanged || blendChanged || chestChanged) {
                        cached->options = compat::ngRequireObject(
                            compat::ngRequireObject(compat::ngRequireObject(run)->session())->inputContext(
                                run->stepTaskId(compat::RetainedString::fromStaticLiteral("compile"))))->value(
                                    compat::RetainedString::fromStaticLiteral("options")).json;
                        if (captureChanged) cached->options.sourceIndexAssignSlot("render") = Json(cached->previews);
                        if (blendChanged)
                            cached->options.sourceIndexAssignSlot(deterministic::upwardJawRidgeBlendOption) =
                                Json(static_cast<double>(blend));
                        if (chestChanged)
                            cached->options.sourceIndexAssignSlot(deterministic::feminineChestDepthOption) =
                                Json(feminineChestDepth);
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

void AutoRigPanel::renderActiveProgress() {
    if (!activeRun || !worker) return;
    const auto session = activeRun->session();
    const auto progress = session->executionProgress();
    const bool stopping = session->isCanceled();
    const auto state = stopping ? TaskState::Canceled : TaskState::Running;
    ImGui::TextColored(taskStatusColor(state), "%s", taskStatusIcon(state));
    ImGui::SameLine();
    if (stopping) {
        reviewLine("Stopping AutoRig");
    } else if (!progress.currentTask.empty()) {
        const auto spec = session->taskSpec(progress.currentTask);
        const auto task = spec.label.empty() ? progress.currentTask : translateText(spec.label.retainedString());
        const auto status = cStringCopy(translateText("Running: %s"));
        ImGui::TextWrapped(status.data(), cStringCopy(task).data());
    } else {
        reviewLine("AutoRig is running");
    }
    const auto count = cStringCopy(translateText("Tasks resolved in this run: %zu"));
    ImGui::TextDisabled(count.data(), progress.resolvedTasks);
}

bool AutoRigPanel::renderRun(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run) {
    namespace framework = ::nijigenerate::autorig::framework;
    using State = ::nijigenerate::autorig::workflow::AutoRigWorkflowState;
    using core::translation::ngTranslateRetained;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto id = cStringCopy(compat::RetainedStringSlice(compat::ngRequireObject(run)->id()));
    ImGui::PushID(id.data());
    bool result = false;
    compat::ngRunWithScopeExit([&] {
        result = [&] {
        const auto snapshot = run->snapshot(false);
        const auto label = ngTranslateRetained(run->displayName());
        const bool running = worker != nullptr && activeRun == run;
        enum RunView { Check, GlobalSettings, SelectedArea, Outputs };
        auto* ui = ImGui::GetStateStorage();
        const auto viewKey = ImGui::GetID("run-view");
        int view = ui->GetInt(viewKey, Check);
        bool remove = false;
        if (running) {
            ImGui::BeginDisabled(run->session()->isCanceled());
            if (ImGui::SmallButton(cStringCopy(translateText("Cancel")).data())) run->cancel();
            ImGui::EndDisabled();
        } else {
            if (ImGui::SmallButton("\ue5d3##run-actions")) ImGui::OpenPopup("run-actions"); // more_horiz
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", cStringCopy(translateText("Run options")).data());
            if (ImGui::BeginPopup("run-actions")) {
                const auto choose = [&](RunView destination, const framework::Text& label) {
                    if (ImGui::MenuItem(cStringCopy(label).data(), nullptr, view == destination))
                        view = destination;
                };
                choose(Check, translateText("Review this result"));
                choose(GlobalSettings, translateText("Workflow settings"));
                if (run->workflowId() == "model-to-rig") choose(SelectedArea, translateText("Rerun a specific area"));
                choose(Outputs, translateText("Workflow output"));
                ImGui::Separator();
                if (ImGui::MenuItem(cStringCopy(translateText("Run batch")).data(), nullptr, false, !worker))
                    startRun(run);
                remove = ImGui::MenuItem(cStringCopy(translateText("Delete session")).data(),
                    nullptr, false, !run->session()->isBusy());
                ImGui::EndPopup();
            }
        }
        ui->SetInt(viewKey, view);
        if (remove) return true;
        ImGui::SameLine();
        auto state = TaskState::Pending;
        switch (snapshot.state) {
        case State::Pending: break;
        case State::Running: state = TaskState::Running; break;
        case State::Succeeded: state = TaskState::Succeeded; break;
        case State::Failed: state = TaskState::Failed; break;
        case State::Canceled: state = TaskState::Canceled; break;
        case State::PartiallySucceeded: state = TaskState::PartiallySucceeded; break;
        }
        if (renderStateTree(label, state, compat::RetainedString::fromStaticLiteral("run"),
                ImGuiTreeNodeFlags_DefaultOpen)) {
            if (running || view == Check) {
                const auto tasks = workflowTasks(run);
                const auto steps = run->orderedSteps();
                const auto executingTask = running ? run->session()->executionProgress().currentTask : Text{};
                std::vector<ReviewStep> rows;
                rows.reserve(tasks.size());
                const bool builtIn = run->workflowId().view() == "model-to-rig";
                for (const auto& task : tasks) {
                    const auto step = std::ranges::find_if(steps, [&](const auto& item) {
                        return run->stepTaskId(item.id.retainedString()) == task;
                    });
                    const auto stepId = step == steps.end() ? compat::RetainedStringSlice{} : step->id;
                    const auto spec = run->session()->taskSpec(task);
                    rows.push_back({task, stepId, spec.label.empty() ? compat::RetainedStringSlice(task) :
                        compat::RetainedStringSlice(translateText(spec.label.retainedString())),
                        task == executingTask ? TaskState::Running : run->session()->task(task, false).state});
                }

                std::vector<std::size_t> checks;
                std::vector<std::size_t> issues;
                std::optional<std::size_t> current;
                for (std::size_t index = 0; index < rows.size(); ++index) {
                    const auto& row = rows[index];
                    if (row.state == TaskState::Running) current = index;
                    if (needsAttention(row.state)) issues.push_back(index);
                }
                if (builtIn) {
                    for (const auto key : {"compile", "bake", "controls", "verify"}) {
                        for (std::size_t index = 0; index < rows.size(); ++index)
                            if (rows[index].step.view() == key && rows[index].state != TaskState::Pending)
                                checks.push_back(index);
                    }
                } else {
                    for (std::size_t index = 0; index < rows.size(); ++index)
                        if (rows[index].state != TaskState::Pending) checks.push_back(index);
                }
                const auto suggested = !issues.empty() ? std::optional(issues.front()) :
                    !checks.empty() ? std::optional(checks.front()) : std::nullopt;
                {
                    const auto tab = reviewTaskByRun.find(run->id());
                    if (tab != reviewTaskByRun.end()) {
                        for (std::size_t index = 0; index < rows.size(); ++index)
                            if (rows[index].task == tab->second) current = index;
                    }
                    if (!current && !running) current = suggested;
                }
                const auto handoffs = workflowHandoffs(run, rows);
                std::optional<std::size_t> handoff;
                const auto runId = run->id();
                if (const auto picked = reviewHandoffByRun.find(runId);
                        picked != reviewHandoffByRun.end() && picked->second < handoffs.size())
                    handoff = picked->second;
                const auto selectTask = [&](std::size_t index) {
                    current = index;
                    reviewTaskByRun[runId] = rows[*current].task;
                    reviewHandoffByRun.erase(runId);
                    handoff.reset();
                };
                const auto completed = std::ranges::count_if(rows, [](const auto& row) {
                    return row.state == TaskState::Succeeded;
                });
                const auto summary = cStringCopy(translateText("%zu of %zu steps completed · %zu need attention"));
                ImGui::TextWrapped(summary.data(), static_cast<std::size_t>(completed), rows.size(), issues.size());
                if (!running && snapshot.state == State::Pending) {
                    ImGui::BeginDisabled(worker != nullptr);
                    if (ImGui::Button(cStringCopy(translateText("Start AutoRig")).data())) startRun(run);
                    ImGui::EndDisabled();
                } else if (!running) {
                    const auto& queue = issues.empty() ? checks : issues;
                    if (!queue.empty()) {
                        const auto at = current ? std::ranges::find(queue, *current) : queue.end();
                        const auto next = at == queue.end() || std::next(at) == queue.end() ? queue.front() : *std::next(at);
                        const auto action = issues.empty() ? translateText("Next check") : translateText("Review issue");
                        if (ImGui::SmallButton(cStringCopy(action).data())) selectTask(next);
                    }
                    if (!queue.empty()) ImGui::SameLine();
                    if (ImGui::SmallButton(cStringCopy(translateText("Workflow settings")).data()))
                        ui->SetInt(viewKey, GlobalSettings);
                }
                if (const auto choice = renderWorkflowFlow(rows, handoffs, current, handoff)) selectTask(*choice);

                ImGui::Spacing();
                if (current) {
                    const auto selected = *current;
                    const auto& row = rows[*current];
                    const auto renderInputs = [&] {
                        const auto inputs = std::ranges::count_if(handoffs, [&](const auto& edge) {
                            return edge.to == selected && !edge.ports.empty();
                        });
                        if (inputs) reviewLine("Inputs from other steps");
                        const auto missing = missingRequiredInput(run, row.task);
                        for (std::size_t i = 0; i < handoffs.size(); ++i) {
                            const auto& edge = handoffs[i];
                            if (edge.to != selected || edge.ports.empty()) continue;
                            const auto& source = rows[edge.from];
                            const bool openByDefault = inputs == 1 || (!missing.empty() &&
                                std::ranges::any_of(edge.ports, [&](const auto& port) { return port.input == missing; }));
                            ImGui::PushID(static_cast<int>(i));
                            const bool open = ImGui::TreeNodeEx("source", openByDefault ?
                                ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", cStringCopy(source.label).data());
                            if (ImGui::IsItemToggledOpen()) {
                                if (open) reviewHandoffByRun[runId] = i;
                                else if (handoff == i) reviewHandoffByRun.erase(runId);
                            }
                            if (open) {
                                renderHandoff(run, source.task, row.task, edge.ports);
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                    };
                    const auto renderNext = [&] {
                        bool hasNext = false;
                        for (const auto& edge : handoffs) {
                            if (edge.from != selected) continue;
                            hasNext = true;
                            const auto& next = rows[edge.to];
                            const auto missing = missingRequiredInput(run, next.task);
                            const bool finished = next.state == TaskState::Succeeded || next.state == TaskState::NotApplicable;
                            ImGui::PushID(cStringCopy(next.task).data());
                            ImGui::TextWrapped("%s", cStringCopy(next.label).data());
                            if (finished) {
                                const auto status = taskStatusLabel(next.state);
                                ImGui::TextUnformatted(status.data(), status.data() + status.size());
                            } else if (!missing.empty()) {
                                const auto format = cStringCopy(translateText("Waiting for %s"));
                                ImGui::TextWrapped(format.data(), cStringCopy(handoffLabel(missing)).data());
                            } else {
                                ImGui::BeginDisabled(worker != nullptr);
                                if (ImGui::Button(cStringCopy(translateText("Run this step")).data())) {
                                    selectTask(edge.to);
                                    startRun(run, next.step, false, {}, next.task);
                                }
                                ImGui::EndDisabled();
                            }
                            if (finished || !missing.empty() || worker == nullptr) ImGui::SameLine();
                            if (ImGui::SmallButton(cStringCopy(translateText("Open step")).data()))
                                selectTask(edge.to);
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                        if (!hasNext) reviewLine("No dependent steps");
                    };
                    renderTask(run, row.task, row.step, row.state == TaskState::Running, renderInputs, renderNext);
                } else if (running) {
                    reviewLine("Running selected steps");
                }

            } else {
                if (ImGui::SmallButton(cStringCopy(translateText("Back to the check")).data()))
                    ui->SetInt(viewKey, Check);
                if (view == GlobalSettings) renderSessionConfiguration(run);
                if (view == Outputs && renderSessionOutput(run)) ui->SetInt(viewKey, Check);
            }
            if (!running && view == SelectedArea && run->workflowId() == "model-to-rig") {
                const auto steps = run->orderedSteps();
                ImGui::BeginDisabled(worker != nullptr);
                ImGui::InputText(cStringCopy(translateText("AutoRig tags")).data(), scopeTags.data(), scopeTags.size());
                ImGui::SameLine();
                if (ImGui::Button(cStringCopy(translateText("Rerun selected tags")).data())) {
                    const auto puppet = project::incActivePuppet();
                    if (puppet) {
                        framework::AutoRigTargetSelector selector;
                        std::string_view rest(scopeTags.data());
                        while (!rest.empty()) {
                            const auto end = rest.find_first_of(", ");
                            const auto token = rest.substr(0, end);
                            if (!token.empty()) selector.tags.emplace_back(token);
                            if (end == rest.npos) break;
                            rest.remove_prefix(end + 1);
                        }
                        try {
                            auto plan = run->plan(std::move(selector), framework::Text(
                                std::to_string(puppet->root->getUUID())));
                            startRun(run, {}, true, std::move(plan));
                        } catch (const compat::ThrowableException& error) {
                            const std::lock_guard lock(stateMonitor);
                            lastError = error.msg;
                        }
                    }
                }
                ImGui::EndDisabled();
                const std::lock_guard lock(stateMonitor);
                if (scopedResultRun == run->id() && ImGui::TreeNode(cStringCopy(translateText("Tagged run results")).data())) {
                    for (const auto& task : scopedResult.tasks) {
                        const auto spec = run->session()->taskSpec(task.taskId);
                        const auto name = translateText(spec.label.retainedString());
                        ImGui::PushID(cStringCopy(task.taskId).data());
                        if (renderStateTree(name, task.state, compat::RetainedString::fromStaticLiteral("scoped-task"), 0)) {
                            evidenceText(taskStatusLabel(task.state));
                            const auto steps = run->orderedSteps();
                            const auto step = std::ranges::find_if(steps, [&](const auto& item) {
                                return run->stepTaskId(item.id.retainedString()) == task.taskId;
                            });
                            reviewGuidance(step == steps.end() ? std::string_view{} : step->id.view(), spec.phase);
                            renderTaskEvidence(run, task);
                            if (ImGui::TreeNode(cStringCopy(translateText("Advanced evidence")).data())) {
                                renderTaskEvidence(run, task, true);
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        return false;
        }();
    }, [] { ImGui::PopID(); });
    return result;
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
        run->session()->setInput(run->stepTaskId(literal("exclusions").retainedString()),
            literal("options").retainedString(), framework::AutoRigValue::jsonValue(options));
        context->setValue(literal("options").retainedString(), framework::AutoRigValue::jsonValue(options));
        if (rerun) startRun(run, {}, false);
    } catch (const compat::ThrowableException& error) {
        const std::lock_guard lock(stateMonitor);
        lastError = error.msg;
    }
}

bool AutoRigPanel::renderSessionOutput(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run) {
    using Kind = ::nijigenerate::autorig::framework::AutoRigValueKind;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto title = core::translation::ngTranslateCString("Workflow output");
    if (!ImGui::TreeNodeEx(title.data(), ImGuiTreeNodeFlags_DefaultOpen)) return false;
    bool openVerification = false;
    compat::ngRunWithScopeExit([&] {
        if (run->renderOutputUI()) return;
        const bool builtIn = run->workflowId() == "model-to-rig";
        if (builtIn && ImGui::SmallButton(cStringCopy(translateText("Open final verification")).data())) {
            reviewTaskByRun[run->id()] = run->stepTaskId("verify");
            reviewHandoffByRun.erase(run->id());
            openVerification = true;
        }
        for (const auto& port : run->outputPorts()) {
            ImGui::PushID(cStringCopy(port.id).data());
            const auto name = port.id.view() == "model" ? translateText("Rigged model") :
                port.id.view() == "report" ? translateText("Verification report") : port.id.retainedString();
            const auto id = port.id.retainedString();
            if (!run->hasOutput(id)) {
                ImGui::TextDisabled("%s · %s", cStringCopy(name).data(),
                    cStringCopy(translateText("Waiting for output")).data());
            } else {
                ImGui::TextWrapped("%s · %s", cStringCopy(name).data(),
                    cStringCopy(translateText("Processing complete")).data());
                if (port.kind != Kind::Json && port.kind != Kind::Blob)
                    evidenceText(run->outputArtifact(id).value().text);
            }
            ImGui::PopID();
        }
    }, [] { ImGui::TreePop(); });
    return openVerification;
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
