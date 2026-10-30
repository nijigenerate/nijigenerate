#pragma once

#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/autorig/workflow.hpp>
#include <nijigenerate/autorig/deterministic/presentation.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <memory>
#include <mutex>

namespace nijigenerate::panels::autorig {
namespace detail { class AutoRigWorker; }

// Public source class frame. Its separately owned state/method implementations
// are not replaced with empty native defaults or a successful worker stub.
class AutoRigPanel : public Panel, public std::enable_shared_from_this<AutoRigPanel> {
    friend void ngAutoRigPollWorker();
    template<class> friend struct nicxlive::core::d_runtime::SourceTypeSpellings;
    // Native backing for this instance's source synchronized(this) monitor.
    std::recursive_mutex stateMonitor;
    struct DraftKeyHash {
        std::size_t operator()(const compat::RetainedStringSlice& key) const noexcept {
            return nicxlive::core::d_runtime::ngPhobosStringHash(key.view());
        }
    };
    struct DraftKeyEqual {
        bool operator()(const compat::RetainedStringSlice& left,
                const compat::RetainedStringSlice& right) const noexcept {
            return left.view() == right.view();
        }
    };
    template<class Value> using DraftMap = nicxlive::core::common::ResourceKeyMap<
        compat::RetainedStringSlice, Value, DraftKeyHash, DraftKeyEqual>;
    using TextDraftMap = DraftMap<compat::RetainedStringSlice>;
    std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowManager> workflows;
    compat::Slice<std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun>> runs;
    compat::RetainedStringSlice selectedProviderId;
    compat::RetainedStringSlice selectedWorkflowId;
    TextDraftMap inputDrafts;
    TextDraftMap inputObserved;
    struct InputTextCache {
        std::uint64_t revision = 0;
        compat::RetainedStringSlice text;
    };
    DraftMap<InputTextCache> inputTextCache;
    struct ContextDraft {
        compat::RetainedStringSlice name;
        compat::RetainedStringSlice value;
        ::nijigenerate::autorig::framework::AutoRigValueKind kind =
            ::nijigenerate::autorig::framework::AutoRigValueKind::Path;
    };
    DraftMap<ContextDraft> contextDrafts;
    std::shared_ptr<detail::AutoRigWorker> worker;
    bool reclaimSessionMemory = false;
    std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> activeRun;
    std::uint32_t controlledRoot = 0;
    bool previousDriversEnabled = false;
    compat::RetainedStringSlice lastError;
    struct MaterialDraft {
        std::uint32_t attempt = 0, compiledAttempt = 0;
        std::uint64_t revision = 0;
        compat::Slice<compat::RetainedStringSlice> names, paths, roles, features, sides, reasons, semanticSources;
        compat::Slice<bool> disabled, changed, reset, active, forcedStatic, requestedStatic;
        compat::Slice<std::uint8_t> editedFields;
        bool unresolvedOnly = false;
        compat::RetainedStringSlice filter;
        compat::Slice<compat::RetainedStringSlice> landmarks, landmarkReasons;
        compat::Slice<double> landmarkX, landmarkY;
        compat::Slice<bool> landmarkChanged, landmarkReset;
    };
    DraftMap<MaterialDraft> materialDrafts;
    compat::Slice<compat::RetainedStringSlice> materialRoleChoices;
    struct ReviewDraft {
        std::uint32_t attempt = 0;
        compat::Slice<::nijigenerate::autorig::deterministic::presentation::RigReviewTable> tables;
        compat::Slice<compat::RetainedStringSlice> operationIds, operationLabels;
        compat::Slice<bool> enabled;
    };
    DraftMap<ReviewDraft> reviewDrafts;
    struct WorkflowOptionsDraft {
        std::uint64_t revision = 0;
        ::nijigenerate::autorig::framework::Json options{};
        bool previews = false;
    };
    DraftMap<WorkflowOptionsDraft> workflowOptionsDrafts;
    std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowManager> workflowManager();
    void finishWorker();
    void restoreDriverState();
    void startRun(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice stepId = {}, bool force = true);
    compat::RetainedStringSlice presetLabel(::nijigenerate::autorig::workflow::AutoRigWorkflowPreset preset);
    void removeRun(compat::RetainedStringSlice runId);
    compat::RetainedStringSlice draftKey(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId, compat::RetainedStringSlice portId);
    compat::RetainedStringSlice inputText(::nijigenerate::autorig::framework::AutoRigValue value);
    compat::RetainedStringSlice cachedInputText(compat::RetainedStringSlice key, std::uint64_t revision,
        compat::SharedDelegate<compat::RetainedStringSlice()> load);
    compat::RetainedStringSlice valueKindLabel(::nijigenerate::autorig::framework::AutoRigValueKind kind);
    ::nijigenerate::autorig::framework::AutoRigValue draftValue(
        ::nijigenerate::autorig::framework::AutoRigValueKind kind, compat::RetainedStringSlice draft);
    void applyInput(std::shared_ptr<::nijigenerate::autorig::framework::IAutoRigInputContext> context,
        ::nijigenerate::autorig::framework::AutoRigPortSpec port, compat::RetainedStringSlice draft);
    bool renderStateTree(compat::RetainedStringSlice title, compat::RetainedStringSlice state,
        compat::RetainedStringSlice id, ImGuiTreeNodeFlags flags);
    void renderPresetPicker();
    bool renderRun(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run);
    void renderSessionConfiguration(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run);
    void renderSessionValue(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice name, ::nijigenerate::autorig::framework::AutoRigValueKind kind,
        compat::RetainedStringSlice current, bool contextValue);
    void renderSessionOutput(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run);
    void renderDefaultInputs(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId);
    void renderDefaultOutputs(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId);
    void renderReviewTables(
        compat::Slice<::nijigenerate::autorig::deterministic::presentation::RigReviewTable>& tables);
    bool renderRigOutput(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId);
    void renderMaterialRoles(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice taskId);
    void renderTask(std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        ::nijigenerate::autorig::framework::AutoRigWorkflowStep step);
protected:
    void prepareSourceClassInfo() override;
    void onInit() override;
    void onUpdate() override;
public:
    AutoRigPanel();
    ~AutoRigPanel() override = default;
    AutoRigPanel(const AutoRigPanel&) = delete;
    AutoRigPanel& operator=(const AutoRigPanel&) = delete;
    AutoRigPanel(AutoRigPanel&&) = delete;
    AutoRigPanel& operator=(AutoRigPanel&&) = delete;

    virtual compat::RetainedStringSlice resumeWorkflow(compat::RetainedStringSlice runId,
        compat::RetainedStringSlice stepId);
    virtual compat::RetainedStringSlice executeImportedModel(
        ::nijigenerate::autorig::framework::Json options,
        ::nijigenerate::autorig::framework::Json context);
    virtual ::nijigenerate::autorig::framework::Json runStatus(compat::RetainedStringSlice runId);
    virtual void stop();
};

std::shared_ptr<::nijigenerate::autorig::framework::AutoRigSessionManager> ngAutoRigSessionManager();
void ngRegisterAutoRigProcessor(std::shared_ptr<::nijigenerate::autorig::framework::AutoRigProcessor> processor);
void ngAutoRigStopAll();
void ngAutoRigPollWorker();
// Explicit native entry for the original module's incPanel!AutoRigPanel mixin.
// The complete import catalogue calls it before application/thread entry.
void ngInitializeAutoRigPanelModule();
} // namespace nijigenerate::panels::autorig

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::panels::autorig::AutoRigPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.autorig.AutoRigPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.autorig.AutoRigPanel)";
};
#define NG_AUTORIG_PANEL_TYPE_ORIGIN(Type, Qualified, Unqualified, Object) \
template<> struct SourceTypeSpellings<::nijigenerate::panels::autorig::Type> { \
    inline static constexpr bool isNativeObject = Object; \
    inline static constexpr char qualifiedArgument[] = "(" Qualified ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" Unqualified ")"; \
    inline static constexpr char qualifiedElement[] = Qualified; \
    inline static constexpr char unqualifiedElement[] = Unqualified; \
};
NG_AUTORIG_PANEL_TYPE_ORIGIN(AutoRigPanel, "nijigenerate.panels.autorig.AutoRigPanel", "AutoRigPanel", true)
NG_AUTORIG_PANEL_TYPE_ORIGIN(AutoRigPanel::InputTextCache,
    "nijigenerate.panels.autorig.AutoRigPanel.InputTextCache", "InputTextCache", false)
NG_AUTORIG_PANEL_TYPE_ORIGIN(AutoRigPanel::ContextDraft,
    "nijigenerate.panels.autorig.AutoRigPanel.ContextDraft", "ContextDraft", false)
NG_AUTORIG_PANEL_TYPE_ORIGIN(AutoRigPanel::MaterialDraft,
    "nijigenerate.panels.autorig.AutoRigPanel.MaterialDraft", "MaterialDraft", false)
NG_AUTORIG_PANEL_TYPE_ORIGIN(AutoRigPanel::ReviewDraft,
    "nijigenerate.panels.autorig.AutoRigPanel.ReviewDraft", "ReviewDraft", false)
NG_AUTORIG_PANEL_TYPE_ORIGIN(AutoRigPanel::WorkflowOptionsDraft,
    "nijigenerate.panels.autorig.AutoRigPanel.WorkflowOptionsDraft", "WorkflowOptionsDraft", false)
#undef NG_AUTORIG_PANEL_TYPE_ORIGIN
} // namespace nicxlive::core::d_runtime

